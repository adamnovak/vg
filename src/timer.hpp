#ifndef VG_TIMER_H
#define VG_TIMER_H


#include <chrono>
#include <exception>
#include <stdexcept>
#include <functional>
#include <atomic>


/** \file timer.hpp
 * 
 * Provides a system for limiting the runtime of function call trees, so that
 * straggling reads do not bog down an entire pipeline.
 */
 
namespace vg {

using namespace std;

/**
 * Represents an exception thrown when a timer expires.
 */
class TimerExpiredException: public runtime_error {
public:
    TimerExpiredException();
};

/**
 * Maintains a timer for each thread. The timer can be set, and then a function
 * can be called to check the time elapsed and throw if too much time has gone
 * by.
 *
 * Currently not re-entrant: only one timer can be running in a thread at a
 * time.
 */
class Timer {
public:

    /**
     * Check the timer for the current thread and throw an exception if it is
     * expired.
     */
    static void check();

    /**
     * Start the timer for the current thread, allotting the given number of
     * milliseconds.
     */
    static void start(chrono::milliseconds ms);
    
    /**
     * Stop the timer for the current thread, so that further check() calls will
     * not throw. Returns the elapsed time.
     */
    static chrono::milliseconds stop();
    
    /**
     * Execute the given function, limited to the given amount of time. Return
     * the time elapsed, and raise TimerExpiredException if the function runs
     * out of time. The function being timed must call Timer::check()
     * periodically, and not catch TimerExpiredException itself.
     */
    static chrono::milliseconds time(chrono::milliseconds ms, function<void(void)> to_time);
    
    /**
     * Execute the given function. Return true if it finishes, and false if it
     * runs out of time and throws a TimerExpiredException. The function being
     * timed must call Timer::check() periodically, and not catch
     * TimerExpiredException itself.
     */
    static bool try_time(chrono::milliseconds ms, function<void(void)> to_time);
    
    /**
     * Implement try_time for raw millisecond counts.
     */
    static bool try_time(size_t ms, function<void(void)> to_time);

private:
    /// Constructor is private
    Timer() = default;
    
    /// We keep a static instance for each thread
    static thread_local Timer thread_timer;
    
    /// We keep global statistics when timers start and expire
    static atomic<size_t> started_timers;
    static atomic<size_t> expired_timers;
    
    /// Is this Timer object for this thread active?
    bool active = false;
    
    /// When did this Timer object start?
    chrono::high_resolution_clock::time_point start_time;
    
    /// When will this Timer object expire?
    chrono::high_resolution_clock::time_point expiration_time;

};

}

#endif
