#pragma once

#include "mongoose.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <queue>

static const int INVALID_CONNECTION_ID = -1;

enum class StreamMessageType
{
    ConnectionOpen,
    Message,
    ConnectionClose
};

struct StreamMessage
{
    int connection_id;
    bool is_admin;
    StreamMessageType type;
    std::string message;
};

class MessageQueue
{
public:
    void push_request(const StreamMessage& message);
    void push_response(const StreamMessage& message);
    bool try_pop_request(StreamMessage& message, int timeout_ms);
    bool try_pop_response(StreamMessage& message);

private:
    std::mutex m_mutex;
    std::condition_variable m_request_cv;

    std::queue<StreamMessage> m_requests;
    std::queue<StreamMessage> m_responses;
};

class WebSocket
{
public:
    WebSocket(const std::string& client_endpoint, const std::string& admin_endpoint);
    ~WebSocket();

    bool try_pop_request(StreamMessage& message, int timeout_ms);
    void push_response(int connection_id, const std::string& message);

private:
    mg_mgr m_mgr;

    std::thread m_thread;
    MessageQueue m_queue;
};
