#pragma once

#include <string>

namespace cliphist {

bool InitializeLogging(const std::string& db_path);
void ShutdownLogging();
void LogInfo(const std::string& message);
void LogError(const std::string& message);
std::string CurrentLogPath();

}  // namespace cliphist
