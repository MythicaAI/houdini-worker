#include "automation.h"
#include "interrupt.h"
#include "file_cache.h"
#include "stream_writer.h"
#include "types.h"
#include "util.h"
#include "websocket.h"

#include <MOT/MOT_Director.h>
#include <PI/PI_ResourceManager.h>
#include <UT/UT_Main.h>
#include <iostream>

static const int COOK_TIMEOUT_SECONDS = 1;

static bool execute_automation(const std::string& message, MOT_Director* boss, FileCache& file_cache, StreamWriter& writer)
{
    WorkerRequest request;
    if (!util::parse_request(message, request, file_cache))
    {
        writer.error("Failed to parse request");
        return false;
    }

    if (std::holds_alternative<CookRequest>(request))
    {
        return util::cook(boss, std::get<CookRequest>(request), writer);
    }
    else if (std::holds_alternative<FileUploadRequest>(request))
    {
        FileUploadRequest file_upload_req = std::get<FileUploadRequest>(request);
        return file_cache.add_file(file_upload_req.file_path, file_upload_req.content_base64);
    }

    return false;
}

static void process_message(MOT_Director* boss, FileCache& file_cache, const std::string& message, StreamWriter& writer)
{
    util::log() << "Processing messages" << std::endl;

    // Setup interrupt handler
    InterruptHandler interruptHandler(writer);
    UT_Interrupt* interrupt = UTgetInterrupt();
    interrupt->setInterruptHandler(&interruptHandler);
    interrupt->setEnabled(true);
    interruptHandler.start_timeout(COOK_TIMEOUT_SECONDS);

    // Execute automation
    writer.state(AutomationState::Start);

    bool result = execute_automation(message, boss, file_cache, writer);

    writer.state(AutomationState::End);

    // Cleanup
    interrupt->setEnabled(false);
    interrupt->setInterruptHandler(nullptr);

    util::cleanup_session(boss);
}

int theMain(int argc, char *argv[])
{
    if (argc != 2)
    {
        util::log() << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    const int port = std::stoi(argv[1]);

    // Initialize Houdini
    MOT_Director* boss = new MOT_Director("standalone");
    OPsetDirector(boss);
    PIcreateResourceManager();

    FileCache file_cache;

    // Initialize websocket server
    WebSocket websocket("0.0.0.0", port);

    while (true)
    {
        StreamMessage message;
        if (websocket.try_pop_request(message, 1000))
        {
            StreamWriter writer(websocket, message.connection_id);
            process_message(boss, file_cache, message.message, writer);
        }
    }

    return 0;
}

UT_MAIN(theMain);