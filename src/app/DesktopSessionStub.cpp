#include "app/DesktopSession.hpp"

#include <iostream>

#include "app/QtClipboardRuntime.hpp"

namespace cliphist {

int DesktopSession::Run(const CommandOptions& options) {
#ifdef _WIN32
  return RunQtDesktopSession(options);
#else
  (void)options;
  std::cerr << "当前构建未启用 X11，desktop 模式仅支持 Linux X11 环境\n";
  return 4;
#endif
}

}  // namespace cliphist
