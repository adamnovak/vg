#ifndef VG_ALLOCATOR_CONFIG_HPP_INCLUDED
#define VG_ALLOCATOR_CONFIG_HPP_INCLUDED

/**
 * \file
 * Allocator configuration header. Used with either
 * allocator_config_jemalloc.cpp or allocator_config_system.cpp as appropriate
 * for the build.
 *
 * Contains startup functions and functions to manipulate memory profiling, if available.
 */

#include <cstddef>
#include <functional>
 
namespace vg {

/**
 * Interface for working with the memory allocator that is compiled into the build.
 */
struct AllocatorConfig {

    /**
     * If using a non-system memory allocator, initialize it to a safe
     * configuration in this runtime environment.
     *
     * Sets all the other hooks.
     */
    static void configure();

    /**
     * Turn memory profiling on or off, if available in the allocator.
     */
    static void set_profiling(bool should_profile);

    /**
     * Dump a memory profiling snapshot, if available in the allocator.
     */
    static void snapshot();

    // We can't actually call any of the functions defined here from code in
    // libvg, sicne the allocator config only links in in the executable alogn
    // with the allocator library.
    //
    // So we make the executable code grab and pass along function pointers for
    // anything we ened to call elsewhere.

    /**
     * Function pointer type for function that can set and remove memory arenas.
     *
     * Set all allocations in all OMP threads to come from the given memory
     * region of the given size, or restores normal allocator behavior if
     * region is null.
     *
     * Returns true if successful and false if not supported by the allocator.
     *
     * May only be called from the main thread.
     */
    using arena_hook_t = std::function<bool(char* region, size_t size)>;

    /**
     * Get the memory arena add/remove function.
     */
    static arena_hook_t get_arena_hook();

};

}
 
#endif
