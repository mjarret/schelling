#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <utility>
#include <cassert>

class WorldAgentPacked {
public:
    struct Agent { uint32_t pos; uint32_t type; };
private:
    std::vector<Agent>                          agents_;    // compact agent records
    std::vector<std::pair<uint32_t, uint32_t>>  pos_index_; // (vertex, agent_id), sorted by vertex
    uint32_t n_vertices_;

    // Helper: find index entry for vertex v
    inline auto find_vertex(uint32_t v) const {
        return std::lower_bound(pos_index_.begin(), pos_index_.end(), std::make_pair(v, 0u));
    }

public:
    explicit WorldAgentPacked(uint32_t n_vertices, uint32_t reserve_agents = 0)
        : agents_(), pos_index_(), n_vertices_(n_vertices) {
        agents_.reserve(reserve_agents);
        pos_index_.reserve(reserve_agents);
    }

    inline uint32_t num_vertices() const { return n_vertices_; }
    inline uint32_t num_agents()   const { return static_cast<uint32_t>(pos_index_.size()); }

    inline bool occupied(uint32_t v) const {
        auto it = find_vertex(v);
        return (it != pos_index_.end() && it->first == v);
    }

    inline uint32_t type_of(uint32_t v) const {
        auto it = find_vertex(v);
        assert(it != pos_index_.end() && it->first == v);
        return agents_[it->second].type;
    }

    // Removes the agent occupying vertex v (if any), keeping arrays compact
    inline void clear_vertex(uint32_t v) {
        auto it = find_vertex(v);
        if (it == pos_index_.end() || it->first != v) return;

        const uint32_t id       = it->second;
        const uint32_t last_id  = static_cast<uint32_t>(agents_.size() - 1);

        // Remove position index entry for v
        pos_index_.erase(it);

        if (id != last_id) {
            // Swap agent[id] with last agent
            const uint32_t last_pos  = agents_[last_id].pos;
            const uint32_t last_type = agents_[last_id].type;
            agents_[id] = { last_pos, last_type };

            // Update index entry pointing to last_pos: change agent_id to 'id'
            auto it2 = std::lower_bound(pos_index_.begin(), pos_index_.end(), std::make_pair(last_pos, 0u));
            assert(it2 != pos_index_.end() && it2->first == last_pos);
            it2->second = id;
        }

        agents_.pop_back();
    }

    // Creates a new agent at vertex v with given type (used for initialization)
    inline void set_vertex(uint32_t v, uint32_t type) {
        // Precondition: v should be empty
        auto it = find_vertex(v);
        if (it != pos_index_.end() && it->first == v) return; // already occupied
        const uint32_t id = static_cast<uint32_t>(agents_.size());
        agents_.push_back({v, type});
        pos_index_.insert(it, {v, id});
    }

    // Move agent from src->dst (dst must be empty)
    inline void move(uint32_t src, uint32_t dst) {
        auto it = find_vertex(src);
        assert(it != pos_index_.end() && it->first == src);
        const uint32_t id = it->second;

        // Remove old index entry
        pos_index_.erase(it);

        // Update agent record
        agents_[id].pos = dst;

        // Insert new index entry for dst
        auto it2 = std::lower_bound(pos_index_.begin(), pos_index_.end(), std::make_pair(dst, 0u));
        pos_index_.insert(it2, {dst, id});
    }

    template <typename F>
    inline void for_each_agent(F&& f) const {
        for (auto &p : pos_index_) {
            const uint32_t v  = p.first;
            const uint32_t id = p.second;
            f(v, agents_[id].type);
        }
    }
};
