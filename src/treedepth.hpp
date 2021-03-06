#pragma once
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>

#include "centrality.hpp"
#include "exact_cache.hpp"
#include "graph.hpp"
#include "separator.hpp"
#include "set_trie.hpp"
#include "treedepth_tree.hpp"

// Trivial treedepth implementation, useful for simple sanity checks.
int treedepth_trivial(const Graph &G) {
  if (G.N == 1) return 1;
  int td = G.N;
  for (int v = 0; v < G.N; ++v) {
    int td_for_this_root = 0;
    for (auto cc : G.WithoutVertex(v)) {
      td_for_this_root = std::max(td_for_this_root, treedepth_trivial(cc) + 1);
    }
    td = std::min(td, td_for_this_root);
  }
  return td;
}

// This function returns the treedepth and root of a Graph using simple
// heuristics. This only works for special graphs. It returns -1, -1, if
// no such exact heuristic is found.
std::pair<int, int> treedepth_exact(const Graph &G) {
  int N = G.N;

  // Do a quick check for special cases for which we know the answer.
  if (G.IsCompleteGraph()) {
    return {N, G.global[0]};
  } else if (G.IsStarGraph()) {
    // Find node with max_degree.
    for (int v = 0; v < G.N; ++v)
      if (G.Adj(v).size() == G.max_degree) return {2, G.global[v]};
  } else if (G.IsCycleGraph()) {
    // Find the bound, 1 + td of path of length N - 1.
    N--;
    int bnd = 2;
    while (N >>= 1) bnd++;
    return {bnd, G.global[0]};
  } else if (G.IsPathGraph()) {
    // Find the bound, this is the ceil(log_2(N)).
    int bnd = 1;
    while (N >>= 1) bnd++;

    // Find a leaf and then find the middle node.
    for (int v = 0; v < G.N; ++v)
      if (G.Adj(v).size() == 1) {
        int prev = v;
        v = G.adj[v][0];

        // Find the middle node.
        for (int i = 1; i < G.N / 2; i++) {
          int tmp = v;
          v = (prev ^ G.adj[v][0] ^ G.adj[v][1]);
          prev = tmp;
        }
        return {bnd, G.global[v]};
      }
  } else if (G.N < exactCacheSize) {
    auto [td, root] = exactCache(G.adj);
    return {td, G.global[root]};
  } else if (G.IsTreeGraph()) {
    // TODO: this one is semi-expensive, but probably doesn't occur often.
    return treedepth_tree(G);
  }
  return {-1, -1};
}

// The global cache (a SetTrie) is what we use to store bounds on treedepths
// for subsets of the global graph (which correspond to induced subgraphs).
// Every subgraph that is in the cache comes with three data: a lower_bound, an
// upper_bound, and a root.
//
// The following is expect to be true of the cache and must be maintained:
// - It contains only connected subgraphs of the connected graph.
// - The lower_bound contains a proven lower bound on the subgraph.
// - The upper_bound contains a proven upper bound on the subgraph, where
// - The root is an element of the subgraph which witnesses this upper_bound,
//   and furthermore each connected component of the subgraph with the root
//   removed is also in the cache.
SetTrie cache;

// Cheap treedepth upper bound, useful for simple sanity checks.
std::pair<int, int> treedepth_upper(const Graph &G) {
  // Run some checks to see if we can simply find the exact td already.
  auto [td_exact, root_exact] = treedepth_exact(G);
  if (td_exact > -1 && root_exact > -1) return {td_exact, root_exact};
  Node *node = cache.Search(G);
  if (node) {
    return {node->upper_bound, node->root};
  }

  // Do a very simple recursion.
  for (int v = 0; v < G.N; v++)
    if (G.Adj(v).size() == G.max_degree) {
      auto cc = G.WithoutVertex(v);
      int result = 0;
      for (auto &&H : cc) result = std::max(result, treedepth_upper(H).first);
      return {result + 1, G.global[v]};
    }
  assert(false);
}

// Global variable keeping track of the time we've spent so far, and the limit.
time_t time_start_treedepth;
int max_time_treedepth = INT_MAX;  // No time limit.

// If we look for subsets, how much may those subsets differ from the set we
// are considering?
//
// subset_gap == 0 corresponds to not looking for subsets.
// subset_gap == INT_MAX corresponds to finding all subsets.
const int subset_gap = INT_MAX;

// The function treedepth computes Treedepth bounds on subgraphs of the global
// graph.
//
// Parameters:
// - Graph G, the graph for which we try to compute treedepth bounds.
// - int search_lbnd, the lower bound of which treedepths are useful.
// - int search_ubnd, the upper bound of which treedepths are useful.
//
// Returns (as pair<int, int>):
// - lower: a lower bound on the treedepth of the graph.
// - upper: an upper bound on the treedepth of the graph.
//
// Explanation of the search upper bound: the instance of treedepth that has
// called this instance may already have a decomposition of depth d of the
// graph it is decomposing. Then there is no reason to continue the search for
// this subgraph if its treedepth lower bound exceeds d - 1 (= search_lbnd).
// Thus we can exit as soon as lower comes above search_ubnd.
//
// Explanation of the search lower bound: the instance of treedepth that has
// called this instance will possibly be calling it on multiple components. If
// one sister component already has a lower bound on its treedepth of d, then
// there is no reason to try to get the treedepth of this subgraph any lower
// than d. Thus if we find a decomposition that can has depth at most d, i.e.
// upper is at most search_lbnd, we are done.
class Treedepth {
 public:
  const Graph &G;
  int lower = -1, upper = -1, root = -1;
  Node *node = nullptr;

  Treedepth(const Graph &G) : G(G) {
    // Set the trivial bounds.
    lower = std::max(G.M / G.N + 1, int(G.min_degree) + 1);
    upper = G.N;
    root = G.global[0];
  }

  std::vector<std::vector<int>> best_upper_separators;
  std::tuple<int, int, int> Calculate(int search_lbnd, int search_ubnd,
                                      bool store_best_separators = false) {
    // If the trivial bounds suffice, we are done.
    if (search_ubnd <= lower || search_lbnd >= upper || lower == upper ||
        search_lbnd > search_ubnd) {
      return {lower, upper, root};
    }

    // Run some checks to see if we can simply find the exact td already.
    auto [td_exact, root_exact] = treedepth_exact(G);
    if (td_exact > -1 && root_exact > -1)
      return {td_exact, td_exact, root_exact};

    // Lets check if it already exists in the cache.
    std::vector<int> G_word = G;
    node = cache.Search(G_word);
    if (node) {
      // This graph was in the cache, retrieve lower/upper bounds.
      lower = node->lower_bound;
      upper = node->upper_bound;
      root = node->root;

      // If cached bouns suffice, return! :-).
      if (search_ubnd <= lower || search_lbnd >= upper || lower == upper)
        return {lower, upper, root};
    }

    if (G.N == full_graph.N) std::cerr << "full_graph: kCore" << std::flush;

    // Below we calculate the smallest k-core that G can contain. If this is
    // non- empty, we recursively calculate the treedepth on this core first.
    // This should give a nice lower bound pretty rapidly.
    int v_min_degree = -1;
    auto cc_core = G.kCore(G.min_degree + 1);
    std::vector<std::vector<int>> kcore_best_separators;

    // If we do not have a kcore, simply remove a singly min degree vertex.
    if (cc_core.empty()) {
      int v_glob_min_degree = full_graph.N;
      for (int v = 0; v < G.N; ++v)
        if (G.Adj(v).size() == G.min_degree &&
            G.global[v] < v_glob_min_degree) {
          v_min_degree = v;
          v_glob_min_degree = G.global[v];
        }
      cc_core = G.WithoutVertex(v_min_degree);
    } else {
      for (int v = 0; v < G.N; ++v)
        if (G.Adj(v).size() == G.min_degree) {
          v_min_degree = v;
          break;
        }
    }

    if (!cc_core.empty()) {
      assert(cc_core[0].N < G.N);

      // Sort the components on density.
      std::sort(cc_core.begin(), cc_core.end(),
                [](auto &c1, auto &c2) { return c1.M / c1.N > c2.M / c2.N; });
      for (const auto &cc : cc_core) {
        Treedepth treedepth_cc(cc);
        auto [lower_cc, upper_cc, root_cc] = treedepth_cc.Calculate(
            std::max(lower, search_lbnd), std::min(upper, search_ubnd), true);

        if (lower_cc > lower) {
          lower = lower_cc;
          if (node) node->lower_bound = lower;
        }
        if (upper_cc + G.N - cc.N < upper) {
          upper = upper_cc + G.N - cc.N;
          root = G.global[v_min_degree];
          if (node) {
            node->upper_bound = upper;
            node->root = root;
          }
        }
        if (search_ubnd <= lower || search_lbnd >= upper || lower == upper)
          return {lower, upper, root};
        kcore_best_separators.insert(
            kcore_best_separators.end(),
            make_move_iterator(treedepth_cc.best_upper_separators.begin()),
            make_move_iterator(treedepth_cc.best_upper_separators.end()));
      }
    }
    if (G.N == full_graph.N)
      std::cerr << " gave a lower bound of " << lower << std::endl;

    // If G doesn't exist in the cache, lets add it now, since we will start
    // doing some real work.
    if (node == nullptr) {
      // Do a cheap upper bound search.
      auto [upper_H, root_H] = treedepth_upper(G);
      if (G.N == full_graph.N)
        std::cerr << "full_graph: treedepth_upper(G) = " << upper_H
                  << std::endl;
      if (upper_H < upper) {
        assert(root_H > -1);
        upper = upper_H;
        root = root_H;
      }

      // Try to find a better lower bound from some of its big subsets.
      for (auto [node_sub, node_gap] : cache.BigSubsets(G_word, subset_gap)) {
        lower = std::max(lower, node_sub->lower_bound);
        if (node_gap + node_sub->upper_bound < upper) {
          auto sub_word = node_sub->Word();
          std::vector<int> diff;
          std::set_difference(G_word.begin(), G_word.end(), sub_word.begin(),
                              sub_word.end(),
                              std::inserter(diff, diff.begin()));
          assert(diff.size() == node_gap);
          upper = node_gap + node_sub->upper_bound;
          root = diff[0];
        }
      }

      // Compute DfsTree-tree from the most promising node once, and then
      // evaluate the treedepth_tree on this tree.
      int v_max_degree = -1;
      for (int v = 0; v < G.N; ++v)
        if (G.Adj(v).size() == G.max_degree) {
          v_max_degree = v;
          break;
        }
      lower = std::max(lower, treedepth_tree(G.DfsTree(v_max_degree)).first);

      // Insert into the cache.
      node = cache.Insert(G).first;
      node->lower_bound = lower;
      node->upper_bound = upper;
      node->root = root;

      if (search_ubnd <= lower || search_lbnd >= upper || lower == upper)
        return {lower, upper, root};
    }

    // Main loop: try every separator as a set of roots.
    // new_lower tries to find a new treedepth lower bound on this subgraph.
    int new_lower = G.N;
    if (G.N == full_graph.N)
      std::cerr << "full_graph: bounds before separator loop " << lower
                << " <= td <= " << upper << "." << std::endl;

    for (auto &sep_vertices : kcore_best_separators) {
      for (int &v : sep_vertices) v = G.LocalIndex(v);
      Separator separator(G, sep_vertices);
      if (separator.fully_minimal) {
        SeparatorIteration(separator, search_lbnd, search_ubnd, new_lower,
                           store_best_separators);

        if (upper <= search_lbnd || lower == upper) {
          if (G.N == full_graph.N)
            std::cerr
                << "full_graph: separator from kcore gives `upper == lower == "
                << lower << "`, early exit. " << std::endl;
          // Choosing separator already gives us a treedepth decomposition
          // which is good enough (either a sister branch is at least this
          // long, or it matches a previously proved lower bound for this
          // subgraph) so we can use v as our root.
          return {lower, upper, root};
        }
      }
    }

    SeparatorGenerator sep_generator(G);
    size_t total_separators = 0;
    while (sep_generator.HasNext()) {
      auto separators = sep_generator.Next(100000);

      total_separators += separators.size();
      std::sort(separators.begin(), separators.end(),
                [](const Separator &s1, const Separator &s2) {
                  return s1.largest_component < s2.largest_component;
                });

      for (int s = 0; s < separators.size(); s++) {
        CheckTime();
        const Separator &separator = separators[s];
        SeparatorIteration(separator, search_lbnd, search_ubnd, new_lower,
                           store_best_separators);

        if (search_ubnd <= lower || search_lbnd >= upper || lower == upper) {
          if (G.N == full_graph.N) {
            std::cerr << "full_graph: generated total of " << total_separators
                      << " separators so far." << std::endl;
            std::cerr << "full_graph: separator " << s << " / "
                      << separators.size()
                      << " gives `upper == lower == " << lower
                      << "`, early exit. "
                      << "Separator has " << separator.vertices.size()
                      << " vertices, and largest component is ("
                      << separator.largest_component.first << ", "
                      << separator.largest_component.second << ")."
                      << std::endl;
          }
          // Choosing separator already gives us a treedepth decomposition which
          // is good enough (either a sister branch is at least this long, or it
          // matches a previously proved lower bound for this subgraph) so we
          // can use v as our root.
          return {lower, upper, root};
        }
      }
    }
    if (G.N == full_graph.N) {
      std::cerr << "full_graph: generated total of " << total_separators
                << " separators so far." << std::endl;
      std::cerr << "full_graph: completed entire separator loop." << std::endl;
    }
    node->lower_bound = lower = std::max(lower, new_lower);
    return {lower, upper, root};
  }

  // Returns whether this separator gave a lowering of the treedepth.
  inline void SeparatorIteration(const Separator &separator,
                                 const int search_lbnd, const int search_ubnd,
                                 int &new_lower,
                                 bool store_best_separators = false) {
    const int sep_size = separator.vertices.size();
    const int search_ubnd_sep =
        std::max(1, std::min(search_ubnd, upper) - sep_size);
    int search_lbnd_sep = std::max(1, std::max(search_lbnd, lower) - sep_size);

    int upper_sep = 0;
    int lower_sep = lower - sep_size;

    // Calculate a trivial lower bound on basis of the largest_component,
    // and continue if we won't get any improvement.
    const int lower_trivial =
        separator.largest_component.second / separator.largest_component.first +
        1;
    if (lower_trivial + sep_size >= new_lower) return;

    // Sort the components of G \ separator on density.
    auto cc = G.WithoutVertices(separator.vertices);
    std::sort(cc.begin(), cc.end(), [](const Graph &c1, const Graph &c2) {
      return c1.M / c1.N > c2.M / c2.N;
    });

    for (auto &&H : cc) {
      auto tuple = Treedepth(H).Calculate(search_lbnd_sep, search_ubnd_sep);

      const int lower_H = std::get<0>(tuple);
      const int upper_H = std::get<1>(tuple);

      upper_sep = std::max(upper_sep, upper_H);
      lower_sep = std::max(lower_sep, lower_H);
      search_lbnd_sep = std::max(search_lbnd_sep, lower_H);

      // If this won't give any new lower/upper bounds, we might as well stop.
      if (upper_sep + sep_size > upper && lower_sep + sep_size >= new_lower)
        return;
    }
    new_lower = std::min(new_lower, lower_sep + sep_size);

    // If we find a new lower bound, update the cache accordingly :-).
    if (lower_sep > lower) node->lower_bound = lower = lower_sep;

    // If we find a new upper bound, update the cache accordingly :-).
    if (upper_sep + sep_size < upper) {
      best_upper_separators.clear();
      node->upper_bound = upper = upper_sep + sep_size;
      node->root = root = G.global[separator.vertices[0]];

      // Iteratively remove the separator from G and update bounds.
      Graph H = G;
      for (int i = 1; i < separator.vertices.size(); i++) {
        // Get the subgraph after removing separator[i-1].
        auto cc =
            H.WithoutVertex(H.LocalIndex(G.global[separator.vertices[i - 1]]));
        assert(cc.size());
        if (cc.size() == 1)
          H = std::move(cc[0]);
        else {
          // We have multiple components, stop if all are single vertices.
          if (cc.size() == H.N - 1) break;
          for (auto &&ccc : cc)
            if (ccc.N > 1) {
              // Sanity check that there is only one non single vertex
              // component.
              assert(cc.size() + ccc.N == H.N);
              H = std::move(ccc);
              break;
            }
        }
        auto [node_H, inserted_H] = cache.Insert(H);

        // Now if H was new to the cache, or we have better bounds, lets
        // update!
        if (inserted_H || (upper - i < node_H->upper_bound)) {
          node_H->upper_bound = upper - i;
          node_H->lower_bound = std::max(lower - i, node_H->lower_bound);
          node_H->root = G.global[separator.vertices[i]];
        }
      }
    }
    if (upper_sep + sep_size == upper && store_best_separators) {
      std::vector<int> best_upper_separator;
      best_upper_separator.resize(sep_size);
      for (int i = 0; i < sep_size; i++)
        best_upper_separator[i] = G.global[separator.vertices[i]];
      best_upper_separators.emplace_back(std::move(best_upper_separator));
    }
  }

  inline void VertexIteration(int vertex, const int search_lbnd,
                              const int search_ubnd, int &new_lower) {
    // No need to check leaves.
    if (G.Adj(vertex).size() == 1) return;

    int search_ubnd_v = std::max(1, std::min(search_ubnd - 1, upper - 1));
    int search_lbnd_v = std::max(search_lbnd - 1, 1);

    int upper_v = 0;
    int lower_v = lower - 1;

    // Sort the components of G \ separator on density.
    auto cc = G.WithoutVertex(vertex);
    std::sort(cc.begin(), cc.end(), [](const Graph &c1, const Graph &c2) {
      return c1.M / c1.N > c2.M / c2.N;
    });

    for (auto &&H : cc) {
      auto tuple = Treedepth(H).Calculate(search_lbnd_v, search_ubnd_v);

      int lower_H = std::get<0>(tuple);
      int upper_H = std::get<1>(tuple);

      lower_v = std::max(lower_v, lower_H);
      upper_v = std::max(upper_v, upper_H);

      search_lbnd_v = std::max(search_lbnd_v, lower_H);

      // If this won't give any new lower/upper bounds, we might as well
      // break.
      if (upper_v + 1 >= upper && lower_v + 1 >= new_lower) break;
    }
    new_lower = std::min(new_lower, lower_v + 1);

    // If we find a new upper bound, update the cache accordingly :-).
    if (upper_v + 1 < upper) {
      node->upper_bound = upper = upper_v + 1;
      node->root = root = G.global[vertex];
    }
  }

  void CheckTime() {
    // Check whether we are still in the time limits.
    time_t now;
    time(&now);
    if (difftime(now, time_start_treedepth) > max_time_treedepth)
      throw std::runtime_error(
          "Ran out of time, spent " +
          std::to_string(difftime(now, time_start_treedepth)) + " seconds.");
  }
};

// Recursive function to reconstruct the tree that atains the treedepth.
void reconstruct(const Graph &G, int root, std::vector<int> &tree, int td) {
  assert(G.N);

  // Ensure that the cache contains the correct node.
  int new_root = std::get<2>(Treedepth(G).Calculate(td, G.N));
  assert(new_root > -1);
  tree.at(new_root) = root;

  // Root is the global coordinate, find its local coordinate.
  int local_root = -1;
  for (int v = 0; v < G.N; ++v)
    if (G.global[v] == new_root) {
      local_root = v;
      break;
    }
  assert(local_root > -1);
  for (auto H : G.WithoutVertex(local_root))
    reconstruct(H, new_root, tree, td - 1);
}

// Little helper function that returns the treedepth for the given graph.
std::pair<int, std::vector<int>> treedepth(const Graph &G) {
  cache = SetTrie();
  time(&time_start_treedepth);
  int td = std::get<1>(Treedepth(G).Calculate(1, G.N));
  std::vector<int> tree(G.N, -2);
  std::cerr << "full_graph: treedepth is " << td << "." << std::endl;
  reconstruct(G, -1, tree, td);
  std::cerr << "There are " << cache.size() << " subsets in the full cache."
            << std::endl;
  // The reconstruction is 0 based, the output is 1 based indexing, fix.
  for (auto &v : tree) v++;
  return {td, std::move(tree)};
}
