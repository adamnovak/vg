#include "augment.hpp"

void AugmentedGraph::break_end(const Node* orig_node, int offset, bool left_side) {
    
    NodeHash::iterator i = index.find(orig_node->id());
    if (i == index.end()) {
        // We can't break a node that has no representation in the augmented graph
        return;
    }
    
    NodeMap& node_map = i->second;
    NodeMap::iterator j = node_map.upper_bound(offset);
    if (j == node_map.begin()) {
        // We can't break a node at an offset smaller than the start offset of
        // the first existing chunk.
        return;
    }

    // Go from the first node starting after where we want to break to the node
    // starting before where we want to break.
    --j;
    // And get the offse it starts at in the original node.
    int sub_offset = j->first;

    // We need 
    function<Node*(Node*, EntryCat, int)> lambda = [&](Node* fragment, EntryCat cat, int sup) {
        
        if (offset < sub_offset || offset >= sub_offset + fragment->sequence().length()) {
            // This is incorrect. Return null.
            return (Node*) nullptr;
        }

        // if our cut point is already the exact left or right side of the node, then
        // we don't have anything to do than return it.
        if (offset == sub_offset && left_side == true) {
            return fragment;
        }
        if (offset == sub_offset + fragment->sequence().length() - 1 && left_side == false) {
            return fragment;
        }

        // otherwise, we're somewhere in the middle, and have to subdivide the node
        // first, shorten the exsisting node.
        
        // How long should the new node be?
        int new_len = left_side ? offset - sub_offset : offset - sub_offset + 1;
        assert(new_len > 0 && new_len != fragment->sequence().length());
        
        // Get the string sequence of the rest of the node we are cutting.
        string frag_seq = fragment->sequence();
        *fragment->mutable_sequence() = frag_seq.substr(0, new_len);

        // then make a new node for the right part
        Node* new_node = graph->create_node(frag_seq.substr(new_len, frag_seq.length() - new_len), ++(*_max_id));
        add_fragment(orig_node, sub_offset + new_len, new_node, cat, sup);

        return new_node;
    };

    // none of this affects copy number
    int sup_ref = j->second.sup_ref;
    Node* fragment_ref = j->second.ref;
    Node* new_node_ref = fragment_ref != NULL ? lambda(fragment_ref, Ref, sup_ref) : NULL;
    int sup_alt1 = j->second.sup_alt1;
    Node* fragment_alt1 = j->second.alt1;
    Node* new_node_alt1 = fragment_alt1 != NULL ? lambda(fragment_alt1, Alt1, sup_alt1) : NULL;
    int sup_alt2 = j->second.sup_alt2;
    Node* fragment_alt2 = j->second.alt2;
    Node* new_node_alt2 = fragment_alt2 != NULL ? lambda(fragment_alt2, Alt2, sup_alt2) : NULL;

    Entry ret = left_side ? Entry(new_node_ref, sup_ref, new_node_alt1, sup_alt1, new_node_alt2, sup_alt2) :
        Entry(fragment_ref, sup_ref, fragment_alt1, sup_alt1, fragment_alt2, sup_alt2);

    return ret;
}
