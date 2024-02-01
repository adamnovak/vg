/// \file shared_arena.cpp
///  
/// Unit tests for shared memory arena
///

#include <iostream>
#include <vector>

#include "../config/allocator_config.hpp"

#include "../shared_arena.hpp"

#include "catch.hpp"

namespace vg {

namespace unittest {
using namespace std;
    
TEST_CASE("Allocation in a shared memory arena works", "[arena]") {

    AllocatorConfig::arena_hook_t set_arena_area = AllocatorConfig::get_arena_hook();

    // The arena has a min size of a few megabytes.
    const size_t ARENA_SIZE = 4 * 1024 * 1024;
    char arena_mem[ARENA_SIZE];

    bool opened = set_arena_area(arena_mem, ARENA_SIZE);

    void* allocated = malloc(10);
    if (allocated) {
        free(allocated);
    }

    bool closed = set_arena_area(nullptr, 0);

    REQUIRE(opened == true);
    REQUIRE(closed == true);

    REQUIRE((intptr_t) allocated >= (intptr_t) arena_mem);
    REQUIRE((intptr_t) allocated < (intptr_t) arena_mem + ARENA_SIZE);
    
}

TEST_CASE("SharedArena can be created and destroyed", "[arena]") {

    AllocatorConfig::arena_hook_t set_arena_area = AllocatorConfig::get_arena_hook();

    bool entered = false;
    {
        SharedArena the_arena("/thearena", 10 * 1024 * 1024, set_arena_area);
        entered = the_arena.enter();
        if (entered) {
            the_arena.leave();
        }
    }
    REQUIRE(entered == true);
    
}

TEST_CASE("SharedArena can be connected", "[arena]") {

    AllocatorConfig::arena_hook_t set_arena_area = AllocatorConfig::get_arena_hook();

    bool entered = false;
    {
        SharedArena the_arena("/thearena", 10 * 1024 * 1024, set_arena_area);
        entered = the_arena.enter();
        if (entered) {
            std::string* secret_str = new std::string("This is a secret message");
            the_arena.save_named_value("message", (const void*) secret_str);
            the_arena.leave();
        }

        SharedArena the_other_arena("/thearena");
        const std::string* got_str = (const std::string*) the_other_arena.load_named_value("message");
        REQUIRE(got_str != nullptr);
        REQUIRE(*got_str == "This is a secret message");
    }
    REQUIRE(entered == true);
    
}

}
}
        

