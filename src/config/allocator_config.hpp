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
 
namespace vg {

/**
 * Interface for working with the memory allocator that is compiled into the build.
 */
struct AllocatorConfig {

    /**
     * If using a non-system memory allocator, initialize it to a safe
     * configuration in this runtime environment.
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

    /**
     * Set all allocations in all OMP threads to come from the given memory
     * region of the given size, or restores normal allocator behavior if
     * region is null.
     *
     * Returns true if successful and false if not supported by the allocator.
     *
     * May only be called from the main thread.
     */
    static bool set_arena_area(char* region, size_t size);

};

}
 
#endif
