#include "util/Path.hpp"

#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace cliphist {

std::string WideToUtf8(const std::wstring& value);

namespace {
#ifdef _WIN32
std::string KnownFolderUtf8(REFKNOWNFOLDERID folder_id) {
  PWSTR wide_path = nullptr;
  const HRESULT rc = SHGetKnownFolderPath(folder_id, KF_FLAG_CREATE, nullptr, &wide_path);
  if (FAILED(rc) || wide_path == nullptr) {
    if (wide_path != nullptr) {
      CoTaskMemFree(wide_path);
    }
    return {};
  }

  const std::wstring value(wide_path);
  CoTaskMemFree(wide_path);
  return WideToUtf8(value);
}
#endif
}  // namespace

std::wstring WideFromUtf8(const std::string& value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                       static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }

  std::wstring out(static_cast<std::size_t>(size), L'\0');
  const int written = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                          static_cast<int>(value.size()),
                                          out.data(), size);
  if (written != size) {
    return {};
  }

  return out;
#else
  return std::wstring(value.begin(), value.end());
#endif
}

std::string WideToUtf8(const std::wstring& value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }

  const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                                       static_cast<int>(value.size()), nullptr, 0,
                                       nullptr, nullptr);
  if (size <= 0) {
    return {};
  }

  std::string out(static_cast<std::size_t>(size), '\0');
  const int written = WideCharToMultiByte(
      CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size,
      nullptr, nullptr);
  if (written != size) {
    return {};
  }

  return out;
#else
  return std::string(value.begin(), value.end());
#endif
}

std::vector<std::string> CommandLineArgsUtf8(int argc, char** argv) {
#ifdef _WIN32
  int wide_argc = 0;
  LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
  if (wide_argv != nullptr && wide_argc > 0) {
    std::vector<std::string> out;
    out.reserve(static_cast<std::size_t>(wide_argc));
    for (int i = 0; i < wide_argc; ++i) {
      out.push_back(WideToUtf8(wide_argv[i]));
    }
    LocalFree(wide_argv);
    return out;
  }
#endif

  std::vector<std::string> out;
  out.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    out.emplace_back((argv != nullptr && argv[i] != nullptr) ? argv[i] : "");
  }
  return out;
}

std::filesystem::path FsPathFromUtf8(const std::string& path_utf8) {
  return std::filesystem::u8path(path_utf8);
}

std::string Utf8FromFsPath(const std::filesystem::path& path) {
  return path.u8string();
}

std::string TempDirectoryUtf8() {
  return Utf8FromFsPath(std::filesystem::temp_directory_path());
}

std::string WindowsLocalAppDataUtf8() {
#ifdef _WIN32
  return KnownFolderUtf8(FOLDERID_LocalAppData);
#else
  return {};
#endif
}

std::string WindowsRoamingAppDataUtf8() {
#ifdef _WIN32
  return KnownFolderUtf8(FOLDERID_RoamingAppData);
#else
  return {};
#endif
}

}  // namespace cliphist
