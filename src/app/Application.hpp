#pragma once

#include "cli/CommandParser.hpp"

namespace cliphist {

class Application {
 public:
  int Run(const CommandOptions& options);
};

}  // namespace cliphist
