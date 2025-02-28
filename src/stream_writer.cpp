#include "mongoose.h"
#include "stream_writer.h"

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
    m_websocket.push_response(StreamMessage{m_connection_id, json});
}