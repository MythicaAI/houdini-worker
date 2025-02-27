#include "interrupt.h"

void InterruptHandler::start(UT_Interrupt *intr,
    const UT_InterruptMessage &msg,
    const UT_StringRef &main_optext,
    int priority) 
{
    if (priority >= m_priority_threshold)
    {
        std::string message = msg.buildMessage().toStdString();
        m_writer.status(message);
    }
}

void InterruptHandler::push(UT_Interrupt *intr,
    const UT_InterruptMessage &msg,
    const UT_StringRef &main_optext,
    int priority) 
{
    if (priority >= m_priority_threshold)
    {
        std::string message = msg.buildMessage().toStdString();
        m_writer.status(message);
    }

    if (m_timeout_seconds > 0)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        if (elapsed >= m_timeout_seconds)
        {
            m_writer.error("Timeout");
            intr->interrupt();
        }
    }
}

void InterruptHandler::busyCheck(bool interrupted,
    float percent,
    float longpercent) 
{
    m_writer.status("Progress: " + std::to_string(percent) + " " + std::to_string(longpercent));
}

void InterruptHandler::start_timeout(int timeout_seconds)
{
    start_time = std::chrono::steady_clock::now();
    m_timeout_seconds = timeout_seconds;
}
