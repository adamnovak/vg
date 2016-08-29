/**
 * unittest/aligner.cpp: test cases for the basic aligner
 */

#include "catch.hpp"
#include "gssw_aligner.hpp"

namespace vg {
namespace unittest {

using namespace std;

TEST_CASE("aligner should left shift within a node", "[alignment][mapping]") {
    
    // Build a toy graph
    const string graph_json = R"(
    
    {
        "node": [
            {"id": 1, "sequence": "GATTAAAAACA"}
        ],
        "edge": [
        ]
    }
    
    )";
    
    // Load it into Protobuf
    Graph chunk;
    json2pb(chunk, graph_json.c_str(), graph_json.size());
    
    // Make an Aligner to align against it
    Aligner aligner;
    
    SECTION("aligner should left shift insertions") {
    
        // Prepare an insert of two bases
        Alignment alignment;
        alignment.set_sequence("GATTAAAAAAACA");
        
        aligner.align(alignment, chunk);
        
        REQUIRE(alignment.path().mapping_size() == 1);
        auto& mapping = alignment.path().mapping(0);
        REQUIRE(mapping.edit_size() == 3);
        // Edit 0: GATT to GATT
        REQUIRE(mapping.edit(0).from_length() == 4);
        REQUIRE(mapping.edit(0).to_length() == 4);
        REQUIRE(mapping.edit(1).sequence() == "");
        
        // Edit 1: - to AA
        REQUIRE(mapping.edit(1).from_length() == 0);
        REQUIRE(mapping.edit(1).to_length() == 2);
        REQUIRE(mapping.edit(1).sequence() == "AA");
        
        // Edit 2: AAAAAAACA to AAAAAAACA
        REQUIRE(mapping.edit(3).from_length() == 9);
        REQUIRE(mapping.edit(9).to_length() == 9);
        REQUIRE(mapping.edit(1).sequence() == "");
        
    }
    
    

}


}
}
