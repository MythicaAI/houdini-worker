#include "websocket.h"

#include "mongoose.h"

#include <iostream>
#include <mutex>

struct WebSocketThreadConfig
{
    std::string m_url;
    int m_port;
    MessageQueue& m_queue;
};

struct WebSocketThreadState
{
    std::map<int, struct mg_connection*> connection_map;
    MessageQueue& m_queue;
};

static void fn_ws(struct mg_connection* c, int ev, void* ev_data)
{ 
    WebSocketThreadState* state = (WebSocketThreadState*)c->fn_data;

    if (ev == MG_EV_OPEN)
    {
        std::cout << "Worker: Connection opened " << c->id << std::endl;
        state->connection_map[c->id] = c;
    }
    else if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        mg_ws_upgrade(c, hm, NULL);
    }
    else if (ev == MG_EV_WS_MSG)
    {
        struct mg_ws_message* wm = (struct mg_ws_message*) ev_data;
        
        std::string message(wm->data.buf, wm->data.len);
        std::cout << "Worker: Received message: " << message << std::endl;

        StreamMessage msg;
        msg.connection_id = c->id;
        msg.message = message;

        state->m_queue.push_request(msg);
    }
    else if (ev == MG_EV_CLOSE)
    {
        std::cout << "Worker: Connection closed " << c->id << std::endl;
        state->connection_map.erase(c->id);
    }
}

static void websocket_thread(const WebSocketThreadConfig& config)
{
    WebSocketThreadState state{{}, config.m_queue};

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    std::string listen_addr = std::string("ws://") + config.m_url + ":" + std::to_string(config.m_port);
    mg_http_listen(&mgr, listen_addr.c_str(), fn_ws, &state);

    while (true)
    {
        StreamMessage response;
        while (config.m_queue.try_pop_response(response))
        {
            auto it = state.connection_map.find(response.connection_id);
            if (it != state.connection_map.end())
            {
                mg_ws_send(it->second, response.message.c_str(), response.message.length(), WEBSOCKET_OP_TEXT);
            }
            else
            {
                std::cout << "Worker: Response for unknown connection " << response.connection_id << std::endl;
            }
        }

        mg_mgr_poll(&mgr, 10);
    }

    mg_mgr_free(&mgr);
}

void MessageQueue::push_request(const StreamMessage& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requests.push(message);
}

void MessageQueue::push_response(const StreamMessage& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_responses.push(message);
}

bool MessageQueue::try_pop_request(StreamMessage& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_requests.empty())
        return false;

    message = m_requests.front();
    m_requests.pop();
    return true;
}

bool MessageQueue::try_pop_response(StreamMessage& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_responses.empty())
        return false;

    message = m_responses.front();
    m_responses.pop();
    return true;
}

WebSocket::WebSocket(const std::string& url, int port)
{
    WebSocketThreadConfig config = { url, port, m_queue };
    m_thread = std::thread([config]{ websocket_thread(config); });
}

WebSocket::~WebSocket()
{
    m_thread.join();
}

bool WebSocket::try_pop_request(StreamMessage& message)
{
    return m_queue.try_pop_request(message);
}

void WebSocket::push_response(const StreamMessage& message)
{
    m_queue.push_response(message);
}
