#include "Remotery.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"
#include "websocket.h"
#include <UT/UT_Base64.h>
#include <UT/UT_JSONValue.h>
#include <UT/UT_WorkBuffer.h>

#include <iostream>

void StreamWriter::state(AutomationState state)
{
    writeToStream(m_client_id, "automation", state == AutomationState::Start ? "\"start\"" : "\"end\"");
}

void StreamWriter::status(const std::string& message)
{
    UT_JSONValue json;
    json.setString(message);
    writeToStream(m_client_id, "status", json.toString().c_str());
}

void StreamWriter::error(const std::string& message)
{
    UT_JSONValue json;
    json.setString(message);
    writeToStream(m_client_id, "error", json.toString().c_str());
}

void StreamWriter::file(const std::string& file_name, const std::vector<char>& file_data)
{
    rmt_ScopedCPUSample(WriteFile, 0);

    UT_WorkBuffer encoded_buffer;
    UT_Base64::encode((uint8_t*)file_data.data(), file_data.size(), encoded_buffer);
    std::string encoded(encoded_buffer.buffer(), encoded_buffer.length());

    writeToStream(m_client_id, "file", "{\"file_name\":\"" + file_name + "\", \"content_base64\":\"" + encoded + "\"}");
}

void StreamWriter::geometry(const Geometry& geometry)
{
    rmt_ScopedCPUSample(WriteGeometry, 0);

    std::string json = "{\"points\":[";
    for (size_t i = 0; i < geometry.points.size(); i++)
    {
        json += std::to_string(geometry.points[i]);
        if (i < geometry.points.size() - 1)
        {
            json += ",";
        }
    }
    if (geometry.normals.size() > 0)
    {
        json += "],\"normals\":[";
        for (size_t i = 0; i < geometry.normals.size(); i++)
        {
            json += std::to_string(geometry.normals[i]);
            if (i < geometry.normals.size() - 1)
            {
                json += ",";
            }
        }
    }
    if (geometry.uvs.size() > 0)
    {
        json += "],\"uvs\":[";
        for (size_t i = 0; i < geometry.uvs.size(); i++)
        {
            json += std::to_string(geometry.uvs[i]);
            if (i < geometry.uvs.size() - 1)
            {
                json += ",";
            }
        }
    }
    if (geometry.colors.size() > 0)
    {
        json += "],\"colors\":[";
        for (size_t i = 0; i < geometry.colors.size(); i++)
        {
            json += std::to_string(geometry.colors[i]);
            if (i < geometry.colors.size() - 1)
            {
                json += ",";
            }
        }
    }
    json += "],\"indices\":[";
    for (size_t i = 0; i < geometry.indices.size(); i++)
    {
        json += std::to_string(geometry.indices[i]);
        if (i < geometry.indices.size() - 1)
        {
            json += ",";
        }
    }
    json += "]}";

    writeToStream(m_client_id, "geometry", json);
}

void StreamWriter::file_resolve(const std::string& file_id)
{
    int target_id = m_admin_id != INVALID_CONNECTION_ID ? m_admin_id : m_client_id;
    writeToStream(target_id, "file_resolve", "{\"file_id\":\"" + file_id + "\"}");
}

void StreamWriter::writeToStream(int connection_id, const std::string& op, const std::string& data)
{
    std::string json = "{\"op\":\"" + op + "\",\"data\":" + data + "}\n";
    m_websocket.push_response(connection_id, json);
}