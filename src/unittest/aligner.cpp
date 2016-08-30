/**
 * unittest/aligner.cpp: test cases for the basic aligner
 */

#include "catch.hpp"
#include "gssw_aligner.hpp"

namespace vg {
namespace unittest {

using namespace std;

TEST_CASE("aligner should left shift within a node", "[alignment][mapping][shifting]") {
    
    // Build a toy graph
    const string graph_json = R"(
    
    {
        "node": [
            {"id": 1, "sequence": "GATGATGATTAAAAACAACAGACTGCTAGCTAACTATTCGAC"},
            {"id": 2, "sequence": "GGACATTGGCACCAAAGCATCATCATAAGAAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA"}
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
    
    SECTION("aligner should left shift homopolymer insertions") {
    
        // Prepare an insert of two bases
        Alignment alignment;
        alignment.set_sequence("GATGATGATTAAAAAAACAACAGACTGCTAGCTAACTATTCGAC");
        
        aligner.align(alignment, chunk);
        
        REQUIRE(alignment.path().mapping_size() == 1);
        auto& mapping = alignment.path().mapping(0);
        REQUIRE(mapping.edit_size() == 3);
        // Edit 0: GATGATGATT to GATGATGATT
        REQUIRE(mapping.edit(0).from_length() == 10);
        REQUIRE(mapping.edit(0).to_length() == 10);
        REQUIRE(mapping.edit(0).sequence() == "");
        
        // Edit 1: - to AA
        REQUIRE(mapping.edit(1).from_length() == 0);
        REQUIRE(mapping.edit(1).to_length() == 2);
        REQUIRE(mapping.edit(1).sequence() == "AA");
        
        // Edit 2: AAAAACAACAGACTGCTAGCTAACTATTCGAC to AAAAACAACAGACTGCTAGCTAACTATTCGAC
        REQUIRE(mapping.edit(2).from_length() == 32);
        REQUIRE(mapping.edit(2).to_length() == 32);
        REQUIRE(mapping.edit(2).sequence() == "");
        
    }
    
    SECTION("aligner should left shift homopolymer deletions") {
    
        // Prepare a deletion of two bases
        Alignment alignment;
        alignment.set_sequence("GATGATGATTAAACAACAGACTGCTAGCTAACTATTCGAC");
        
        aligner.align(alignment, chunk);
        
        REQUIRE(alignment.path().mapping_size() == 1);
        auto& mapping = alignment.path().mapping(0);
        REQUIRE(mapping.edit_size() == 3);
        // Edit 0: GATGATGATT to GATGATGATT
        REQUIRE(mapping.edit(0).from_length() == 10);
        REQUIRE(mapping.edit(0).to_length() == 10);
        REQUIRE(mapping.edit(0).sequence() == "");
        
        // Edit 1: AA to -
        REQUIRE(mapping.edit(1).from_length() == 2);
        REQUIRE(mapping.edit(1).to_length() == 0);
        REQUIRE(mapping.edit(1).sequence() == "");
        
        // Edit 2: AAACAACAGACTGCTAGCTAACTATTCGAC to AAACAACAGACTGCTAGCTAACTATTCGAC
        REQUIRE(mapping.edit(2).from_length() == 30);
        REQUIRE(mapping.edit(2).to_length() == 30);
        REQUIRE(mapping.edit(2).sequence() == "");
        
    }
    
    SECTION("aligner should left shift repeat insertions") {
    
        // Prepare an insertion of two units
        Alignment alignment;
        alignment.set_sequence("GGACATTGGCACCAAAGCATCATCATAAGAAGAAGAAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA");
        
        aligner.align(alignment, chunk);
        
        REQUIRE(alignment.path().mapping_size() == 1);
        auto& mapping = alignment.path().mapping(0);
        REQUIRE(mapping.edit_size() == 3);
        // Edit 0: CATATCAT to CATCATCAT
        REQUIRE(mapping.edit(0).from_length() == 26);
        REQUIRE(mapping.edit(0).to_length() == 26);
        REQUIRE(mapping.edit(0).sequence() == "");
        
        // Edit 1: - to AAGAAG
        REQUIRE(mapping.edit(1).from_length() == 0);
        REQUIRE(mapping.edit(1).to_length() == 6);
        REQUIRE(mapping.edit(1).sequence() == "AAGAAG");
        
        // Edit 2: AAGAAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA to AAGAAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA
        REQUIRE(mapping.edit(2).from_length() == 39);
        REQUIRE(mapping.edit(2).to_length() == 39);
        REQUIRE(mapping.edit(2).sequence() == "");
        
    }
    
    SECTION("aligner should left shift repeat deletions") {
    
        // Prepare a deletion of a unit
        Alignment alignment;
        alignment.set_sequence("GGACATTGGCACCAAAGCATCATCATAAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA");
        
        aligner.align(alignment, chunk);
        
        REQUIRE(alignment.path().mapping_size() == 1);
        auto& mapping = alignment.path().mapping(0);
        REQUIRE(mapping.edit_size() == 3);
        // Edit 0: GGACATTGGCACCAAAGCATCATCAT to GGACATTGGCACCAAAGCATCATCAT
        REQUIRE(mapping.edit(0).from_length() == 26);
        REQUIRE(mapping.edit(0).to_length() == 26);
        REQUIRE(mapping.edit(0).sequence() == "");
        
        // Edit 1: AAG to -
        REQUIRE(mapping.edit(1).from_length() == 3);
        REQUIRE(mapping.edit(1).to_length() == 0);
        REQUIRE(mapping.edit(1).sequence() == "");
        
        // Edit 2: AAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA to AAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA
        REQUIRE(mapping.edit(2).from_length() == 36);
        REQUIRE(mapping.edit(2).to_length() == 36);
        REQUIRE(mapping.edit(2).sequence() == "");
        
    }
}

TEST_CASE("aligner should left shift across nodes", "[alignment][mapping][shifting]") {
    
    // Build a toy graph with a 2-unit known deletion
    const string graph_json = R"(
    
    {
        "node": [
            {"id": 1, "sequence": "GGACATTGGCACCAAAGCATCATCAT"},
            {"id": 2, "sequence": "AAGAAG"},
            {"id": 3, "sequence": "AAGAAG"},
            {"id": 4, "sequence": "TAGTAGTAGTACGTAGCTGGCATCTGA"}
        ],
        "edge": [
            {"from": 1, "to": 2},
            {"from": 1, "to": 3},
            {"from": 2, "to": 3},
            {"from": 3, "to": 4}
        ]
    }
    
    )";
    
    // Load it into Protobuf
    Graph chunk;
    json2pb(chunk, graph_json.c_str(), graph_json.size());
    
    // Make an Aligner to align against it
    Aligner aligner;
    
    SECTION("aligner should left shift repeat insertions") {
    
        // Prepare an insertion of a unit
        Alignment alignment;
        alignment.set_sequence("GGACATTGGCACCAAAGCATCATCATAAGAAGAAGAAGAAGTAGTAGTAGTACGTAGCTGGCATCTGA");
        
        aligner.align(alignment, chunk);
        
        cerr << pb2json(alignment) << endl;
        
        REQUIRE(alignment.path().mapping_size() == 4);
        auto& path = alignment.path();
        
        // Mapping 0 Edit 0: GGACATTGGCACCAAAGCATCATCAT to GGACATTGGCACCAAAGCATCATCAT
        REQUIRE(path.mapping(0).edit_size() == 1);
        REQUIRE(path.mapping(0).edit(0).from_length() == 26);
        REQUIRE(path.mapping(0).edit(0).to_length() == 26);
        REQUIRE(path.mapping(0).edit(0).sequence() == "");
        
        // Mapping 1 Edit 0: - to AAG
        REQUIRE(path.mapping(1).edit_size() == 2);
        REQUIRE(path.mapping(1).edit(0).from_length() == 0);
        REQUIRE(path.mapping(1).edit(0).to_length() == 3);
        REQUIRE(path.mapping(1).edit(0).sequence() == "AAG");
        // Mapping 1 edit 1: AAGAAG to AAGAAG
        REQUIRE(path.mapping(1).edit(1).from_length() == 6);
        REQUIRE(path.mapping(1).edit(1).to_length() == 6);
        REQUIRE(path.mapping(1).edit(1).sequence() == "");
        
        // Mapping 2 edit 0: AAGAAG to AAGAAG
        REQUIRE(path.mapping(2).edit_size() == 1);
        REQUIRE(path.mapping(2).edit(0).from_length() == 6);
        REQUIRE(path.mapping(2).edit(0).to_length() == 6);
        REQUIRE(path.mapping(2).edit(0).sequence() == "");
        
        // Mapping 3 edit 0: TAGTAGTAGTACGTAGCTGGCATCTGA to TAGTAGTAGTACGTAGCTGGCATCTGA
        REQUIRE(path.mapping(3).edit_size() == 1);
        REQUIRE(path.mapping(3).edit(0).from_length() == 27);
        REQUIRE(path.mapping(3).edit(0).to_length() == 27);
        REQUIRE(path.mapping(3).edit(0).sequence() == "");
    }
    
    SECTION("aligner should left shift repeat deletions") {
    
        // Prepare a deletion of 3 units
        Alignment alignment;
        alignment.set_sequence("GGACATTGGCACCAAAGCATCATCATAAGTAGTAGTAGTACGTAGCTGGCATCTGA");
        
        aligner.align(alignment, chunk);
        
        REQUIRE(alignment.path().mapping_size() == 3);
        auto& path = alignment.path();
        
        // Mapping 0 Edit 0: GGACATTGGCACCAAAGCATCATCAT to GGACATTGGCACCAAAGCATCATCAT
        REQUIRE(path.mapping(0).edit_size() == 1);
        REQUIRE(path.mapping(0).edit(0).from_length() == 26);
        REQUIRE(path.mapping(0).edit(0).to_length() == 26);
        REQUIRE(path.mapping(0).edit(0).sequence() == "");
        
        // Mapping 1 Edit 0: AAG to - (having taken the known deletion)
        REQUIRE(path.mapping(1).edit_size() == 2);
        REQUIRE(path.mapping(1).edit(0).from_length() == 3);
        REQUIRE(path.mapping(1).edit(0).to_length() == 0);
        REQUIRE(path.mapping(1).edit(0).sequence() == "");
        // Mapping 1 edit 1: AAG to AAG
        REQUIRE(path.mapping(1).edit(1).from_length() == 3);
        REQUIRE(path.mapping(1).edit(1).to_length() == 3);
        REQUIRE(path.mapping(1).edit(1).sequence() == "");
        
        // Mapping 2 edit 0: TAGTAGTAGTACGTAGCTGGCATCTGA to TAGTAGTAGTACGTAGCTGGCATCTGA
        REQUIRE(path.mapping(2).edit_size() == 1);
        REQUIRE(path.mapping(2).edit(0).from_length() == 27);
        REQUIRE(path.mapping(2).edit(0).to_length() == 27);
        REQUIRE(path.mapping(2).edit(0).sequence() == "");
    }
    
    

}


}
}
