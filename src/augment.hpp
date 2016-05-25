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

// We need to break apart nodes but remember where they came from to update
// edges. Wrap all this up in this class.  For a position in the input graph, we
// can have one or more nodes in the augmented graph (ref, alts), so we map to
// colelctions of nodes. We also annotate all the nodes and edges with how much
// support they have.
struct AugmentedGraph {
    // Represents support on the forward and reverse strands. One gets
    // associated with every node base and every edge.
    struct Support {
        // Forward strand support
        int forward = 0;
        // Reverse strand support
        int reverse = 0;
    };

    // This represents all the nodes created for a run of reference positions: one ref node and all the alt nodes.
    struct Entry {
        Node* ref;
        vector<Support> ref_support;
        vector<Node*> alts;
        vector<vector<Support>> alt_support;
        
        // Make a new Entry for just a ref node
        Entry(Node* ref): ref(ref), ref_support(ref->sequence().size()) {
        }
        
        // Make a new entry with zero support
        Entry(Node* ref, vector<Node*> alts): ref(ref), ref_support(ref->sequence().size()),
            alts(alts), alt_support(alts.size()) {
            
            for(size_t i = 0; i < alts.size(); i++) {
                // Allocate support for each base of each alt.
                alt_support[i] = vector<Support>(alts[i]->sequence().size());
            }
        }
        
        // Add a new alt with no support. Must not be in here already.
        void push_back(Node* alt) {
            alts.push_back(alt);
            alt_support.push_back(vector<Support>(alt->sequence().size()));
        }
        
        // Get the number of nodes here (ref plus alts)
        int size() {
            return 1 + alts.size();
        }
        
        // Get the ith ref or alt node. i must be <= size().
        Node*& operator[](int i) {
            if(i == 0) {
                return ref;
            } else {
                return alts.at(i - 1);
            }
        }
        
        // Get the support for the ith ref or alt node. i must be <= size().
        vector<Support>& sup(int i) {
            if(i == 0) {
                return ref_support;
            } else {
                return alt_support.at(i - 1);
            }
        }
    };
    // Start offset in original graph node -> ref and all alts in augmented graph
    typedef map<int, Entry> NodeMap;
    // Node id in original graph to NodeMap saying what alts are available for each part.
    typedef hash_map<int64_t, NodeMap> NodeHash;
    
    // This stores the embedding of the augmented graph in the original graph,
    // or equivalently the fluffing-up of the original graph into the augmented
    // graph.
    NodeHash index;
    
    // This holds support for all the edges in the augmented graph. When edges
    // are created or destroyed, we need to update this.
    map<Edge*, Support> edge_support;

    // We keep around the augmented graph that holds all the ref and alt nodes
    // we have been pointing to in the index, and all the edges we have support
    // annotations for. Instead of using its divide_node-type methods, we handle
    // all the node and edge rewriting ourselves in order to keep our
    // annotations in sync.
    VG graph;
    
    // We handle our own ID generation because we would like new node IDs to all
    // come after old node IDs.
    id_t next_id;

    // Break an original-graph node (if needed) so that the specified side of
    // the specified base becomes free, so we can attach edges to it. Fixes up
    // the index and preserves all the support counts. Returns the reference
    // Node* on the left of the break if left_side is true, and the one on the
    // right of the break otherwise.
    Node* break_end(const Node* orig_node, int offset, bool left_side);    
    
    // Add an alt node (or add support to the ref node) starting at the given
    // offset in the given original node. Takes care of breaking the reference
    // node, creating (and deduplicating) the alt node, and wiring everything
    // up. Optionally takes some support to either give to that alt node or add
    // to the existing alt node (and its ref-touching edges).
    void add_fragment(const Node* orig_node, int offset, int ref_length, string alt_sequence, Support support = Support());
    
    // Add the given support to an original edge. Adds support only to the ref-
    // ref edge, not any edges created connecting alts to each other.
    void add_edge(const Edge* orig_edge, Support support);
    
    // Create (and/or add support to) an edge between positions on original nodes.
    void add_edge(const Node* orig_from, int from_offset, bool from_start,
        const Node* orig_to, int to_offset, bool to_end, Support support = Support());
    
};

// We can serialize these things for debugging.
ostream& operator<<(ostream& os, const AugmentedGraph::NodeMap& nm);
ostream& operator<<(ostream& os, AugmentedGraph::Entry entry);

}

#endif
