#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cliphist {

std::vector<std::string> CommandLineArgsUtf8(int argc, char** argv);
std::filesystem::path FsPathFromUtf8(const std::string& path_utf8);
std::string Utf8FromFsPath(const std::filesystem::path& path);
std::wstring WideFromUtf8(const std::string& value);
std::string TempDirectoryUtf8();
std::string WindowsLocalAppDataUtf8();
std::string WindowsRoamingAppDataUtf8();

}  // namespace cliphist
