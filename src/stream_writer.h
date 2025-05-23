#pragma once

#include "types.h"

#include <string>
#include <vector>

class WebSocket;

enum class AutomationState
{
    Start,
    End
};

class StreamWriter
{
public:
    StreamWriter(WebSocket& websocket, int client_id, int admin_id)
        : m_websocket(websocket), m_client_id(client_id), m_admin_id(admin_id)
    {}

    void state(AutomationState state);

    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void admin_info(const std::string& message);
    void admin_warning(const std::string& message);
    void admin_error(const std::string& message);

    void file(const std::string& file_name, const std::vector<char>& file_data);
    void geometry(const GeometrySet& geometry_set);
    void file_resolve(const std::string& file_id);

private:
    void writeToStream(int connection_id, const std::string& op, const std::string& data);

    WebSocket& m_websocket;
    int m_client_id;
    int m_admin_id;
};