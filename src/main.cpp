#include <iostream>

#include "app/Application.hpp"
#include "cli/CommandParser.hpp"
#include "util/Logger.hpp"

int main(int argc, char** argv) {
  cliphist::CommandParser parser;
  cliphist::CommandOptions opts = parser.Parse(argc, argv);
  (void)cliphist::InitializeLogging(opts.db_path);
  cliphist::LogInfo("process start");
  cliphist::LogInfo("db path: " + opts.db_path);
  if (!cliphist::CurrentLogPath().empty()) {
    std::cerr << "日志文件: " << cliphist::CurrentLogPath() << '\n';
  }

  if (opts.type == cliphist::CommandType::kHelp) {
    cliphist::LogInfo("showing help");
    std::cout << cliphist::HelpText();
    cliphist::ShutdownLogging();
    return 0;
  }

  if (opts.type == cliphist::CommandType::kInvalid) {
    cliphist::LogError("invalid command: " + opts.error);
    std::cerr << opts.error << '\n' << cliphist::HelpText();
    cliphist::ShutdownLogging();
    return 1;
  }

  cliphist::Application app;
  const int rc = app.Run(opts);
  cliphist::LogInfo("process exit rc=" + std::to_string(rc));
  cliphist::ShutdownLogging();
  return rc;
}
