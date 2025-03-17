#include "automation.h"
#include "file_cache.h"
#include "houdini_session.h"
#include "Remotery.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"
#include "websocket.h"

#include <UT/UT_Main.h>

static void process_message(HoudiniSession& session, FileCache& file_cache, const std::string& message, StreamWriter& writer)
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

        if (!util::resolve_files(cook_req, file_cache, writer))
        {
            writer.error("Failed to resolve files");
            return;
        }

        util::cook(session, cook_req, writer);
    }
    else if (std::holds_alternative<FileUploadRequest>(request))
    {
        FileUploadRequest& file_upload_req = std::get<FileUploadRequest>(request);
        file_cache.add_file(file_upload_req.file_path, file_upload_req.content_base64);
    }
}

int theMain(int argc, char *argv[])
{
    if (argc != 3)
    {
        util::log() << "Usage: " << argv[0] << " <client_port> <admin_port>\n";
        return 1;
    }

    const int client_port = std::stoi(argv[1]);
    const int admin_port = std::stoi(argv[2]);

    Remotery* rmt;
    rmt_CreateGlobalInstance(&rmt);

    // Initialize Houdini
    FileCache file_cache;
    HoudiniSession session;

    // Initialize websocket server
    WebSocket websocket(client_port, admin_port);

    util::log() << "Ready to receive requests" << std::endl;
    while (true)
    {
        StreamMessage message;
        if (websocket.try_pop_request(message, 1000))
        {
            rmt_ScopedCPUSample(ProcessRequest, 0);

            StreamWriter writer(websocket, message.connection_id);
            writer.state(AutomationState::Start);
            process_message(session, file_cache, message.message, writer);
            writer.state(AutomationState::End);
        }
    }

    rmt_DestroyGlobalInstance(rmt);
    return 0;
}

UT_MAIN(theMain);