#include "mongoose.h"
#include "streaming.h"

#include <iostream>

void StreamWriter::state(AutomationState state)
{
    writeToStream("automation", state == AutomationState::Start ? "start" : "end");
}

void StreamWriter::status(const std::string& message)
{
    writeToStream("status", message);
}

void StreamWriter::error(const std::string& message)
{
    writeToStream("error", message);
}

void StreamWriter::file(const std::string& path)
{
    writeToStream("file", path);
}

void StreamWriter::writeToStream(const std::string& op, const std::string& data)
{
    std::cout << "Worker: Sending response: " << op << " " << data << std::endl;
    std::string json = "{\"op\":\"" + op + "\",\"data\":\"" + data + "\"}\n";
    mg_ws_send(m_conn, json.c_str(), json.size(), WEBSOCKET_OP_TEXT);
}