#include <iostream>

#include "app/Application.hpp"
#include "cli/CommandParser.hpp"

int main(int argc, char** argv) {
  cliphist::CommandParser parser;
  cliphist::CommandOptions opts = parser.Parse(argc, argv);

  if (opts.type == cliphist::CommandType::kHelp) {
    std::cout << cliphist::HelpText();
    return 0;
  }

  if (opts.type == cliphist::CommandType::kInvalid) {
    std::cerr << opts.error << '\n' << cliphist::HelpText();
    return 1;
  }

  cliphist::Application app;
  return app.Run(opts);
}
