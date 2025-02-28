#include "automation.h"
#include "interrupt.h"
#include "stream_writer.h"
#include "types.h"
#include "websocket.h"

#include <MOT/MOT_Director.h>
#include <PI/PI_ResourceManager.h>
#include <UT/UT_Main.h>
#include <iostream>

static const int COOK_TIMEOUT_SECONDS = 1;

static bool execute_automation(const std::string& message, MOT_Director* boss, StreamWriter& writer)
{
    CookRequest request;
    if (!util::parse_request(message, request))
    {
        writer.error("Failed to parse request");
        return false;
    }

    return util::cook(boss, request, writer);
}

static void process_message(MOT_Director* boss, const std::string& message, StreamWriter& writer)
{
    std::cout << "Worker: Processing messages" << std::endl;

    // Setup interrupt handler
    InterruptHandler interruptHandler(writer);
    UT_Interrupt* interrupt = UTgetInterrupt();
    interrupt->setInterruptHandler(&interruptHandler);
    interrupt->setEnabled(true);
    interruptHandler.start_timeout(COOK_TIMEOUT_SECONDS);

    // Execute automation
    writer.state(AutomationState::Start);

    bool result = execute_automation(message, boss, writer);

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
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    const int port = std::stoi(argv[1]);
    
    // Initialize Houdini
    MOT_Director* boss = new MOT_Director("standalone");
    OPsetDirector(boss);
    PIcreateResourceManager();

    // Initialize websocket server
    WebSocket websocket("0.0.0.0", port);

    while (true)
    {
        StreamMessage message;
        if (websocket.try_pop_request(message, 1000))
        {
            StreamWriter writer(websocket, message.connection_id);
            process_message(boss, message.message, writer);
        }
    }

    return 0;
}

UT_MAIN(theMain);