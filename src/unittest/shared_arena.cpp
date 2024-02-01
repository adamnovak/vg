/// \file shared_arena.cpp
///  
/// Unit tests for shared memory arena
///

#include <iostream>
#include <vector>

#include "../config/allocator_config.hpp"

#include "catch.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
    SharedArena(const std::string& path, size_t size);

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
     * thread safe with itself or get_named_value(). Can only be used while the
     * arena is entered.
     */
    void save_named_value(const std::string& name, const void* pointer);

    /**
     * Get a pointer to something in the shared memory by name, or null if
     * nothing is saved with that name.
     */
    const void* get_named_value(const std::string& name) const;

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

};

SharedArena::SharedArena(const std::string& path, size_t size) : path(path), shm_fd(0), size(size), mapped_address(nullptr), can_enter(true), is_entered(false) {
    // We need to create this area.

    if (size < sizeof(void*) + sizeof(std::unordered_map<std::string, const void*>*)) {
        // No room for mapping address and name index
        throw std::runtime_error("Size is too small");
    }

    // Create the shared memory object
    shm_fd = shm_open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("Cannot open shared memory");
        exit(1);
    }
    
    // Set it to be big enough
    if (ftruncate(shm_fd, size) == -1) {
        perror("Cannot set shared memory size");
        exit(1);
    }
	
    // Map it at some arbitrary address
    mapped_address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mapped_address == MAP_FAILED) {
		perror("Cannot map shared memory");
        exit(1);
	}

    // Save the mapping address to the beginning of the object so we can tell where to map it in readers.
    *(void**) mapped_address = mapped_address;
    // Note we have no name table
    *(std::unordered_map<std::string, const void*>**)(((void**) mapped_address) + 1) = nullptr;
    
}

SharedArena::SharedArena(const std::string& path) : path(), shm_fd(0), size(0), mapped_address(nullptr), can_enter(false), is_entered(false) {
    // We need to connect to this existing area
    
    // Open the existing object read-only
    shm_fd = shm_open(path.c_str(), O_RDONLY, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("Cannot open shared memory");
        exit(1);
    }

    // Get its size
    struct stat stat_buf;
    if (fstat(shm_fd, &stat_buf) != 0) {
        perror("Cannot get size of shared memory");
        exit(1);
    }
    size = stat_buf.st_size;

    if (size < sizeof(void*) + sizeof(std::unordered_map<std::string, const void*>*)) {
        // No room for mapping address and name index
        throw std::runtime_error("Size is too small");
    }

    // Map it at some arbitrary address
    mapped_address = mmap(nullptr, size, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (mapped_address == MAP_FAILED) {
		perror("Cannot map shared memory");
        exit(1);
	}
    // Figure out where it *wants* to be.
    void* requested_address = *(void**) mapped_address;

    // Unmap it from here and map it there instead.
    if (munmap(mapped_address, size) != 0) {
        perror("Cannot unmap shared memory");
        exit(1);
    }

    int flags = MAP_SHARED;
#ifdef __APPLE__
    // TODO: Implement a bunch of Mach port stuff to poll the address range to make sure it is free.
    // See <https://jvns.ca/blog/2018/01/26/mac-memory-maps/>
    flags |= MAP_FIXED;
#else
    // We are Linux so we should be able to use the safety check
    flags |= MAP_FIXED_NOREPLACE;
#endif
    mapped_address = mmap(requested_address, size, PROT_READ, flags, shm_fd, 0);
    if (mapped_address == MAP_FAILED) {
		perror("Cannot map shared memory at requested address");
        exit(1);
	}
}

SharedArena::~SharedArena() {
    if (mapped_address) {
        if (munmap(mapped_address, size) != 0) {
            perror("Cannot unmap shared memory");
            exit(1);
        }
        mapped_address = nullptr;
    }

    if (shm_fd != 0) {
        if (close(shm_fd) != 0) {
            perror("Cannot close shared memory");
            exit(1);
        }
        shm_fd = 0;
    }

    if (!path.empty()) {
        // We are the one responsible for removing the name.
        if (shm_unlink(path.c_str()) != 0) {
            perror("Cannot unlink shared memory");
            exit(1);
        }
    }
}

bool SharedArena::enter() {
    if (!can_enter) {
        return false;
    }
    // The arena comes after the address pointer and the name table pointer.
    char* arena_mem = (char*)((std::unordered_map<std::string, const void*>**)(((void**) mapped_address) + 1) + 1);
    // And uses all the bytes except for those.
    size_t arena_size = size - sizeof(void*) - sizeof(std::unordered_map<std::string, const void*>*);

    if (AllocatorConfig::set_arena_area(arena_mem, arena_size)) {
        // We entered.
        can_enter = false;
        is_entered = true;

        // Allocate the name table in the arena
        std::unordered_map<std::string, const void*>*& name_table = *(std::unordered_map<std::string, const void*>**)(((void**) mapped_address) + 1);
        name_table = new std::unordered_map<std::string, const void*>();

        if (name_table == nullptr) {
            is_entered = false;
            if (!AllocatorConfig::set_arena_area(nullptr, 0)) {
                throw std::runtime_error("Failed to leave arena");
            }
            throw std::runtime_error("Cannot allocate name table");
        }

        return true;
    } else {
        // We can't do the arena
        return false;
    }
}


void SharedArena::leave() {
    if (!is_entered) {
        throw std::runtime_error("Cannot leave arena that is not entered");
    }
    if (!AllocatorConfig::set_arena_area(nullptr, 0)) {
        throw std::runtime_error("Failed to leave arena");
    }
    is_entered = false;
}


void SharedArena::save_named_value(const std::string& name, const void* pointer) {
    if (!is_entered) {
        throw std::runtime_error("Cannot save value in arena that is not entered");
    }
    std::unordered_map<std::string, const void*>*& name_table = *(std::unordered_map<std::string, const void*>**)(((void**) mapped_address) + 1);
    if (name_table == nullptr) {
        throw std::runtime_error("Cannot save value because name table is null");
    }
    (*name_table)[name] = pointer;
}


const void* SharedArena::get_named_value(const std::string& name) const {
    std::unordered_map<std::string, const void*>*& name_table = *(std::unordered_map<std::string, const void*>**)(((void**) mapped_address) + 1);
    if (name_table == nullptr) {
        throw std::runtime_error("Cannot get value because name table is null");
    }
    auto it = name_table->find(name);
    if (it == name_table->end()) {
        // Not in there
        return nullptr;
    }
    return it->second;
}


namespace unittest {
using namespace std;
    
TEST_CASE("Allocation in a shared memory arena works", "[arena]") {

    // The arena has a min size of a few megabytes.
    const size_t ARENA_SIZE = 4 * 1024 * 1024;
    char arena_mem[ARENA_SIZE];

    bool opened = AllocatorConfig::set_arena_area(arena_mem, ARENA_SIZE);

    void* allocated = malloc(10);
    if (allocated) {
        free(allocated);
    }

    bool closed = AllocatorConfig::set_arena_area(nullptr, 0);

    REQUIRE(opened == true);
    REQUIRE(closed == true);

    REQUIRE((intptr_t) allocated >= (intptr_t) arena_mem);
    REQUIRE((intptr_t) allocated < (intptr_t) arena_mem + ARENA_SIZE);
    
}

TEST_CASE("SharedArena can be created and destroyed", "[arena]") {

    bool entered = false;
    {
        SharedArena the_arena("/thearena", 10 * 1024 * 1024);
        entered = the_arena.enter();
        if (entered) {
            the_arena.leave();
        }
    }
    REQUIRE(entered == true);
    
}

TEST_CASE("SharedArena can be connected", "[arena]") {

    bool entered = false;
    {
        SharedArena the_arena("/thearena", 10 * 1024 * 1024);
        entered = the_arena.enter();
        if (entered) {
            std::string* secret_str = new std::string("This is a secret message");
            the_arena.save_named_value("message", (const void*) secret_str);
            the_arena.leave();
        }

        SharedArena the_other_arena("/thearena");
        const std::string* got_str = (const std::string*) the_other_arena.get_named_value("message");
        REQUIRE(got_str != nullptr);
        REQUIRE(*got_str == "This is a secret message");
    }
    REQUIRE(entered == true);
    
}

}
}
        

