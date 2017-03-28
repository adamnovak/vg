#include "timer.hpp"

#include <iostream>

namespace vg {

using namespace std;

TimerExpiredException::TimerExpiredException(): runtime_error("Thread timer expired!") {
    // Nothing to do!
}

void Timer::check() {
    
    if (thread_timer.active) {

        // What time is it?
        auto now = chrono::high_resolution_clock::now();
        
        if (now > thread_timer.expiration_time) {
            // It's too late!
            expired_timers++;
            throw TimerExpiredException();
        }
    }
}

void Timer::start(chrono::milliseconds ms) {
    // Record a started timer
    started_timers++;
    // Start the timer
    thread_timer.active = true;
    // It starts now
    thread_timer.start_time = chrono::high_resolution_clock::now();
    // And expires later
    thread_timer.expiration_time = thread_timer.start_time + ms;
}

chrono::milliseconds Timer::stop() {
    // Stop the timer
    thread_timer.active = false;
    // Return the elapsed time
    return chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - thread_timer.start_time);
}

chrono::milliseconds Timer::time(chrono::milliseconds ms, function<void(void)> to_time) {
    // Start the timer
    Timer::start(ms);
    // Run the function
    to_time();
    // Stop the timer and return elapsed time
    return Timer::stop();
}

bool Timer::try_time(chrono::milliseconds ms, function<void(void)> to_time) {
    try {
        // Run the function under the timer
        Timer::time(ms, to_time);
        // It finished successfully
        return true;
    } catch (TimerExpiredException e) {
        // The function did not finish. But since we did an exception, all its
        // mutexes and stuff are cleaned up.
        
        // Stop the timer so the next check doesn;t throw again
        chrono::milliseconds elapsed = Timer::stop();
        
        // Grab the statistics
        size_t started = started_timers;
        size_t expired = expired_timers;
        
#pragma omp critical (cerr)
        cerr << "[vg::Timer] warning: Thread timer expired at " << elapsed.count() << " ms; "
            << expired << "/" << started << " timers expired" << endl;
        
        return false;
    }
}

bool Timer::try_time(size_t ms, function<void(void)> to_time) {
    // Make the ms count be a real chrono type.
    return Timer::try_time(chrono::milliseconds(ms), to_time);
}

// Put the thread-local static Timer in this compilation unit
thread_local Timer Timer::thread_timer{};

atomic<size_t> Timer::started_timers{0};
atomic<size_t> Timer::expired_timers{0};

}
