#pragma once

#include <string>

enum class AutomationState
{
    Start,
    End
};

class StreamWriter
{
public:
    StreamWriter(struct mg_connection* conn) : m_conn(conn) {}

    void state(AutomationState state);
    void status(const std::string& message);
    void error(const std::string& message);
    void file(const std::string& path);

private:
    void writeToStream(const std::string& op, const std::string& data);

    struct mg_connection* m_conn;
};