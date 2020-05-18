#include <chrono>
#include <cmath>
#include <exception>

#include "treedepth.hpp"

std::string ExtractFileName(const std::string& str) {
  return str.substr(str.find_last_of("/") + 1);
}

std::vector<int> vertex_domination(const Graph& G, std::vector<int> vertices) {
  std::vector<int> result;
  result.reserve(vertices);
  std::vector<bool> in_nbh(G.N, false);
  std::vector<bool> contracted(G.N, false);
  for (int v : vertices) {
    for (int nb : adj[v]) in_nbh[nb] = true;
    for (int w : vertices) {
      if (v < w) continue;
      for (int nb : adj[w])
        if (!in_nbh[nb] && nb != v) {
          contract = false;
          break;
        }
    }
    for (int nb : adj[v]) in_nbh[nb] = false;
  }
}

int main(int argc, char** argv) {
  LoadGraph(std::cin);
  Nauty nauty_full(full_graph);
  Nauty nauty_full_contract(full_graph.WithoutSymmetricNeighboorhoods().first);
  size_t leaves = 0;
  for (int v : nauty_full.orbit_representatives)
    if (full_graph.Adj(v).size() == 1) leaves++;
  std::cerr << nauty_full.num_orbits << "," << nauty_full.num_orbits - leaves
            << "," << nauty_full.num_orbits << ","
            << nauty_full_contract.num_automorphisms << std::endl;
  return 0;

  auto start = std::chrono::steady_clock::now();
  try {
    //    auto seperator_gen = SeparatorGenerator(full_graph_as_sub);
    //    size_t total_count = 0;
    //    while (seperator_gen.HasNext()) {
    //      total_count += seperator_gen.Next(100000).size();
    //      time(&end);
    //      std::cerr << "Total number of separators is " << total_count
    //                << ". Speed is " << double(total_count) / difftime(end,
    //                start)
    //                << " seps / s.\n";
    //    }

    auto [td, tree] = treedepth(full_graph);
    double time_elapsed =
        0.1 * std::round(10 * std::chrono::duration<double>(
                                  std::chrono::steady_clock::now() - start)
                                  .count());
    std::cerr << "Treedepth is: " << td << std::endl;
    std::cerr << "Elapsed time is " << time_elapsed << " seconds.\n";

    std::cout << td << std::endl;
    for (int parent : tree) std::cout << parent << std::endl;

    std::cerr << td << "," << time_elapsed << ", " << std::endl;
    return 0;
  } catch (std::exception& e) {
    double time_elapsed =
        0.1 * std::round(10 * std::chrono::duration<double>(
                                  std::chrono::steady_clock::now() - start)
                                  .count());
    std::cerr << "Failed! Encountered exception:\"" << e.what() << "\"."
              << std::endl;
    std::cerr << -1 << "," << time_elapsed << "," << e.what() << std::endl;
    return 1;
  }
}
