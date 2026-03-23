#include "app/ProgramEntry.hpp"

#include <iostream>
#include <string>
#include <vector>

#include "app/Application.hpp"
#include "cli/CommandParser.hpp"
#include "util/Logger.hpp"
#include "util/Path.hpp"

#ifdef _WIN32
#include <filesystem>
#include <optional>
#include <windows.h>
#endif

namespace cliphist {

namespace {

#ifdef _WIN32
bool IsGuiMode(CommandType type) {
  return type == CommandType::kDesktop || type == CommandType::kUi;
}

std::wstring QuoteWindowsArg(const std::wstring& arg) {
  if (arg.empty()) {
    return L"\"\"";
  }
  if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    return arg;
  }

  std::wstring out = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t ch : arg) {
    if (ch == L'\\') {
      ++backslashes;
      continue;
    }
    if (ch == L'"') {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(L'"');
      backslashes = 0;
      continue;
    }
    if (backslashes > 0) {
      out.append(backslashes, L'\\');
      backslashes = 0;
    }
    out.push_back(ch);
  }
  if (backslashes > 0) {
    out.append(backslashes * 2, L'\\');
  }
  out.push_back(L'"');
  return out;
}

std::optional<int> LaunchGuiDelegate(const std::vector<std::string>& utf8_args,
                                     const CommandOptions& options) {
  if (!IsGuiMode(options.type)) {
    return std::nullopt;
  }

  wchar_t exe_buf[MAX_PATH] = {};
  const DWORD len = GetModuleFileNameW(nullptr, exe_buf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return std::nullopt;
  }

  std::filesystem::path current_exe(exe_buf);
  if (current_exe.filename() == L"cliphist-gui.exe") {
    return std::nullopt;
  }

  const std::filesystem::path gui_exe = current_exe.parent_path() / L"cliphist-gui.exe";
  if (!std::filesystem::exists(gui_exe)) {
    return std::nullopt;
  }

  std::wstring command_line = QuoteWindowsArg(gui_exe.wstring());
  for (std::size_t i = 1; i < utf8_args.size(); ++i) {
    command_line.push_back(L' ');
    command_line += QuoteWindowsArg(WideFromUtf8(utf8_args[i]));
  }

  std::vector<wchar_t> mutable_cmd(command_line.begin(), command_line.end());
  mutable_cmd.push_back(L'\0');

  STARTUPINFOW startup_info{};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info{};
  std::wstring work_dir = gui_exe.parent_path().wstring();

  const BOOL ok =
      CreateProcessW(gui_exe.c_str(), mutable_cmd.data(), nullptr, nullptr, FALSE,
                     CREATE_NEW_PROCESS_GROUP, nullptr,
                     work_dir.empty() ? nullptr : work_dir.c_str(), &startup_info,
                     &process_info);
  if (!ok) {
    return std::nullopt;
  }

  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  return 0;
}
#endif

}  // namespace

int RunProgram(int argc, char** argv, bool allow_gui_delegate) {
  CommandParser parser;
  std::vector<std::string> utf8_args = CommandLineArgsUtf8(argc, argv);
  std::vector<char*> utf8_argv;
  utf8_argv.reserve(utf8_args.size());
  for (auto& arg : utf8_args) {
    utf8_argv.push_back(arg.data());
  }

  CommandOptions opts =
      parser.Parse(static_cast<int>(utf8_argv.size()), utf8_argv.data());

  (void)InitializeLogging(opts.db_path);
  LogInfo("process start");
  LogInfo("db path: " + opts.db_path);
  if (allow_gui_delegate && !CurrentLogPath().empty()) {
    std::cerr << "日志文件: " << CurrentLogPath() << '\n';
  }

#ifdef _WIN32
  if (allow_gui_delegate) {
    if (const auto delegated = LaunchGuiDelegate(utf8_args, opts)) {
      LogInfo("delegated gui command to cliphist-gui.exe");
      ShutdownLogging();
      return *delegated;
    }
  }
#endif

  if (opts.type == CommandType::kHelp) {
    LogInfo("showing help");
    std::cout << HelpText();
    ShutdownLogging();
    return 0;
  }

  if (opts.type == CommandType::kInvalid) {
    LogError("invalid command: " + opts.error);
    std::cerr << opts.error << '\n' << HelpText();
    ShutdownLogging();
    return 1;
  }

  Application app;
  const int rc = app.Run(opts);
  LogInfo("process exit rc=" + std::to_string(rc));
  ShutdownLogging();
  return rc;
}

}  // namespace cliphist
