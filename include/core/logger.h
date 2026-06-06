#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <format>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

// ============================================================
// std::formatter specialization for QString
// Allows spdlog::info("{}", someQString) to work directly
// ============================================================
template <>
struct std::formatter<QString> : std::formatter<std::string> {
    auto format(const QString& qstr, auto& ctx) const {
        return std::formatter<std::string>::format(qstr.toStdString(), ctx);
    }
};

// ============================================================
// spdlog-based logging setup (replaces old Logger singleton)
// ============================================================

// Initialize spdlog with rotating file sink + qt_color_sink (UI)
// Call once at startup, after QApplication exists
void setupLogging(QTextEdit* uiTextEdit, const QString& logDir);

// Set UI sink log level (controls what appears in LogPage)
void setUILogLevel(spdlog::level::level_enum level);
spdlog::level::level_enum uiLogLevel();

// Get current log file path
QString logFilePath();

// Open log folder in system explorer
bool openLogFolder(bool selectFile = true);

// Convenience: clear UI text edit
void clearLogUi();

#endif // LOGGER_H
