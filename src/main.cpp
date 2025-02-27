#include "automation.h"
#include "interrupt.h"
#include "mongoose.h"
#include "streaming.h"
#include "types.h"

#include <MOT/MOT_Director.h>
#include <PI/PI_ResourceManager.h>
#include <UT/UT_Main.h>
#include <iostream>

static const int COOK_TIMEOUT_SECONDS = 1;

static bool process_message(const std::string& message, MOT_Director* boss, StreamWriter& writer)
{
    CookRequest request;
    if (!util::parse_request(message, request))
    {
        writer.error("Failed to parse request");
        return false;
    }

    return util::cook(boss, request, writer);
}

struct AppContext
{
    MOT_Director* boss;
};

static void fn_ws(struct mg_connection* c, int ev, void* ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        std::cout << "Worker: Received new connection" << std::endl;
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        mg_ws_upgrade(c, hm, NULL);
    }
    else if (ev == MG_EV_WS_MSG)
    {
        struct mg_ws_message* wm = (struct mg_ws_message*) ev_data;
        
        std::string message(wm->data.buf, wm->data.len);
        std::cout << "Worker: Received message: " << message << std::endl;

        AppContext* ctx = (AppContext*)c->fn_data;

        // Setup interrupt handler
        StreamWriter writer(c);
        InterruptHandler interruptHandler(writer);
        UT_Interrupt* interrupt = UTgetInterrupt();
        interrupt->setInterruptHandler(&interruptHandler);
        interrupt->setEnabled(true);

        // Execute automation
        writer.state(AutomationState::Start);

        bool result = process_message(message, ctx->boss, writer);

        writer.state(AutomationState::End);
        
        // Cleanup
        interrupt->setEnabled(false);
        interrupt->setInterruptHandler(nullptr);

        util::cleanup_session(ctx->boss);
    }
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
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    
    AppContext ctx = { boss };

    std::string listen_addr = std::string("ws://0.0.0.0:") + std::to_string(port);
    mg_http_listen(&mgr, listen_addr.c_str(), fn_ws, &ctx);

    // Event loop
    std::cout << "Worker: Listening on port " << port << std::endl;
    while (true)
    {
        mg_mgr_poll(&mgr, 1000);
    }

    mg_mgr_free(&mgr);
    return 0;
}

UT_MAIN(theMain);