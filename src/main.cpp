#include "automation.h"
#include "file_cache.h"
#include "session.h"
#include "Remotery.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"
#include "websocket.h"

#include <map>
#include <UT/UT_Main.h>

static void process_message(HoudiniSession& session, FileCache& file_cache, FileMap* file_map_admin, FileMap& file_map_client, const std::string& message, StreamWriter& writer)
{
    WorkerRequest request;
    if (!util::parse_request(message, request, writer))
    {
        writer.error("Failed to parse request");
        return;
    }

    if (std::holds_alternative<CookRequest>(request))
    {
        CookRequest& cook_req = std::get<CookRequest>(request);

        std::vector<std::string> unresolved_files;
        util::resolve_files(cook_req, file_map_admin, file_map_client, writer, unresolved_files);

        for (const std::string& file_id : unresolved_files)
        {
            writer.file_resolve(file_id);
        }

        if (!unresolved_files.empty())
        {
            writer.error("Failed to resolve files");
            return;
        }

        util::cook(session, cook_req, writer);
    }
    else if (std::holds_alternative<FileUploadRequest>(request))
    {
        FileUploadRequest& file_upload_req = std::get<FileUploadRequest>(request);

        std::string file_path = file_upload_req.file_path;
        if (file_path.empty())
        {
            file_path = file_cache.add_file(file_upload_req.content_base64, file_upload_req.content_type, writer);
        }

        if (!file_map_client.add_file(file_upload_req.file_id, file_path, writer))
        {
            writer.error("Failed to upload file: " + file_upload_req.file_id);
        }
    }
}

int find_admin_id(const std::map<int, ClientSession>& sessions)
{
    for (const auto& session : sessions)
    {
        if (session.second.m_is_admin)
        {
            return session.first;
        }
    }

    return INVALID_CONNECTION_ID;
}

int theMain(int argc, char *argv[])
{
    if (argc != 3)
    {
        util::log() << "Usage: " << argv[0] << " <client_endpoint> <admin_endpoint>\n";
        return 1;
    }

    const std::string client_endpoint = argv[1];
    const std::string admin_endpoint = argv[2];

    Remotery* rmt;
    rmt_CreateGlobalInstance(&rmt);

    // Initialize worker state
    FileCache file_cache;
    HoudiniSession houdini_session;
    std::map<int, ClientSession> sessions;

    // Initialize websocket server
    WebSocket websocket(client_endpoint, admin_endpoint);

    util::log() << "Ready to receive requests" << std::endl;
    while (true)
    {
        StreamMessage message;
        if (websocket.try_pop_request(message, 1000))
        {
            rmt_ScopedCPUSample(ProcessRequest, 0);

            if (message.type == StreamMessageType::ConnectionOpen)
            {
                assert(sessions.find(message.connection_id) == sessions.end());
                sessions[message.connection_id] = ClientSession(message.is_admin);
            }
            else if (message.type == StreamMessageType::Message)
            {
                int client_id = message.connection_id;
                if (sessions.find(client_id) == sessions.end())
                {
                    util::log() << "Unknown connection id: " << client_id << std::endl;
                    continue;
                }

                int admin_id = !sessions[client_id].m_is_admin ? find_admin_id(sessions) : INVALID_CONNECTION_ID;

                FileMap& file_map_client = sessions[client_id].m_file_map;
                FileMap* file_map_admin = admin_id != INVALID_CONNECTION_ID ? &sessions[admin_id].m_file_map : nullptr;

                StreamWriter writer(websocket, client_id, admin_id);
                writer.state(AutomationState::Start);
                process_message(houdini_session, file_cache, file_map_admin, file_map_client, message.message, writer);
                writer.state(AutomationState::End);
            }
            else if (message.type == StreamMessageType::ConnectionClose)
            {
                assert(sessions.find(message.connection_id) != sessions.end());
                sessions.erase(message.connection_id);
            }
        }
    }

    rmt_DestroyGlobalInstance(rmt);
    return 0;
}

UT_MAIN(theMain);