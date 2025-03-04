#include "automation.h"
#include "stream_writer.h"
#include "types.h"

#include <OP/OP_Director.h>
#include <OP/OP_OTLLibrary.h>
#include <GEO/GEO_Primitive.h>
#include <GU/GU_Detail.h>
#include <SOP/SOP_Node.h>
#include <MOT/MOT_Director.h>
#include <filesystem>
#include <iostream>

namespace util
{

static std::string install_library(MOT_Director* boss, const std::string& hda_file, int definition_index, StreamWriter& writer)
{
    // Load the library
    OP_OTLManager& manager = boss->getOTLManager();
    manager.installLibrary(hda_file.c_str());

    int library_index = manager.findLibrary(hda_file.c_str());
    if (library_index < 0)
    {
        writer.error("Failed to find library: " + hda_file);
        return "";
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

static OP_Node* create_node(MOT_Director* boss, const std::string& node_type, StreamWriter& writer)
{
     // Find the root /obj network
    OP_Network* obj = (OP_Network*)boss->findNode("/obj");
    if (!obj)
    {
        writer.error("Failed to find obj network");
        return nullptr;
    }
    assert(obj->getNchildren() == 0);

    // Create geo node
    OP_Network* geo_node = (OP_Network*)obj->createNode("geo", "processor_parent");
    if (!geo_node || !geo_node->runCreateScript())
    {
        writer.error("Failed to create geo node");
        return nullptr;
    }

    // Create the SOP node
    OP_Node* node = geo_node->createNode(node_type.c_str(), "processor");
    if (!node || !node->runCreateScript())
    {
        writer.error("Failed to create node of type: " + node_type);
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
        input_node->setInt("bImportAnimation", 0, 0.0f, 1);
        input_node->setInt("bImportBoneSkin", 0, 0.0f, 1);
        input_node->setInt("bConvertYUp", 0, 0.0f, 1);
        input_node->setInt("bUnlockGeo", 0, 0.0f, 1);
        input_node->setInt("pack", 0, 0.0f, 1);
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

static void set_inputs(OP_Node* node, const std::map<int, std::string>& inputs, StreamWriter& writer)
{
    OP_Network* parent = node->getParent();

    for (const auto& [index, path] : inputs)
    {
        OP_Node* input_node = create_input_node(parent, path, writer);
        if (!input_node)
        {
            input_node = parent->createNode("null");
            if (!input_node || !input_node->runCreateScript())
            {
                writer.error("Failed to create null node for " + path);
                continue;
            }
        }

        node->setInput(index, input_node);
    }
}

static void set_parameters(OP_Node* node, const ParameterSet& parameters)
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
    }
}

bool export_geometry(OP_Node* node, Geometry& geom, StreamWriter& writer)
{
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

    GA_ROHandleV3 P_handle(gdp, GA_ATTRIB_POINT, "P");
    GA_ROHandleV3 N_handle(gdp, GA_ATTRIB_POINT, "N");
    GA_ROHandleV3 UV_handle(gdp, GA_ATTRIB_VERTEX, "uv");
    if (!P_handle.isValid())
    {
        writer.error("Geometry missing point attribute");
        return false;
    }

    int prim_index = 0;
    const GEO_Primitive* prim;
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMPOLY)
        {
           continue;
        }

        GA_Range pt_range = prim->getPointRange();
        GA_Size num_points = pt_range.getEntries();
        if (num_points < 3)
        {
            continue;
        }

        int base_index = geom.points.size() / 3;
        assert(geom.points.size() % 3 == 0);

        for (GA_Iterator pt_it(pt_range); !pt_it.atEnd(); ++pt_it)
        {
            GA_Offset ptOff = *pt_it;

            UT_Vector3 pos = P_handle.get(ptOff);
            geom.points.push_back(pos.x());
            geom.points.push_back(pos.y());
            geom.points.push_back(pos.z());

            if (N_handle.isValid())
            {
                UT_Vector3 norm = N_handle.get(ptOff);
                geom.normals.push_back(norm.x());
                geom.normals.push_back(norm.y());
                geom.normals.push_back(norm.z());
            }

            if (UV_handle.isValid())
            {
                UT_Vector3 uv = UV_handle.get(ptOff);
                geom.uvs.push_back(uv.x());
                geom.uvs.push_back(uv.y());
            }
        }

        // Triangulate as a fan
        int num_tris = num_points - 2;
        for (int i = 0; i < num_tris; i++)
        {
            geom.indices.push_back(base_index + 0);
            geom.indices.push_back(base_index + i + 1);
            geom.indices.push_back(base_index + i + 2);
        }

        prim_index++;
    }

    if (geom.points.size() == 0)
    {
        writer.error("Geometry contains no primitives");
        return false;
    }

    return true;
}

bool cook(MOT_Director* boss, const CookRequest& request, StreamWriter& writer)
{
    // Install the library
    std::string node_type = install_library(boss, request.hda_file, request.definition_index, writer);
    if (node_type.empty())
    {
        return false;
    }

    // Setup the node
    OP_Node* node = create_node(boss, node_type, writer);
    if (!node)
    {
        return false;
    }

    set_inputs(node, request.inputs, writer);
    set_parameters(node, request.parameters);

    // Cook the node
    OP_Context context(0.0);
    if (!node->cook(context))
    {
        writer.error("Failed to cook node");
        return false;
    }

    // Export results
    Geometry geo;
    if (!export_geometry(node, geo, writer))
    {
        writer.error("Failed to export geometry");
        return false;
    }

    writer.geometry(geo);

    return true;
}

void cleanup_session(MOT_Director* boss)
{
    for (int i = boss->getNchildren() - 1; i >= 0; i--)
    {
        OP_Network* subnetwork = (OP_Network*)boss->getChild(i);
        for (int j = subnetwork->getNchildren() - 1; j >= 0; j--)
        {
            subnetwork->destroyNode(subnetwork->getChild(j));
        }
    }
}

}
