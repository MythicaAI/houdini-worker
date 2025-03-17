#include "Remotery.h"
#include "util.h"
#include "websocket.h"

#include <iostream>
#include <mutex>

struct WebSocketThreadConfig
{
    int m_client_port;
    int m_admin_port;
    mg_mgr& m_mgr;
    MessageQueue& m_queue;
};

struct WebSocketThreadState
{
    std::map<int, struct mg_connection*> connection_map;
    MessageQueue& m_queue;
};

template<bool is_admin>
static void fn_ws(struct mg_connection* c, int ev, void* ev_data)
{
    WebSocketThreadState* state = (WebSocketThreadState*)c->fn_data;

    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        mg_ws_upgrade(c, hm, NULL);
    }
    else if (ev == MG_EV_WS_OPEN)
    {
        util::log() << "Connection opened " << c->id << " " << (is_admin ? "(admin)" : "(client)") << std::endl;
        state->connection_map[c->id] = c;
    }
    else if (ev == MG_EV_WS_MSG)
    {
        struct mg_ws_message* wm = (struct mg_ws_message*) ev_data;

        std::string message(wm->data.buf, wm->data.len);
        util::log() << "Received message from connection " << c->id << ": " << (message.length() > 100 ? message.substr(0, 97) + "..." : message) << std::endl;

        StreamMessage msg;
        msg.connection_id = c->id;
        msg.message = message;

        state->m_queue.push_request(msg);
    }
    else if (ev == MG_EV_CLOSE)
    {
        util::log() << "Connection closed " << c->id << std::endl;
        state->connection_map.erase(c->id);
    }
}

static void websocket_thread(const WebSocketThreadConfig& config)
{
    WebSocketThreadState state{{}, config.m_queue};

    std::string client_listen_addr = std::string("ws://") + "0.0.0.0:" + std::to_string(config.m_client_port);
    std::string admin_listen_addr = std::string("ws://") + "0.0.0.0:" + std::to_string(config.m_admin_port);
    mg_http_listen(&config.m_mgr, client_listen_addr.c_str(), fn_ws<false>, &state);
    mg_http_listen(&config.m_mgr, admin_listen_addr.c_str(), fn_ws<true>, &state);

    while (true)
    {
        StreamMessage response;
        while (config.m_queue.try_pop_response(response))
        {
            rmt_ScopedCPUSample(SendResponse, 0);

            auto it = state.connection_map.find(response.connection_id);
            if (it != state.connection_map.end())
            {
                mg_ws_send(it->second, response.message.c_str(), response.message.length(), WEBSOCKET_OP_TEXT);
            }
            else
            {
                util::log() << "Response for unknown connection " << response.connection_id << std::endl;
            }
        }

        mg_mgr_poll(&config.m_mgr, 1000);
    }
}

void MessageQueue::push_request(const StreamMessage& message)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_requests.push(message);
    }
    m_request_cv.notify_one();
}

void MessageQueue::push_response(const StreamMessage& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_responses.push(message);
}

bool MessageQueue::try_pop_request(StreamMessage& message, int timeout_ms)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto has_request = [this] { return !m_requests.empty(); };
    if (!m_request_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), has_request))
    {
        return false;
    }

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

WebSocket::WebSocket(int client_port, int admin_port)
{
    mg_mgr_init(&m_mgr);
    mg_wakeup_init(&m_mgr);

    WebSocketThreadConfig config = { client_port, admin_port, m_mgr, m_queue };
    m_thread = std::thread([config]{ websocket_thread(config); });
}

WebSocket::~WebSocket()
{
    m_thread.join();

    mg_mgr_free(&m_mgr);
}

bool WebSocket::try_pop_request(StreamMessage& message, int timeout_ms)
{
    return m_queue.try_pop_request(message, timeout_ms);
}

void WebSocket::push_response(const StreamMessage& message)
{
    m_queue.push_response(message);
    mg_wakeup(&m_mgr, message.connection_id, "wake", 4);
}
