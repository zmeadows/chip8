#pragma once

#include <condition_variable>
#include <mutex>

#include "chip8_timer.hpp"

namespace chip8 {

// A simple flag for synchronizing two threads.
// This is not meant to be robust and only intended to work with our simple use-case
// of one 'producer' and one 'consumer'
class sync_flag {
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    bool flag;

public:
    sync_flag() : flag(false) {}

    bool check() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return flag;
    }

    void set(void)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            flag = true;
        }
        cv.notify_all();
    }

    void unset(void)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            flag = false;
        }
        cv.notify_all();
    }

    void wait(bool state) const
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (flag != state) {
            cv.wait(lock);
        }
    }

    bool wait_for(bool state, const chip8::timer::clock::duration& wait_time) const
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, wait_time);
        return flag == state;
    }

    bool wait_until(bool state, const chip8::timer::clock::time_point& release_time) const
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_until(lock, release_time);
        return flag == state;
    }
};

} // namespace chip8