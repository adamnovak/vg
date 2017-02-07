#include "graph_synchronizer.hpp"

namespace vg {

using namespace std;

GraphSynchronizer::GraphSynchronizer(VG& graph) : graph(graph) {
    // Nothing to do!
}

const string& GraphSynchronizer::get_path_sequence(const string& path_name) {
    ting::shared_lock<ting::shared_mutex> guard(whole_graph_mutex);
    cerr << "Getting path for " << path_name << endl;
    return get_path_index(path_name).sequence;
    
    /*return with_path_index<const string&>(path_name, [](const PathIndex& index) {
        return index.sequence;
    });*/
}

    
// We need a function to grab the index for a path
PathIndex& GraphSynchronizer::get_path_index(const string& path_name) {
    // Caller has a read lock on the graph.

    {
        // Get a read lock on the indexes map.
        ting::shared_lock<ting::shared_mutex> indexes_lock(indexes_mutex);
        
        if (indexes.count(path_name)) {
            // If we find it, return it
            indexes.at(path_name);
        }
    
    }
    
    // Otherwise, it wasn't there when we grabbed the read lock. Try again with a write lock
    {
        std::unique_lock<ting::shared_mutex> indexes_lock(indexes_mutex);
        
        if (!indexes.count(path_name)) {
            // Still not already made. Generate it.
            indexes.emplace(piecewise_construct,
                forward_as_tuple(path_name), // Make the key
                forward_as_tuple(graph, path_name, true)); // Make the PathIndex
        }
        return indexes.at(path_name);
        
    }
}

void GraphSynchronizer::update_path_indexes(const vector<Translation>& translations) {
    // Caller already has a writer lock on the graph, so we just need a read
    // lock on the indexes map. This still lets us write to the individual
    // indexes.
    ting::shared_lock<ting::shared_mutex> indexes_lock(indexes_mutex);
    
    for (auto& kv : indexes) {
        // We need to touch every index (IN PLACE!)
        
        // Feed each index all the translations, which it will parse into node-
        // partitioning translations and then apply.
        kv.second.apply_translations(translations);
    }
}

GraphSynchronizer::Lock::Lock(GraphSynchronizer& synchronizer,
    const string& path_name, size_t path_offset, size_t context_bases, bool reflect) : 
    synchronizer(synchronizer), path_name(path_name), path_offset(path_offset), 
    context_bases(context_bases), reflect(reflect) {
    
    // Nothing to do. We've saved all the details on the request.
}

void GraphSynchronizer::Lock::lock() {
    // Now we have to block until a lock is obtained.
    
    if (!locked_nodes.empty()) {
        // We already have a lock
        return;
    }
    
    // What we do is, we lock the locked_nodes set and wait on the condition
    // variable, with the check code being that we read-lock the whole graph,
    // find the subgraph and immediate neighbors and verify none of its nodes
    // are locked, all while holding the read lock. On success, we keep the read
    // lock, while if any nodes conflict we drop the read lock.
    
    // Lock the locked nodes set.
    std::unique_lock<std::mutex> locked_nodes_lock(synchronizer.locked_nodes_mutex);
    
    // Allocate a reader lock for the whole graph, but don't lock it yet. We
    // might need to wait for someone who has one of our nodes to get a write
    // lock on the graph before we can have the node.
    ting::shared_lock<ting::shared_mutex> whole_graph_lock;
    
    // TODO: what we really want is for all the threads to be able to search at
    // once, with just graph read locks, and then come back and lock the node
    // set and see if they can get exclusive ownership of their nodes. But that
    // needs some kind of shared lock supporting condition variable.
    
    synchronizer.wait_for_region.wait(locked_nodes_lock, [&]{
        // Now we have exclusive use of the graph and indexes, and we need to
        // see if anyone else is using any nodes we need.
        
        // Get a read lock on the graph
        ting::shared_lock<ting::shared_mutex> local_whole_graph_lock(synchronizer.whole_graph_mutex);
        
        // Find the center node, at the position we want to lock out from
        NodeSide center = synchronizer.get_path_index(path_name).at_position(path_offset);
        
        // Extract the context around that node
        VG context;
        synchronizer.graph.nonoverlapping_node_context_without_paths(synchronizer.graph.get_node(center.node), context);
        synchronizer.graph.expand_context_by_length(context, context_bases, false, reflect);
        
        // Also remember all the nodes connected to but not in the context,
        // which also need to be locked.
        periphery.clear();
        
        // We set this to false if a node we want is taken
        bool nodes_available = true;
        
        context.for_each_node([&](Node* node) {
            // For every node in the graph
            
            if (synchronizer.locked_nodes.count(node->id())) {
                // Someone else already has this node. So our condition is false
                // and we need to wait.
                nodes_available = false;
                return;
            }
            
            for (auto* edge : synchronizer.graph.edges_from(node)) {
                if (!context.has_node(edge->to())) {
                    // This is connected but not in the actual context graph. So it's on the periphery.
                    
                    if (synchronizer.locked_nodes.count(edge->to())) {
                        // Someone else already has this node. So our condition
                        // is false and we need to wait.
                        nodes_available = false;
                        return;
                    }
                    
                    periphery.insert(edge->to());
                }
            }
            for (auto* edge : synchronizer.graph.edges_to(node)) {
                if (!context.has_node(edge->from())) {
                    // This is connected but not in the actual context graph. So it's on the periphery.
                    
                    if (synchronizer.locked_nodes.count(edge->from())) {
                        // Someone else already has this node. So our condition
                        // is false and we need to wait.
                        nodes_available = false;
                        return;
                    }
                    
                    periphery.insert(edge->from());
                }
            }
        });
        
        if (nodes_available) {
            // We can have the nodes we need. Remember what they are.
            subgraph = std::move(context);
            
            // Hold onto our read lock on the graph after we return
            whole_graph_lock = std::move(local_whole_graph_lock);
        }
        
        // Return whether we were successful
        return nodes_available;
    });

    // Once we get here, we have a read lock on the whole graph, an exclusive
    // lock on the locked nodes set, and nobody else has claimed our nodes. Our
    // graph and periphery have been filled in, so we just have to record our
    // nodes as locked.
    
    for(id_t id : periphery) {
        // Mark the periphery
        synchronizer.locked_nodes.insert(id);
        locked_nodes.insert(id);
    }
    
    subgraph.for_each_node([&](Node* node) {
        // Mark the actual graph
        synchronizer.locked_nodes.insert(node->id());
        locked_nodes.insert(node->id());
    });
    
    // Now we know nobody else can touch those nodes and we can safely release
    // our locks on the main graph and the locked nodes set by letting them
    // leave scope.
}

void GraphSynchronizer::Lock::unlock() {
    // Get a mutex just on the locked node set. We know nobody is modifying
    // these nodes in the graph, since we still represent a lock on them.
    std::unique_lock<std::mutex> locked_nodes_lock(synchronizer.locked_nodes_mutex);
    
    // Release all the nodes
    for (id_t locked : locked_nodes) {
        synchronizer.locked_nodes.erase(locked);
    }
    
    // Clear our locked nodes
    locked_nodes.clear();
    
    // Notify anyone waiting, so they can all check to see if now they can go.
    locked_nodes_lock.unlock();
    synchronizer.wait_for_region.notify_all();
}

VG& GraphSynchronizer::Lock::get_subgraph() {
    if (locked_nodes.empty()) {
        // Make sure we're actually locked
        throw runtime_error("No nodes are locked! Can't get graph!");
    }
    
    return subgraph;
}

vector<Translation> GraphSynchronizer::Lock::apply_edit(const Path& path) {
    // Make sure we have exclusive ownership of the graph itself since we're
    // going to be modifying its data structures. We need a write lock.
    std::unique_lock<ting::shared_mutex> guard(synchronizer.whole_graph_mutex);
    
    for (size_t i = 0; i < path.mapping_size(); i++) {
        // Check each Mapping to make sure it's on a locked node
        auto node_id = path.mapping(i).position().node_id();
        if (!locked_nodes.count(node_id)) {
            throw runtime_error("Cannot edit unlocked node " + to_string(node_id));
        }
    }
    
    // Make all the edits
    vector<Translation> translations = synchronizer.graph.edit_fast(path);
    
    // Lock all the nodes that result from the translations. They're guaranteed
    // to either be nodes we already have or novel nodes with fresh IDs.
    for (auto& translation : translations) {
        // For every translation's to path
        auto& new_path = translation.to();
        
        for (size_t i = 0; i < new_path.mapping_size(); i++) {
            // For every mapping to a node on that path
            auto node_id = new_path.mapping(i).position().node_id();
            
            if (!locked_nodes.count(i)) {
                // If it's not already locked, lock it.
                locked_nodes.insert(i);
                synchronizer.locked_nodes.insert(i);
            }
        }
    }
    
    // Apply the edits to the path indexes
    synchronizer.update_path_indexes(translations);
    
    // Spit out the translations to the caller. Maybe they can use them on their subgraph or something?
    return translations;
}

}





















