#pragma once

#include <string>

struct Geometry;
class WebSocket;

enum class AutomationState
{
    Start,
    End
};

class StreamWriter
{
public:
    StreamWriter(WebSocket& websocket, int connection_id)
        : m_websocket(websocket), m_connection_id(connection_id)
    {}

    void state(AutomationState state);
    void status(const std::string& message);
    void error(const std::string& message);
    void file(const std::string& path);
    void geometry(const Geometry& geometry);

private:
    void writeToStream(const std::string& op, const std::string& data);

    WebSocket& m_websocket;
    int m_connection_id;
};