#include "util/Logger.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <streambuf>

#include <QByteArray>
#include <QMessageLogContext>
#include <QString>
#include <QtGlobal>

#include "util/Path.hpp"
#include "util/Time.hpp"

namespace cliphist {

namespace {
std::mutex g_log_mutex;
std::ofstream g_log_file;
std::string g_log_path;
QtMessageHandler g_prev_qt_handler = nullptr;

class TeeStreamBuf final : public std::streambuf {
 public:
  TeeStreamBuf(std::streambuf* primary, std::streambuf* secondary)
      : primary_(primary), secondary_(secondary) {}

 protected:
  int overflow(int ch) override {
    if (ch == traits_type::eof()) {
      return sync() == 0 ? 0 : traits_type::eof();
    }
    const char c = static_cast<char>(ch);
    {
      std::lock_guard<std::mutex> lock(g_log_mutex);
      if (primary_ != nullptr) {
        (void)primary_->sputc(c);
      }
      if (secondary_ != nullptr) {
        (void)secondary_->sputc(c);
      }
    }
    return ch;
  }

  std::streamsize xsputn(const char* s, std::streamsize count) override {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (primary_ != nullptr) {
      (void)primary_->sputn(s, count);
    }
    if (secondary_ != nullptr) {
      (void)secondary_->sputn(s, count);
    }
    return count;
  }

  int sync() override {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    int rc = 0;
    if (primary_ != nullptr) {
      rc = primary_->pubsync();
    }
    if (secondary_ != nullptr) {
      rc = (secondary_->pubsync() == 0) ? rc : -1;
    }
    return rc;
  }

 private:
  std::streambuf* primary_ = nullptr;
  std::streambuf* secondary_ = nullptr;
};

std::streambuf* g_prev_cout = nullptr;
std::streambuf* g_prev_cerr = nullptr;
std::unique_ptr<TeeStreamBuf> g_cout_tee;
std::unique_ptr<TeeStreamBuf> g_cerr_tee;

std::string LogTimestamp() { return FormatLocalDateTime(UnixTimeSeconds()); }

void WriteLine(const char* level, const std::string& message) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  if (!g_log_file.is_open()) {
    return;
  }
  g_log_file << "[" << LogTimestamp() << "]"
             << "[" << level << "] " << message << '\n';
  g_log_file.flush();
}

std::string ToStdString(const QString& value) {
  const QByteArray utf8 = value.toUtf8();
  return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

void QtLogHandler(QtMsgType type, const QMessageLogContext& context,
                  const QString& message) {
  std::ostringstream oss;
  if (context.file != nullptr) {
    oss << context.file;
    if (context.line > 0) {
      oss << ":" << context.line;
    }
    oss << " ";
  }
  oss << ToStdString(message);

  switch (type) {
    case QtDebugMsg:
      WriteLine("QT-DEBUG", oss.str());
      break;
    case QtInfoMsg:
      WriteLine("QT-INFO", oss.str());
      break;
    case QtWarningMsg:
      WriteLine("QT-WARN", oss.str());
      break;
    case QtCriticalMsg:
      WriteLine("QT-CRIT", oss.str());
      break;
    case QtFatalMsg:
      WriteLine("QT-FATAL", oss.str());
      break;
  }

  if (g_prev_qt_handler != nullptr) {
    g_prev_qt_handler(type, context, message);
  }
}
}  // namespace

bool InitializeLogging(const std::string& db_path) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  if (g_log_file.is_open()) {
    return true;
  }

  std::error_code ec;
  const std::filesystem::path log_dir =
      FsPathFromUtf8(db_path).parent_path() / "logs";
  std::filesystem::create_directories(log_dir, ec);
  if (ec) {
    return false;
  }

  g_log_path = Utf8FromFsPath(log_dir / "cliphist.log");
  g_log_file.open(g_log_path, std::ios::out | std::ios::app);
  if (!g_log_file.is_open()) {
    g_log_path.clear();
    return false;
  }

  g_log_file << "\n===== session " << LogTimestamp() << " =====\n";
  g_log_file.flush();

  g_prev_cout = std::cout.rdbuf();
  g_prev_cerr = std::cerr.rdbuf();
  g_cout_tee = std::make_unique<TeeStreamBuf>(g_prev_cout, g_log_file.rdbuf());
  g_cerr_tee = std::make_unique<TeeStreamBuf>(g_prev_cerr, g_log_file.rdbuf());
  std::cout.rdbuf(g_cout_tee.get());
  std::cerr.rdbuf(g_cerr_tee.get());

  g_prev_qt_handler = qInstallMessageHandler(QtLogHandler);
  WriteLine("INFO", "logging initialized at " + g_log_path);
  return true;
}

void ShutdownLogging() {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  if (!g_log_file.is_open()) {
    return;
  }

  WriteLine("INFO", "logging shutdown");
  qInstallMessageHandler(g_prev_qt_handler);
  g_prev_qt_handler = nullptr;

  if (g_prev_cout != nullptr) {
    std::cout.rdbuf(g_prev_cout);
    g_prev_cout = nullptr;
  }
  if (g_prev_cerr != nullptr) {
    std::cerr.rdbuf(g_prev_cerr);
    g_prev_cerr = nullptr;
  }
  g_cout_tee.reset();
  g_cerr_tee.reset();

  g_log_file.flush();
  g_log_file.close();
}

void LogInfo(const std::string& message) { WriteLine("INFO", message); }

void LogError(const std::string& message) { WriteLine("ERROR", message); }

std::string CurrentLogPath() { return g_log_path; }

}  // namespace cliphist
