#include "graph_hash.hpp"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <utility>

// Hash combine function from Boost.
inline void BoostHashCombineAlternative(uint32_t& h1, uint32_t k1) {
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  k1 *= c1;
  k1 = (k1 << 15) | (k1 >> (32 - 15));
  k1 *= c2;

  h1 ^= k1;
  k1 = (h1 << 13) | (h1 >> (32 - 13));
  h1 = h1 * 5 + 0xe6546b64;
}

// Another hash combine function from Boost.
inline void BoostHashCombine(uint32_t& seed, uint32_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

std::pair<uint32_t, std::vector<uint32_t>> GraphHash(
    const std::vector<std::vector<int>>& G) {
  std::vector<uint32_t> hashes, hashes_prev;
  hashes.reserve(G.size());
  for (int v = 0; v < G.size(); v++) hashes.push_back(G[v].size());

  // Variable that will hold sorted neighbours.
  std::vector<int> sorted(G.size());

  for (int i = 0; i < G.size(); ++i) {
    hashes_prev = hashes;

    // Calculate new hashes.
    for (int v = 0; v < G.size(); ++v) {
      const std::vector<int>& nghbrs = G[v];
      // Sort neighbours on their current hash value.
      std::iota(sorted.begin(), sorted.begin() + G[v].size(), 0);
      std::sort(sorted.begin(), sorted.begin() + G[v].size(),
                [&hashes_prev, &nghbrs](int v1, int v2) {
                  return hashes_prev[nghbrs[v1]] < hashes_prev[nghbrs[v2]];
                });

      // Combine the hash values to create a new hash.
      uint32_t seed = nghbrs.size();
      for (int i = 0; i < nghbrs.size(); i++) {
        int nghb = nghbrs[sorted[i]];
        if (i > 0)
          assert(hashes_prev[nghb] >= hashes_prev[nghbrs[sorted[i - 1]]]);

        BoostHashCombine(seed, hashes_prev[nghb]);
      }
      hashes[v] = seed;
    }
  }
  // Calculate the final hash.
  uint32_t seed = G.size();
  std::iota(sorted.begin(), sorted.end(), 0);
  std::sort(sorted.begin(), sorted.end(),
            [&hashes](int v1, int v2) { return hashes[v1] < hashes[v2]; });
  for (auto v : sorted) BoostHashCombine(seed, hashes[v]);
  return {seed, std::move(hashes)};
}

std::pair<bool, std::vector<int>> GraphHashIsomphismMapping(
    const std::vector<std::vector<int>>& G1,
    const std::vector<std::vector<int>>& G2) {
  if (G1.size() != G2.size()) return {false, {}};
  std::vector<uint32_t> vertex_h1, vertex_h2;
  uint32_t h1, h2;
  std::tie(h1, vertex_h1) = GraphHash(G1);
  std::tie(h2, vertex_h2) = GraphHash(G2);
  if (h1 != h2) return {false, {}};

  // This will hold a mapping of G1 <-> G2.
  std::vector<int> mapping(G1.size());

  // Create two vectors containing vertices 0 .. N - 1
  std::vector<int> sorted1(G1.size()), sorted2;
  std::iota(sorted1.begin(), sorted1.end(), 0);
  sorted2 = sorted1;

  // Sort the vertices on basis of the hashes.
  std::sort(sorted1.begin(), sorted1.end(), [&vertex_h1](int v1, int v2) {
    return vertex_h1[v1] < vertex_h1[v2];
  });
  std::sort(sorted2.begin(), sorted2.end(), [&vertex_h2](int v1, int v2) {
    return vertex_h2[v1] < vertex_h2[v2];
  });

  for (int v = 0; v < G1.size(); ++v) {
    int v1 = sorted1[v];
    int v2 = sorted2[v];

    // Degree of vertices must coincide.
    if (G1[v1].size() != G2[v2].size()) return {false, {}};

    // Hashes must coincide
    if (vertex_h1[v1] != vertex_h2[v2]) return {false, {}};

    mapping[v1] = v2;
  }

  return {true, std::move(mapping)};
}

// Calculates whether two graphs are an isomoprhism based on the given
// mapping.
bool GraphIsomorphism(const std::vector<std::vector<int>>& G1,
                      const std::vector<std::vector<int>>& G2,
                      const std::vector<int>& mapping) {
  if (G1.size() != G2.size() || G1.size() != mapping.size()) return false;
  // Verify that applying the mapping to G1 indeed lands at G2.
  for (int v1 = 0; v1 < G1.size(); ++v1) {
    int v2 = mapping[v1];
    assert(0 <= v2 && v2 < G2.size());

    // Create a set of all neighbours of in terms of the new indexing
    std::set<int> nghbrs1, nghbrs2;
    for (int nghb : G1[v1]) nghbrs1.insert(mapping[nghb]);
    for (int nghb : G2[v2]) nghbrs2.insert(nghb);
    if (nghbrs1 != nghbrs2) return false;
  }

  return true;
}

// Uses a graph hash as heuristic for checking if two graphs are isomporhm.
bool GraphHashIsomorphism(const std::vector<std::vector<int>>& G1,
                          const std::vector<std::vector<int>>& G2) {
  auto [succes, mapping] = GraphHashIsomphismMapping(G1, G2);
  if (!succes) return false;
  return GraphIsomorphism(G1, G2, mapping);
}
