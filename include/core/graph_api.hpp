#pragma once
/*
Graph API (informal concept)

A graph type G must provide:
  using index_type = std::size_t;

  std::size_t num_vertices() const;
  std::size_t degree(std::size_t v) const; // degree of vertex v

  template<class F>
  void for_each_neighbor(std::size_t v, F&& f) const;
    // Calls f(u) for each neighbor u of v. No allocations; order irrelevant.

Construction and size semantics are graph-specific:
 - Torus: size = WxH (wrap-around).
 - Lollipop: size = m:L (clique size m, path length L; total N = m+L).
*/
