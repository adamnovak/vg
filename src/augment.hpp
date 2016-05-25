#ifndef VG_AUGMENT_H
#define VG_AUGMENT_H

#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <tuple>
#include "vg.pb.h"
#include "vg.hpp"
#include "hash_map.hpp"
#include "utility.hpp"
#include "pileup.hpp"

namespace vg {

using namespace std;

// An AugmentedGraph wraps a VG graph that is partly embedded in another vg
// graph, with only deletions, pure insertions, and SNPs being added on top.
// Given a node and offset in the original graph, we can find where that lands
// in the new graph. We also track support for nodes and edges in the augmented
// graph, in terms of forward and reverse strand reads.
struct AugmentedGraph {
    // Represents support on the forward and reverse strands. One gets
    // associated with every node base and every edge.
    struct Support {
        // Forward strand support
        int forward = 0;
        // Reverse strand support
        int reverse = 0;
    };

    // This holds the actual augmented graph, which starts out as a copy of the
    // original graph.
    VG graph;
    
    // Map from original graph ID and then offset along the original node to the
    // new split-up node in the augmented graph.
    map<id_t, map<int, Node*>> index;
    
    // We want some way to know if edges are reference (and we should give
    // support to them when we give support to the bases before them) or not.
    // This is to handle the ref edge that would be opposite an insert on the
    // last base of a node with multiple edges out of its end. For this we just
    // refer back to the original graph, which can tell us what edges are on a
    // node, and we can find the corresponding edges in our graph and follow
    // them. We also use this to know where to connect a SNP that lands at the
    // start or end of a node.
    const VG& original;

    // Make an AugmentedGraph from an original VG graph, which gets copied to
    // create the augmented version.
    AugmentedGraph(const VG& original);
    
    // Divide a node from the original graph so as to free the given side (left
    // or right) for edge connection. Calling this invalidates any stored
    // support values.
    void divide_node(id_t original_id, int pos, bool is_left);
    
    // Get the NodeSide in the augmented graph that the given side of the given
    // original base now belongs to, or raise an exception if no cut has been
    // made to produce such a side.
    NodeSide find_side(id_t original_id, int pos, bool is_left);
    
    // Get the list of NodeSides in the augmented graph that correspond to the
    // ends of nodes that were attached to the given side of the given base in
    // the original graph. Raises an exception if that side of that base was not
    // at the edge of a node in the original graph. Used to determine which
    // other nodes to go to after an insert at the end of a node, or
    // after/before a SNP at the end/start of a node.
    vector<NodeSide> find_originally_attached_sides(id_t original_id, int pos, bool is_left);
    
    // Add support for a novel, deletion edge. Adds the edge if it doesn't exist.
    void add_support_for_deletion_edge(id_t from, int from_offset, bool from_start, id_t to, int to_offset, bool to_end, Support support);
    
    
    // Add support for the given base of the given original node, and for all
    // reference edges that come after it (if any). It's a bit wonky because it
    // duplicates support multiple ways going e.g. into a SNP, but since when we
    // do the actual calling we only care about support for edges opposite
    // inserts and edges that are deletions, it should work out. Needs to be
    // called for anything that isn't an insert.
    void add_support_for_base_and_subsequent_edges(id_t original_id, int pos, Support support);
    
    // Call this instead of the above for inserts (TODO: and deletes? Do they delete the base they land on or not?)
    void add_support_for_base_and_subsequent_edges(id_t original_id, int pos, Support support);
    
    // Add an insert. Can't call multiple times, because that will make
    // multiple inserts. You need to aggregate support beforehand.
    void add_insert_with_support(id_t original_id, int pos, string inserted, Support support);
    
    
    // Like above, but for a SNP
    void add_snp_with_support(id_t original_id, int pos, string snp, Support support);
    
    // TODO: function to take a pileup, make cuts, scan it again, aggregate SNPs
    // and inserts, and then throw in all the new stuff with its support/support
    // for old stuff.
    
};

}

#endif
