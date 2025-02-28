#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <queue>

struct StreamMessage
{
    int connection_id;
    std::string message;
};

class MessageQueue
{
public:
    void push_request(const StreamMessage& message);
    void push_response(const StreamMessage& message);
    bool try_pop_request(StreamMessage& message);
    bool try_pop_response(StreamMessage& message);

private:
    std::mutex m_mutex;
    std::queue<StreamMessage> m_requests;
    std::queue<StreamMessage> m_responses;
};

class WebSocket
{
public:
    WebSocket(const std::string& url, int port);
    ~WebSocket();

    bool try_pop_request(StreamMessage& message);
    void push_response(const StreamMessage& message);

private:
    std::thread m_thread;
    MessageQueue m_queue;
};
