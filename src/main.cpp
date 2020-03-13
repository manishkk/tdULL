#include <exception>

#include "treedepth.hpp"

std::string ExtractFileName(const std::string& str) {
  return str.substr(str.find_last_of("/") + 1);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Expecting 2 arguments." << std::endl;
    return 1;
  }

  std::ifstream input;
  input.open(argv[1], std::ios::in);
  LoadGraph(input);
  input.close();

  time_t start, end;
  time(&start);

  std::cout << "Calculating treedepth for " << ExtractFileName(argv[1])
            << std::endl;
  try {
    //    auto seperator_gen = SeparatorGenerator(full_graph_as_sub);
    //    size_t total_count = 0;
    //    while (seperator_gen.HasNext()) {
    //      total_count += seperator_gen.Next(100000).size();
    //      time(&end);
    //      std::cout << "Total number of separators is " << total_count
    //                << ". Speed is " << double(total_count) / difftime(end,
    //                start)
    //                << " seps / s.\n";
    //    }
    auto [td, tree] = treedepth(full_graph_as_sub);
    time(&end);
    std::cout << "Treedepth is: " << td << std::endl;
    std::cout << "Elapsed time is " << difftime(end, start) << " seconds.\n";

    std::ofstream output;
    output.open(argv[2], std::ios::out);
    output << td << std::endl;
    for (int parent : tree) output << parent << std::endl;
    output.close();
    std::cout << "Saved the tree to '" << argv[2] << "'" << std::endl;
    std::cerr << ExtractFileName(argv[1]) << "," << td << ","
              << difftime(end, start) << ", " << std::endl;
    return 0;
  } catch (std::exception& e) {
    time(&end);
    std::cout << "Failed! Encountered exception:\"" << e.what() << "\"."
              << std::endl;
    std::cerr << ExtractFileName(argv[1]) << "," << -1 << ","
              << difftime(end, start) << "," << e.what() << std::endl;
    return 1;
  }
}
