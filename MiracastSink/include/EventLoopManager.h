#ifndef EVENT_LOOP_MANAGER_H
#define EVENT_LOOP_MANAGER_H

#include <evpp/event_loop.h>
#include <thread>
#include <atomic>

class EventLoopManager {
public:
    EventLoopManager() : running_(false) {}

    void Start() {
        if (running_.exchange(true)) {
            return;
        }
        loop_thread_ = std::thread([this]() {
            loop_.Run();
            });
    }

    void Stop() {
        if (!running_.exchange(false)) {
            return;
        }
        loop_.Stop();
        if (loop_thread_.joinable()) {
            loop_thread_.join();
        }
    }

    evpp::EventLoop* GetLoop() {
        return &loop_;
    }

    ~EventLoopManager() {
        Stop();
    }

private:
    evpp::EventLoop loop_;
    std::thread loop_thread_;
    std::atomic<bool> running_;
};

#endif // EVENT_LOOP_MANAGER_H