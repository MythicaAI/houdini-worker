#include "automation.h"
#include "session.h"
#include "interrupt.h"
#include "Remotery.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"

#include <OP/OP_Director.h>
#include <OP/OP_OTLLibrary.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_IOTranslator.h>
#include <GU/GU_Detail.h>
#include <ROP/ROP_Node.h>
#include <SOP/SOP_Node.h>
#include <MOT/MOT_Director.h>
#include <PY/PY_Python.h>
#include <UT/UT_Error.h>
#include <UT/UT_Ramp.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

constexpr const int COOK_TIMEOUT_SECONDS = 60;
constexpr const char* SOP_NODE_TYPE = "sop";

namespace util
{

static bool can_incremental_cook(const CookRequest& previous, const CookRequest& current)
{
    if (previous.hda_file != current.hda_file)
    {
        return false;
    }

    if (previous.definition_index != current.definition_index)
    {
        return false;
    }

    if (previous.inputs != current.inputs)
    {
        return false;
    }

    if (previous.parameters.size() != current.parameters.size())
    {
        return false;
    }

    auto same_key = [](const auto& entryA, const auto& entryB) { return entryA.first == entryB.first; };
    return std::equal(previous.parameters.begin(), previous.parameters.end(), current.parameters.begin(), same_key);
}

static std::string install_library(MOT_Director* director, const std::string& hda_file_path, int definition_index, StreamWriter& writer)
{
    // Load the library
    OP_OTLManager& manager = director->getOTLManager();

    int library_index = manager.findLibrary(hda_file_path.c_str());
    if (library_index < 0)
    {
        manager.installLibrary(hda_file_path.c_str());

        library_index = manager.findLibrary(hda_file_path.c_str());
        if (library_index < 0)
        {
            writer.error("Failed to install library: " + hda_file_path);
            return "";
        }
    }

    // Get the actual library from the index
    OP_OTLLibrary* library = manager.getLibrary(library_index);
    if (!library)
    {
        writer.error("Failed to get library at index " + std::to_string(library_index));
        return "";
    }

    int num_definitions = library->getNumDefinitions();
    if (definition_index >= num_definitions)
    {
        writer.error("Definition index out of range: " + std::to_string(definition_index));
        return "";
    }

    const OP_OTLDefinition& definition = library->getDefinition(definition_index);
    std::string node_type = definition.getName().toStdString();

    size_t first = node_type.find("::");
    if (first != std::string::npos)
    {
        size_t last = node_type.find("::", first + 2);

        if (last != std::string::npos)
        {
            node_type = node_type.substr(first + 2, last - (first + 2));
        }
        else
        {
            node_type = node_type.substr(first + 2);
        }
    }

    return node_type;
}

static OP_Node* create_node(MOT_Director* director, const std::string& node_type, StreamWriter& writer)
{
    // Find the root /obj network
    OP_Network* obj = (OP_Network*)director->findNode("/obj");
    if (!obj)
    {
        writer.error("Failed to find obj network");
        return nullptr;
    }
    assert(obj->getNchildren() == 0 || obj->getNchildren() == 1);

    // Create geo node
    OP_Network* geo = (OP_Network*)obj->findNode("geo");
    if (!geo)
    {
        geo = (OP_Network*)obj->createNode("geo", "geo");
        if (!geo || !geo->runCreateScript())
        {
            writer.error("Failed to create geo node");
            return nullptr;
        }
    }
    assert(geo->getNchildren() == 0);

    // Create the SOP node
    OP_Node* node = geo->createNode(node_type.c_str(), SOP_NODE_TYPE);
    if (!node || !node->runCreateScript())
    {
        writer.error("Failed to create node of type: " + node_type);
        return nullptr;
    }

    return node;
}

static OP_Node* find_node(MOT_Director* director)
{
    // Find the root /obj network
    OP_Network* obj = (OP_Network*)director->findNode("/obj");
    if (!obj)
    {
        return nullptr;
    }
    assert(obj->getNchildren() == 0 || obj->getNchildren() == 1);

    // Find existing geo node
    OP_Network* geo = (OP_Network*)obj->findNode("geo");
    if (!geo)
    {
        return nullptr;
    }

    // Find the existing SOP node
    OP_Node* node = geo->findNode(SOP_NODE_TYPE);
    if (!node)
    {
        return nullptr;
    }

    return node;
}

static OP_Node* create_input_node(OP_Network* parent, const std::string& path, StreamWriter& writer)
{
    if (!std::filesystem::exists(path))
    {
        return nullptr;
    }

    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".usd" || ext == ".usdz")
    {
        OP_Node* input_node = parent->createNode("usdimport");
        if (!input_node || !input_node->runCreateScript())
        {
            writer.error("Failed to create usdimport node for " + path);
            return nullptr;
        }

        input_node->setString(path.c_str(), CH_STRING_LITERAL, "filepath1", 0, 0.0f);
        input_node->setInt("input_unpack", 0, 0.0f, 1);
        input_node->setInt("unpack_geomtype", 0, 0.0f, 1);
        return input_node;
    }
    else if (ext == ".obj")
    {
        OP_Node* input_node = parent->createNode("obj_importer");
        if (!input_node || !input_node->runCreateScript())
        {
            writer.error("Failed to create obj_importer node for " + path);
            return nullptr;
        }

        input_node->setString(path.c_str(), CH_STRING_LITERAL, "sObjFile", 0, 0.0f);
        return input_node;
    }
    else if (ext == ".fbx")
    {
        OP_Node* input_node = parent->createNode("fbx_archive_import");
        if (!input_node || !input_node->runCreateScript())
        {
            writer.error("Failed to create fbx_archive_import node for " + path);
            return nullptr;
        }

        input_node->setString(path.c_str(), CH_STRING_LITERAL, "sFBXFile", 0, 0.0f);
        input_node->setInt("bConvertUnits", 0, 0.0f, 1);

        // Reload the node to apply the changes
        UT_String h_path;
        input_node->getFullPath(h_path);

        std::string py =
            "import hou\n"
            "n = hou.node('" + std::string(h_path) + "')\n"
            "n.parm('reload').pressButton()\n";

        bool success = PYrunPythonStatementsAndExpectNoErrors(py.c_str(), "FBX reload");
        if (!success)
        {
            writer.error("Failed to reload fbx_archive_import node for " + path);
            return nullptr;
        }

        return input_node;
    }
    else if (ext == ".gltf" || ext == ".glb")
    {
        OP_Node* input_node = parent->createNode("gltf");
        if (!input_node || !input_node->runCreateScript())
        {
            writer.error("Failed to create gltf node for " + path);
            return nullptr;
        }

        input_node->setString(path.c_str(), CH_STRING_LITERAL, "filename", 0, 0.0f);
        return input_node;
    }

    return nullptr;
}

static void set_inputs(OP_Node* node, const std::map<int, FileParameter>& inputs, StreamWriter& writer)
{
    OP_Network* parent = node->getParent();

    for (const auto& [index, file] : inputs)
    {
        OP_Node* input_node = create_input_node(parent, file.file_path, writer);
        if (!input_node)
        {
            input_node = parent->createNode("null");
            if (!input_node || !input_node->runCreateScript())
            {
                writer.error("Failed to create null node for " + file.file_path);
                continue;
            }
        }
        writer.status("Adding input " + file.file_path + " to node " + node->getName().c_str() + " at index " + std::to_string(index));
        node->setInput(index, input_node);
    }
}

static void set_parameters(OP_Node* node, const ParameterSet& parameters, StreamWriter& writer)
{
    for (const auto& [key, value] : parameters)
    {
        if (std::holds_alternative<int64_t>(value))
        {
            node->setInt(key.c_str(), 0, 0.0f, std::get<int64_t>(value));
        }
        else if (std::holds_alternative<double>(value))
        {
            node->setFloat(key.c_str(), 0, 0.0f, std::get<double>(value));
        }
        else if (std::holds_alternative<std::string>(value))
        {
            node->setString(std::get<std::string>(value).c_str(), CH_STRING_LITERAL, key.c_str(), 0, 0.0f);
        }
        else if (std::holds_alternative<bool>(value))
        {
            node->setInt(key.c_str(), 0, 0.0f, std::get<bool>(value) ? 1 : 0);
        }
        else if (std::holds_alternative<std::vector<int64_t>>(value))
        {
            const auto& int_array = std::get<std::vector<int64_t>>(value);
            for (size_t i = 0; i < int_array.size(); ++i)
            {
                node->setInt(key.c_str(), i, 0.0f, int_array[i]);
            }
        }
        else if (std::holds_alternative<std::vector<double>>(value))
        {
            const auto& float_array = std::get<std::vector<double>>(value);
            for (size_t i = 0; i < float_array.size(); ++i)
            {
                node->setFloat(key.c_str(), i, 0.0f, float_array[i]);
            }
        }
        else if (std::holds_alternative<std::vector<RampPoint>>(value))
        {
            const auto& ramp_points = std::get<std::vector<RampPoint>>(value);

            UT_Ramp ramp;
            for (const auto& point : ramp_points)
            {
                float values[4] = { point.value[0], point.value[1], point.value[2], point.value[3] };
                ramp.addNode(point.pos, values, point.interp);
            }

            PRM_Parm* rampParm = node->getParmPtr(key.c_str());
            if (rampParm)
            {
                node->updateMultiParmFromRamp(0.0, ramp, *rampParm, false, PRM_AK_SET_KEY);
            }
        }
        else if (std::holds_alternative<FileParameter>(value))
        {
            const auto& file = std::get<FileParameter>(value);
            if (!file.file_path.empty())
            {
                node->setString(file.file_path.c_str(), CH_STRING_LITERAL, key.c_str(), 0, 0.0f);
            }
        }
        else
        {
            writer.error("Failed to set parameter: " + key);
        }
    }
}

bool export_geometry_raw(const GU_Detail* gdp, Geometry& geom, StreamWriter& writer)
{
    rmt_ScopedCPUSample(ExportGeometryRaw, 0);

    GA_ROHandleV3 P_handle(gdp, GA_ATTRIB_POINT, "P");
    if (!P_handle.isValid())
    {
        writer.error("Geometry missing point attribute");
        return false;
    }

    GA_ROHandleV3 N_P_handle(gdp, GA_ATTRIB_POINT, "N");
    GA_ROHandleV3 N_V_handle(gdp, GA_ATTRIB_VERTEX, "N");

    GA_ROHandleV3 UV_P_handle(gdp, GA_ATTRIB_POINT, "uv");
    GA_ROHandleV3 UV_V_handle(gdp, GA_ATTRIB_VERTEX, "uv");

    GA_ROHandleV3 Cd_P_handle(gdp, GA_ATTRIB_POINT, "Cd");
    GA_ROHandleV3 Cd_V_handle(gdp, GA_ATTRIB_VERTEX, "Cd");
    GA_ROHandleV3 Cd_PR_handle(gdp, GA_ATTRIB_PRIMITIVE, "Cd");

    const GEO_Primitive* prim;
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMPOLY)
        {
           continue;
        }

        GA_Size num_verts = prim->getVertexCount();
        if (num_verts < 3)
        {
            continue;
        }

        int base_index = geom.points.size() / 3;
        assert(geom.points.size() % 3 == 0);

        GA_Offset primOff = prim->getMapOffset();

        for (GA_Size i = 0; i < num_verts; i++)
        {
            GA_Offset ptOff = prim->getPointOffset(i);
            GA_Offset vtxOff = prim->getVertexOffset(i);

            // Position
            UT_Vector3 pos = P_handle.get(ptOff);
            geom.points.push_back(pos.x());
            geom.points.push_back(pos.y());
            geom.points.push_back(pos.z());

            // Normal
            if (N_P_handle.isValid())
            {
                UT_Vector3 norm = N_P_handle.get(ptOff);
                geom.normals.push_back(norm.x());
                geom.normals.push_back(norm.y());
                geom.normals.push_back(norm.z());
            }
            else if (N_V_handle.isValid())
            {
                UT_Vector3 norm = N_V_handle.get(vtxOff);
                geom.normals.push_back(norm.x());
                geom.normals.push_back(norm.y());
                geom.normals.push_back(norm.z());
            }

            // UV
            if (UV_P_handle.isValid())
            {
                UT_Vector3 uv = UV_P_handle.get(ptOff);
                geom.uvs.push_back(uv.x());
                geom.uvs.push_back(uv.y());
            }
            else if (UV_V_handle.isValid())
            {
                UT_Vector3 uv = UV_V_handle.get(vtxOff);
                geom.uvs.push_back(uv.x());
                geom.uvs.push_back(uv.y());
            }

            // Color
            if (Cd_P_handle.isValid())
            {
                UT_Vector3 color = Cd_P_handle.get(ptOff);
                geom.colors.push_back(color.x());
                geom.colors.push_back(color.y());
                geom.colors.push_back(color.z());
            }
            else if (Cd_V_handle.isValid())
            {
                UT_Vector3 color = Cd_V_handle.get(vtxOff);
                geom.colors.push_back(color.x());
                geom.colors.push_back(color.y());
                geom.colors.push_back(color.z());
            }
            else if (Cd_PR_handle.isValid())
            {
                UT_Vector3 color = Cd_PR_handle.get(primOff);
                geom.colors.push_back(color.x());
                geom.colors.push_back(color.y());
                geom.colors.push_back(color.z());
            }
        }

        // Triangulate as a fan
        int num_tris = num_verts - 2;
        for (int i = 0; i < num_tris; i++)
        {
            geom.indices.push_back(base_index + 0);
            geom.indices.push_back(base_index + i + 1);
            geom.indices.push_back(base_index + i + 2);
        }
    }

    if (geom.points.size() == 0)
    {
        writer.error("Geometry contains no primitives");
        return false;
    }

    return true;
}

bool export_geometry_obj(const GU_Detail* gdp, std::vector<char>& file_data, StreamWriter& writer)
{
    rmt_ScopedCPUSample(ExportGeometryOBJ, 0);

    std::unique_ptr<GEO_IOTranslator> xlate(GU_Detail::getSupportedFormat(".obj"));
    if (!xlate)
    {
        writer.error("OBJ export not supported");
        return false;
    }

    std::stringstream buffer;
    GA_Detail::IOStatus status = xlate->fileSave(gdp, buffer);
    if (!status.success())
    {
        writer.error("Failed to export OBJ to buffer");
        return false;
    }

    std::string str = buffer.str();
    file_data.assign(str.begin(), str.end());
    if (file_data.size() == 0)
    {
        writer.error("Empty OBJ file");
        return false;
    }

    return true;
}

bool export_geometry_with_format(MOT_Director* director, SOP_Node* sop, EOutputFormat format, std::vector<char>& file_data, StreamWriter& writer)
{
    rmt_ScopedCPUSample(ExportGeometryFormat, 0);

    // Find the root /out network
    OP_Network* rop = (OP_Network*)director->findNode("/out");
    if (!rop)
    {
        writer.error("Failed to find rop network");
        return false;
    }

    // Setup the export node
    UT_String sop_path;
    sop->getFullPath(sop_path);

    std::string out_path = "/tmp/export_" + std::to_string(rand());

    ROP_Node* export_node = nullptr;
    if (format == EOutputFormat::FBX)
    {
        export_node = (ROP_Node*)rop->findNode("fbx_export");
        if (!export_node)
        {
            export_node = (ROP_Node*)rop->createNode("filmboxfbx", "fbx_export");

            if (!export_node || !export_node->runCreateScript())
            {
                writer.error("Failed to create fbx export node");
                return false;
            }
        }

        out_path += ".fbx";

        export_node->setString(sop_path.c_str(), CH_STRING_LITERAL, "startnode", 0, 0.0f);
        export_node->setString(out_path.c_str(), CH_STRING_LITERAL, "sopoutput", 0, 0.0f);
        export_node->setInt("exportkind", 0, 0.0f, 0);

    }
    else if (format == EOutputFormat::GLB)
    {
        export_node = (ROP_Node*)rop->findNode("glb_export");
        if (!export_node)
        {
            export_node = (ROP_Node*)rop->createNode("gltf", "glb_export");

            if (!export_node || !export_node->runCreateScript())
            {
                writer.error("Failed to create glb export node");
                return false;
            }
        }

        out_path += ".glb";

        export_node->setString(sop_path.c_str(), CH_STRING_LITERAL, "soppath", 0, 0.0f);
        export_node->setString(out_path.c_str(), CH_STRING_LITERAL, "file", 0, 0.0f);
        export_node->setInt("usesoppath", 0, 0.0f, 1);
    }
    else if (format == EOutputFormat::USD)
    {
        OP_Network* stage = (OP_Network*)director->findNode("/stage");
        if (!stage)
        {
            writer.error("Failed to find stage network");
            return false;
        }

        OP_Node* sop_import = (OP_Node*)stage->findNode("sop_import");
        if (!sop_import)
        {
            sop_import = (OP_Node*)stage->createNode("sopimport", "sop_import");
            if (!sop_import || !sop_import->runCreateScript())
            {
                writer.error("Failed to create usd sop import node");
                return false;
            }
        }

        sop_import->setString(sop_path.c_str(), CH_STRING_LITERAL, "soppath", 0, 0.0f);

        export_node = (ROP_Node*)rop->findNode("usd_export");
        if (!export_node)
        {
            export_node = (ROP_Node*)rop->createNode("usd", "usd_export");

            if (!export_node || !export_node->runCreateScript())
            {
                writer.error("Failed to create usd export node");
                return false;
            }
        }

        UT_String sop_import_path;
        sop_import->getFullPath(sop_import_path);

        out_path += ".usd";

        export_node->setString(sop_import_path.c_str(), CH_STRING_LITERAL, "loppath", 0, 0.0f);
        export_node->setString(out_path.c_str(), CH_STRING_LITERAL, "lopoutput", 0, 0.0f);
        export_node->setString("flattenalllayers", CH_STRING_LITERAL, "savestyle", 0, 0.0f);
    }
    else
    {
        writer.error("Unknown output format");
        return false;
    }

    // Execute the ROP for one frame
    OP_ERROR error = export_node->execute(0.0f);
    if (error >= UT_ERROR_ABORT)
    {
        writer.error("Failed to execute export");
        return false;
    }

    // Load the exported file back into memory
    std::ifstream file(out_path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        writer.error("Failed to open exported file");
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    file_data.resize(size);
    if (!file.read(file_data.data(), size))
    {
        writer.error("Failed to read exported file");
        return false;
    }

    file.close();
    std::filesystem::remove(out_path);

    return true;
}

bool export_geometry(MOT_Director* director, EOutputFormat format, OP_Node* node, StreamWriter& writer)
{
    rmt_ScopedCPUSample(ExportGeometry, 0);

    SOP_Node* sop = node->castToSOPNode();
    if (!sop)
    {
        writer.error("Node is not a SOP node");
        return false;
    }

    OP_Context context(0.0);
    const GU_Detail* gdp = sop->getCookedGeo(context);
    if (!gdp)
    {
        writer.error("Failed to get cooked geometry");
        return false;
    }

    if (format == EOutputFormat::RAW)
    {
        Geometry geo;
        if (!export_geometry_raw(gdp, geo, writer))
        {
            writer.error("Failed to export raw geometry");
            return false;
        }

        writer.geometry(geo);
    }
    else if (format == EOutputFormat::OBJ)
    {
        std::vector<char> file_data;
        if (!export_geometry_obj(gdp, file_data, writer))
        {
            writer.error("Failed to export obj geometry");
            return false;
        }

        writer.file("generated_model.obj", file_data);
    }
    else if (format == EOutputFormat::GLB)
    {
        std::vector<char> file_data;
        if (!export_geometry_with_format(director, sop, format, file_data, writer))
        {
            writer.error("Failed to export glb geometry");
            return false;
        }

        writer.file("generated_model.glb", file_data);
    }
    else if (format == EOutputFormat::FBX)
    {
        std::vector<char> file_data;
        if (!export_geometry_with_format(director, sop, format, file_data, writer))
        {
            writer.error("Failed to export fbx geometry");
            return false;
        }

        writer.file("generated_model.fbx", file_data);
    }
    else if (format == EOutputFormat::USD)
    {
        std::vector<char> file_data;
        if (!export_geometry_with_format(director, sop, format, file_data, writer))
        {
            writer.error("Failed to export usd geometry");
            return false;
        }

        writer.file("generated_model.usd", file_data);
    }
    else
    {
        writer.error("Unknown output format");
        return false;
    }

    return true;
}

void cleanup_session(MOT_Director* director)
{
    rmt_ScopedCPUSample(CleanupSession, 0);

    OP_Network* obj = (OP_Network*)director->findNode("/obj");
    if (obj)
    {
        OP_Network* geo = (OP_Network*)obj->findNode("geo");
        if (geo)
        {
            for (int j = geo->getNchildren() - 1; j >= 0; j--)
            {
                geo->destroyNode(geo->getChild(j));
            }
        }
    }
}

bool cook_internal(HoudiniSession& session, const CookRequest& request, StreamWriter& writer)
{
    // Try to re-use an existing node
    OP_Node* node = nullptr;
    {
        rmt_ScopedCPUSample(UpdateScene, 0);
        if (can_incremental_cook(session.m_state, request))
        {
            node = find_node(session.m_director);
            if (!node)
            {
                util::log() << "Failed to find existing node" << std::endl;
            }
        }

        if (!node)
        {
            cleanup_session(session.m_director);
            session.m_state = CookRequest();

            // Install the library
            std::string node_type = install_library(session.m_director, request.hda_file.file_path, request.definition_index, writer);
            if (node_type.empty())
            {
                return false;
            }

            // Setup the node
            node = create_node(session.m_director, node_type, writer);
            if (!node)
            {
                return false;
            }

            set_inputs(node, request.inputs, writer);
        }

        set_parameters(node, request.parameters, writer);
        session.m_state = request;
    }

    // Cook the node
    {
        rmt_ScopedCPUSample(CookNode, 0);

        OP_Context context(0.0);
        bool success = node->cook(context);

        // Log errors
        UT_Array<UT_Error> errors;
        node->getRawErrors(errors, true);
        for (const UT_Error& error : errors)
        {
            UT_String error_message;
            error.getErrorMessage(error_message, UT_ERROR_NONE, true);
            writer.error(std::string(error_message.c_str()));
        }

        if (!success)
        {
            writer.error("Failed to cook node");
            return false;
        }
    }

    // Export results
    if (!export_geometry(session.m_director, request.format, node, writer))
    {
        writer.error("Failed to export geometry");
        return false;
    }

    return true;
}

bool cook(HoudiniSession& session, const CookRequest& request, StreamWriter& writer)
{
    rmt_ScopedCPUSample(Cook, 0);

    // Setup interrupt handler
    InterruptHandler interruptHandler(writer);
    UT_Interrupt* interrupt = UTgetInterrupt();
    interrupt->setInterruptHandler(&interruptHandler);
    interrupt->setEnabled(true);
    interruptHandler.start_timeout(COOK_TIMEOUT_SECONDS);

    // Execute automation
    auto start_time = std::chrono::high_resolution_clock::now();
    bool result = cook_internal(session, request, writer);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    util::log() << "Processed cook request in " << std::fixed << std::setprecision(2) << duration.count() / 1000.0 << "ms" << std::endl;

    // Cleanup
    interrupt->setEnabled(false);
    interrupt->setInterruptHandler(nullptr);

    return result;
}

}

