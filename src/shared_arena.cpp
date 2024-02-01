/**
 * \file shared_arena.hpp
 * Implementations for shared memory arena
 */


#include "shared_arena.hpp"

#include "config/allocator_config.hpp"

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace vg {

SharedArena::SharedArena(const std::string& path, size_t size, AllocatorConfig::arena_hook_t set_arena_area) : path(path), shm_fd(0), size(size), mapped_address(nullptr), can_enter(true), is_entered(false), set_arena_area(set_arena_area) {
    // We need to create this area.

    if (size < sizeof(void*) + sizeof(std::unordered_map<std::string, const void*>*)) {
        // No room for mapping address and name index
        throw std::runtime_error("Size is too small");
    }

    // Try unlinking first
    shm_unlink(path.c_str());

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

    if (this->set_arena_area(arena_mem, arena_size)) {
        // We entered.
        can_enter = false;
        is_entered = true;

        // Allocate the name table in the arena
        std::unordered_map<std::string, const void*>*& name_table = *(std::unordered_map<std::string, const void*>**)(((void**) mapped_address) + 1);
        name_table = new std::unordered_map<std::string, const void*>();

        if (name_table == nullptr) {
            is_entered = false;
            if (!this->set_arena_area(nullptr, 0)) {
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
    if (!this->set_arena_area(nullptr, 0)) {
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


const void* SharedArena::load_named_value(const std::string& name) const {
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

}
