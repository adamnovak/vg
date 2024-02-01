#ifndef VG_SHARED_ARENA_HPP_INCLUDED
#define VG_SHARED_ARENA_HPP_INCLUDED
/**
 * \file shared_arena.hpp
 * Shared memory arena for allocating in and sharing with other processes.
 */

#include <string>
#include <config/allocator_config.hpp>

namespace vg {

/**
 * Memory allocation arena in shared memory.
 */
class SharedArena {

public:
    // Cannot be copied or moved.
    SharedArena(const SharedArena& other) = delete;
    SharedArena(SharedArena&& other) = delete;
    SharedArena& operator=(const SharedArena& other) = delete;
    SharedArena& operator=(SharedArena&& other) = delete;

    /**
     * Make a new SharedArena we can enter.
     */
    SharedArena(const std::string& path, size_t size, AllocatorConfig::arena_hook_t set_arena_area);

    /**
     * Constructor to open existing shared memory at a path.
     */
    explicit SharedArena(const std::string& path);

    /**
     * Destroy the SharedArena and close out the shared memory.
     * Everything in it must have been destroyed!
     */
    ~SharedArena();

    /**
     * Enter the SharedArena so all OMP threads will allocate from it
     * until leave() is called.
     *
     * Returns false if this is not possible/not implemented.
     */
    bool enter();

    /**
     * Exit the shared memory arena and stop allocating from it. It cannot be
     * re-entered.
     */
    void leave();

    /**
     * Save a pointer to something in the shared memory under a name. Not
     * thread safe with itself or load_named_value(). Can only be used while the
     * arena is entered.
     */
    void save_named_value(const std::string& name, const void* pointer);

    /**
     * Get a pointer to something in the shared memory by name, or null if
     * nothing is saved with that name.
     */
    const void* load_named_value(const std::string& name) const;

private:

    /// String name of the shared memory region, for removing it. Only set if
    /// we created the object.
    std::string path;

    /// File descriptor of the open shared memory object.
    int shm_fd;

    /// Number of bytes in the shared memory region.
    size_t size;

    /// Where the shared memory is mapped
    void* mapped_address;

    /// Flag for if we may enter the arena.
    bool can_enter;

    /// Flag for if the arena is entered.
    bool is_entered;

    /// Hook to the allocator for setting the arena. Not set if not enterable.
    AllocatorConfig::arena_hook_t set_arena_area;

};

}

#endif
