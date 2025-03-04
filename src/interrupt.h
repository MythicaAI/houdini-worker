#pragma once

#include <UT/UT_Interrupt.h>
#include <chrono>

class StreamWriter;

class InterruptHandler : public UT_InterruptHandler
{
public:
    InterruptHandler(StreamWriter& writer, int priority_threshold = 1)
        : m_writer(writer), m_priority_threshold(priority_threshold), m_timeout_seconds(0) {}

    virtual void start(UT_Interrupt *intr,
                      const UT_InterruptMessage &msg,
                      const UT_StringRef &main_optext,
                      int priority) override;

    virtual void push(UT_Interrupt *intr,
                     const UT_InterruptMessage &msg,
                     const UT_StringRef &main_optext,
                     int priority) override;

    virtual void busyCheck(bool interrupted,
                          float percent,
                          float longpercent) override;

    virtual void pop() override {}
    virtual void stop() override {}
    virtual void interruptAllowed(bool allowed, bool allow_ui) override {}

    void start_timeout(int timeout_seconds);

private:
    StreamWriter& m_writer;
    int m_priority_threshold;

    std::chrono::steady_clock::time_point start_time;
    int m_timeout_seconds;
};
