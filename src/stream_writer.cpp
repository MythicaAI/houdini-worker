#include "mongoose.h"
#include "stream_writer.h"

#include <iostream>

void StreamWriter::state(AutomationState state)
{
    writeToStream("automation", state == AutomationState::Start ? "\"start\"" : "\"end\"");
}

void StreamWriter::status(const std::string& message)
{
    writeToStream("status", "\"" + message + "\"");
}

void StreamWriter::error(const std::string& message)
{
    writeToStream("error", "\"" + message + "\"");
}

void StreamWriter::file(const std::string& path)
{
    writeToStream("file", "\"" + path + "\"");
}

void StreamWriter::geometry(const Geometry& geometry)
{
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

    writeToStream("geometry", json);
}

void StreamWriter::writeToStream(const std::string& op, const std::string& data)
{
    std::cout << "Worker: Sending response: " << op << " " << data << std::endl;
    std::string json = "{\"op\":\"" + op + "\",\"data\":" + data + "}\n";
    m_websocket.push_response(StreamMessage{m_connection_id, json});
}