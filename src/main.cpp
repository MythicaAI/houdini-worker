#include "automation.h"
#include "interrupt.h"
#include "file_cache.h"
#include "houdini_session.h"
#include "Remotery.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"
#include "websocket.h"

#include <MOT/MOT_Director.h>
#include <PI/PI_ResourceManager.h>
#include <UT/UT_Main.h>
#include <chrono>
#include <iomanip>
#include <iostream>

static const int COOK_TIMEOUT_SECONDS = 60;

static bool execute_automation(const std::string& message, HoudiniSession& session, FileCache& file_cache, StreamWriter& writer)
{
    WorkerRequest request;
    if (!util::parse_request(message, request, file_cache, writer))
    {
        writer.error("Failed to parse request");
        return false;
    }

    if (std::holds_alternative<CookRequest>(request))
    {
        return util::cook(session, std::get<CookRequest>(request), writer);
    }
    else if (std::holds_alternative<FileUploadRequest>(request))
    {
        FileUploadRequest file_upload_req = std::get<FileUploadRequest>(request);
        return file_cache.add_file(file_upload_req.file_path, file_upload_req.content_base64);
    }

    return false;
}

static void process_message(HoudiniSession& session, FileCache& file_cache, const std::string& message, StreamWriter& writer)
{
    // Setup interrupt handler
    InterruptHandler interruptHandler(writer);
    UT_Interrupt* interrupt = UTgetInterrupt();
    interrupt->setInterruptHandler(&interruptHandler);
    interrupt->setEnabled(true);
    interruptHandler.start_timeout(COOK_TIMEOUT_SECONDS);

    // Execute automation
    writer.state(AutomationState::Start);

    auto start_time = std::chrono::high_resolution_clock::now();
    bool result = execute_automation(message, session, file_cache, writer);
    auto end_time = std::chrono::high_resolution_clock::now();

    writer.state(AutomationState::End);

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    util::log() << "Processed request in " << std::fixed << std::setprecision(2) << duration.count() / 1000.0 << "ms" << std::endl;

    // Cleanup
    interrupt->setEnabled(false);
    interrupt->setInterruptHandler(nullptr);
}

int theMain(int argc, char *argv[])
{
    if (argc != 2)
    {
        util::log() << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    const int port = std::stoi(argv[1]);

    Remotery* rmt;
    rmt_CreateGlobalInstance(&rmt);

    // Initialize Houdini
    FileCache file_cache;
    HoudiniSession session;

    // Initialize websocket server
    WebSocket websocket("0.0.0.0", port);

    util::log() << "Ready to receive requests" << std::endl;
    while (true)
    {
        StreamMessage message;
        if (websocket.try_pop_request(message, 1000))
        {
            rmt_ScopedCPUSample(ProcessRequest, 0);
            StreamWriter writer(websocket, message.connection_id);
            process_message(session, file_cache, message.message, writer);
        }
    }

    rmt_DestroyGlobalInstance(rmt);
    return 0;
}

UT_MAIN(theMain);