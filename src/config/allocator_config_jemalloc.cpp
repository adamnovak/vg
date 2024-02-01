/**
 * \file
 * Allocator configuration procedure for jemalloc.
 */

#include "allocator_config.hpp"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>
#include <vector>
#include <limits>

#include <omp.h>

#ifdef __APPLE__
    // Turn off renaming from e.g. je_mallctl to mallctl because without this on
    // Mac you end up trying to link _mallctl when you call mallctl for some
    // reason.
#define JEMALLOC_NO_RENAME
#endif
#include <jemalloc/jemalloc.h>

#ifndef __APPLE__
    #define je_mallctl mallctl
#endif

extern "C" {
    // Hackily define symbols that jemalloc actually exports.
    // Somehow it gets a "je_" prefix on these relative to what's in its
    // source.
    // They're also all "local" symbols in the dynamic jemalloc library,
    // meaning we can't link them from outside the library; we need to use
    // static jemalloc if we intend to access these from here.
    
    // We use int here but really this takes an enum type.
    bool je_extent_dss_prec_set(int dss_prec);
    
    // This is really the last enum value
    int dss_prec_limit = 3;

    // These are the globals used to store the human-readable dss priority in
    // addition to what the function controls.
    extern const char *je_opt_dss;
    extern const char *je_dss_prec_names[];
    
    extern bool je_opt_retain;
}

// Stringifier we need for jemalloc from its docs
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

namespace vg {

using namespace std;

void AllocatorConfig::configure() {
    // TODO: this is going to allocate when we don't really maybe want to. But
    // the dynamic linker also allocated; we have to hope we don't upset any
    // existing jemalloc stuff.
    ifstream procfile("/proc/sys/vm/overcommit_memory");
    if (procfile) {
        // We're actually on a Linux system with an overcommit setting.
        // TODO: Can it be changed on Mac?
        
        // We need to work around jemalloc's propensity to run out of memory
        // mappings and fail to allocate, when overcommit is disabled and the
        // number of distinct mappings is capped. See <https://github.com/jemalloc/jemalloc/issues/1328>
        
        // Read the setting
        char overcommit;
        procfile >> overcommit;
        
        if (overcommit == '2') {
            // It is the never-overcommit value.
            
            // Complain to the user
            cerr << "vg [warning]: System's vm.overcommit_memory setting is 2 (never overcommit). "
                << "vg does not work well under these conditions; you may appear to run out of memory with plenty of memory left. "
                << "Attempting to unsafely reconfigure jemalloc to deal better with this situation." << endl;
            
            // Try some stuff that may help
            
            // Configure the allocator to prefer sbrk() if it can because memory mapping will cause trouble
            const char* dss_str = "primary";
            size_t dss_str_len = strlen(dss_str);
            
            bool match = false;
            // Redo the dss_prec loop from jemalloc: <https://github.com/jemalloc/jemalloc/blob/83f3294027952710f35014cff1cffd51f281d785/src/jemalloc.c#L1194-L1208>
            // This should cover newly created arenas.
            for (int i = 0; i < dss_prec_limit; i++) {
                if (strncmp(je_dss_prec_names[i], dss_str, dss_str_len) == 0) {
                    if (je_extent_dss_prec_set(i)) {
                        cerr << "Could not reconfigure jemalloc dss_prec" << endl;
                        exit(1);
                    } else {
                        je_opt_dss = je_dss_prec_names[i];
                        match = true;
                        break;
                    }
                }
            }
            if (!match) {
                cerr << "Could not find jemalloc dss_prec of " << dss_str << endl;
                exit(1);
            }
            // Then fix up all existing arenas (without allocating?)
            // To write these string parameters we need to copy a pointer into place, not a value
            const char** dss_str_location = &dss_str; 
            auto mallctl_result = je_mallctl("arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".dss", nullptr, nullptr, (void*) dss_str_location, sizeof(dss_str_location));
            if (mallctl_result) {
                cerr << "Could not set dss priority on existing jemalloc arenas: " << strerror(mallctl_result) << endl;
                exit(1);
            }
            
            // Finally, make the opt_retain flag be off.
            // This seems most likely to upset jemalloc because it changes the semantics of some of its internal fields.
            je_opt_retain = false;
        }
        
    }
}

void AllocatorConfig::set_profiling(bool should_profile) {
    // Send the bool right into jemalloc's profiling-is-active flag.
    //
    // You need to start vg with something like
    // MALLOC_CONF="prof_active:false,prof:true" for this to be useful.
    auto mallctl_result = je_mallctl("prof.active", nullptr, nullptr, &should_profile, sizeof(should_profile));
    if (mallctl_result && should_profile) {
        static bool warned = false;
        if (!warned) {
            // Tell the user once if we wanted to profile but can't.
            std::cerr << "warning[AllocatorConfig::set_profiling]: Memory profiling not available" << std::endl;
            warned = true;
        }
    }
}

void AllocatorConfig::snapshot() {
    // Ask to dump a profile now.
    //
    // You need to start vg with something like
    // MALLOC_CONF="prof_prefix:jeprof.out" for this to have a filename to go
    // to.
    auto mallctl_result = je_mallctl("prof.dump", NULL, NULL, NULL, 0);
    // Ignore any errors since profiling may not be enabled this run. 
}

/**
 * jemalloc arena hooks struct that also keeps track of where we are meant to
 * allocate from and our own allocator state. Allocates all the extents from
 * one contiguous pre-made block of memory, and fails when it is depeleted.
 *
 * "All operations except allocation can be universally opted out of by setting
 * the hook pointers to NULL", say the jemalloc docs, so we implement the
 * world's worst extent manager that only allocates.
 */
struct MemoryBlockExtentHooks : public extent_hooks_t {
    /**
     * Make a new MemoryBlockExtentHooks on the given block with the given size.
     */
    MemoryBlockExtentHooks(void* region, size_t size) : region(region), size(size), cursor(0) {
        // Set all the hook function pointers
        this->alloc = &MemoryBlockExtentHooks::alloc_hook;
        this->dalloc = nullptr;
        this->destroy = nullptr;
        this->commit = nullptr;
        this->decommit = nullptr;
        this->purge_lazy = nullptr;
        this->purge_forced = nullptr;
        this->split = nullptr;
        this->merge = nullptr;
    }

    /**
     * Actual jemalloc extent allocation hook.
     *
     * Returns an address to a block of memory of the given size, aligned to a
     * multiple of the given alignment.
     *
     * If zero points to true, must zero the memory. If commit points to true,
     * must commit the memory.
     *
     * If new_addr is set, must put the memory at new_addr or fail.
     *
     * On failure, returns null and does not write to zero or commit.
     *
     * On success, returns the block address and sets zero to true or false
     * depending on if it was zeroed and commit to true or fasle depending on
     * if it is committed.
     */
    static void* alloc_hook(extent_hooks_t* extent_hooks, void* new_addr, size_t size, size_t alignment, bool* zero, bool* commit, unsigned arena_ind) {
#ifdef debug
        std::cerr << "Extent hook called to allocate " << size << " bytes aligned on " << alignment << " at " << new_addr << std::endl;
#endif

        // Find ourselves 
        MemoryBlockExtentHooks* self = (MemoryBlockExtentHooks*) extent_hooks;
        
        if (new_addr) {
            // Don't bother checking against overlaps, but do check if this is in our region.
            if ((intptr_t) new_addr < (intptr_t) self->region || ((intptr_t) new_addr) + size > ((intptr_t) self->region) + self->size) {
                // This block would go out of the region
#ifdef debug
                std::cerr << "Does not fit in " << self->region << " size " << self->size << std::endl;
#endif
                return nullptr;
            }

            // Mark used through the end of this region
            self->cursor = std::max(self->cursor,  ((intptr_t) new_addr) + size - ((intptr_t) self->region));

            // Zero and commit (we assume everything is committed)
            memset(new_addr, 0, size);
            *zero = true;
            *commit = true;
#ifdef debug
            std::cerr << "Allocated at " << new_addr << std::endl;
#endif
            return new_addr;
        }

        // Otherwise we need to find an address. Put it after the cursor with the provided alignment spacing.
        intptr_t next_free = (intptr_t) self->region + self->cursor;
        size_t alignment_offset = (alignment - (next_free % alignment)) % alignment;
        
        // Fall back on the case where an address is known. It will check if it falls in the region or not.
        return alloc_hook(extent_hooks, (void*)(next_free + alignment_offset), size, alignment, zero, commit, arena_ind);
    }

    /// Base address of the memory region we are managing.
    void* region;
    /// Number of bytes in the region
    size_t size;
    /// Number of bytes used from the region
    size_t cursor;
};

static std::vector<size_t> normal_thread_arena_numbers;

bool AllocatorConfig::set_arena_area(char* region, size_t size) {
    if (region) {
        // Setting up
        if (!normal_thread_arena_numbers.empty()) {
            // One of these is already active
            return false;
        }

        // We need the extent hooks to live somewhere, so sneak them in at the beginning of the region.
        // TODO: Do we need to worry about unaligned access or something?
        if (size < sizeof(MemoryBlockExtentHooks)) {
            return false;
        }
        
        // Account for the hooks at the start of the region
        MemoryBlockExtentHooks* extent_hooks_address = (MemoryBlockExtentHooks*) region;
        void* managed_region_address = (void*)(extent_hooks_address + 1);
        size_t managed_region_size = size - sizeof(MemoryBlockExtentHooks);

        {
            // Check the main thread arena
            unsigned arena_num;
            size_t arena_num_size = sizeof(arena_num);
            auto mallctl_result = je_mallctl("thread.arena", &arena_num, &arena_num_size, nullptr, 0);
            if (mallctl_result) {
                #pragma omp critical (cerr)
                std::cerr << "Could not get thread arena: " << strerror(mallctl_result) << std::endl;
                exit(1);
            }
#ifdef debug
            std::cerr << "Main thread usually uses arena " << arena_num << std::endl;
#endif
        }

        // Get all the arenas for all the threads.
        size_t thread_count = omp_get_num_threads();
        normal_thread_arena_numbers.resize(thread_count);
        #pragma omp parallel
        {
            // Read each thread's arena number
            unsigned arena_num;
            size_t arena_num_size = sizeof(arena_num);
            auto mallctl_result = je_mallctl("thread.arena", &arena_num, &arena_num_size, nullptr, 0);
            if (mallctl_result) {
                #pragma omp critical (cerr)
                std::cerr << "Could not get thread arena: " << strerror(mallctl_result) << std::endl;
                exit(1);
            }
            // And store it
            normal_thread_arena_numbers[omp_get_thread_num()] = arena_num;

#ifdef debug
            #pragma omp critical (cerr)
            std::cerr << "Thread " << omp_get_thread_num() << " usually uses arena " << arena_num << std::endl;
#endif
        }

        // Placement new the hooks into the block
        new (extent_hooks_address) MemoryBlockExtentHooks(managed_region_address, managed_region_size);

        // And attach them to a new arena
        unsigned arena_num = std::numeric_limits<unsigned>::max();
        size_t arena_num_size = sizeof(arena_num);
        extent_hooks_t* extent_hooks_pointer = (extent_hooks_t*) extent_hooks_address;
        size_t extent_hooks_pointer_size = sizeof(extent_hooks_t*);
        auto mallctl_result = je_mallctl("arenas.create", &arena_num, &arena_num_size, &extent_hooks_pointer, extent_hooks_pointer_size);
        if (mallctl_result) {
            std::cerr << "Could not create arena: " << strerror(mallctl_result) << std::endl;
            exit(1);
        }
        assert(arena_num != std::numeric_limits<unsigned>::max());

        if (arena_num_size != sizeof(arena_num)) {
            // Arena creation failed.
            std::cerr << "Arena creation failed without error" << std::endl;
            exit(1);
        }

#ifdef debug
        std::cerr << "Created arena " << arena_num << std::endl;
#endif

        // And tell all the threads to use it
        #pragma omp parallel
        {
            auto mallctl_result = je_mallctl("thread.arena", nullptr, nullptr, &arena_num, arena_num_size);
            if (mallctl_result) {
                #pragma omp critical (cerr)
                std::cerr << "Could not set thread arena: " << strerror(mallctl_result) << std::endl;
                exit(1);
            }
#ifdef debug
            #pragma omp critical (cerr)
            std::cerr << "Thread " << omp_get_thread_num() << " should now use arena " << arena_num << std::endl;
#endif
            mallctl_result = je_mallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
            if (mallctl_result) {
                #pragma omp critical (cerr)
                std::cerr << "Could not flush thread cache: " << strerror(mallctl_result) << std::endl;
                exit(1);
            }
        }

        return true;
    } else {
        // Turning off. Anything we allocate will still be in the arena.

        if (normal_thread_arena_numbers.size() != omp_get_num_threads()) {
            throw std::runtime_error("Trying to turn off arena but we don't have the right original arenas for our threads");
        }

        #pragma omp parallel
        {
            // Set each thread's arena number
            unsigned arena_num = normal_thread_arena_numbers[omp_get_thread_num()];
            size_t arena_num_size = sizeof(arena_num);
            auto mallctl_result = je_mallctl("thread.arena", nullptr, nullptr, &arena_num, arena_num_size);
            if (mallctl_result) {
                #pragma omp critical (cerr)
                std::cerr << "Could not set thread arena: " << strerror(mallctl_result) << std::endl;
                exit(1);
            }

#ifdef debug
            #pragma omp critical (cerr)
            std::cerr << "Thread " << omp_get_thread_num() << " should now use arena " << arena_num << std::endl;
#endif

            mallctl_result = je_mallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
            if (mallctl_result) {
                #pragma omp critical (cerr)
                std::cerr << "Could not flush thread cache: " << strerror(mallctl_result) << std::endl;
                exit(1);
            }
        }

        normal_thread_arena_numbers.clear();

        return true;
    }
}

}
 
