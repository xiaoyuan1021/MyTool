#include "logger.h"

#include <QTextEdit>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QTextCursor>
#include <QTextCharFormat>

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/common.h"

// ============================================================
// Global state (file-scope, not exported)
// ============================================================
static std::shared_ptr<spdlog::logger> s_logger;
static std::shared_ptr<spdlog::sinks::qt_color_sink_mt> s_uiSink;
static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> s_fileSink;
static QString s_logFilePath;
static QTextEdit* s_uiTextEdit = nullptr;  // for re-rendering on level change
static QStringList s_sessionLogs;          // in-memory buffer of current session logs

// ============================================================
// Qt 全局消息处理：将 qDebug/qInfo/qWarning/qCritical 路由到 spdlog
// ============================================================
static void qtMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx)
    auto logger = spdlog::default_logger();
    if (!logger) return;

    switch (type) {
    case QtDebugMsg:    logger->debug(msg.toStdString()); break;
    case QtInfoMsg:     logger->info(msg.toStdString());  break;
    case QtWarningMsg:  logger->warn(msg.toStdString());  break;
    case QtCriticalMsg: logger->error(msg.toStdString()); break;
    case QtFatalMsg:    logger->critical(msg.toStdString()); break;
    }
}

// ============================================================
// Session log buffer sink: stores formatted messages for re-rendering
// ============================================================
#include <vector>

struct LogEntry {
    QString formatted;
    spdlog::level::level_enum level;
};

static std::vector<LogEntry> s_sessionBuffer;

class SessionBufferSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {
        spdlog::memory_buf_t buf;
        formatter_->format(msg, buf);
        QString text = QString::fromUtf8(buf.data(), static_cast<int>(buf.size()));
        s_sessionBuffer.push_back({text, msg.level});
    }
    void flush_() override {}
};

// ============================================================
// Re-render session logs filtered by level
// ============================================================
void rerenderSessionLogs()
{
    if (!s_uiTextEdit) return;

    spdlog::level::level_enum minLevel = uiLogLevel();

    s_uiTextEdit->clear();
    QTextCursor cursor(s_uiTextEdit->textCursor());
    cursor.movePosition(QTextCursor::End);

    // Default colors matching qt_color_sink (dark_colors=false)
    auto levelColor = [](spdlog::level::level_enum lv) -> QColor {
        switch (lv) {
        case spdlog::level::trace:    return Qt::gray;
        case spdlog::level::debug:    return Qt::cyan;
        case spdlog::level::info:     return Qt::green;
        case spdlog::level::warn:     return Qt::yellow;
        case spdlog::level::err:      return Qt::red;
        case spdlog::level::critical: return Qt::red;
        default: return Qt::black;
        }
    };

    for (const auto& entry : s_sessionBuffer) {
        if (entry.level < minLevel) continue;

        QTextCharFormat fmt;
        fmt.setForeground(levelColor(entry.level));
        cursor.insertText(entry.formatted, fmt);
    }
}

// ============================================================
// Public API
// ============================================================

void setupLogging(QTextEdit* uiTextEdit, const QString& logDir)
{
    s_uiTextEdit = uiTextEdit;
    s_sessionBuffer.clear();

    // 1) Create rotating file sink: 5MB max, 3 rotated files
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    s_logFilePath = dir.filePath("app.log");
    s_fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        s_logFilePath.toStdString(), 5 * 1024 * 1024, 3);

    // 2) Create qt_color_sink for UI (uses built-in default colors)
    s_uiSink = std::make_shared<spdlog::sinks::qt_color_sink_mt>(uiTextEdit, 1000, false, true);

    // 3) Create session buffer sink for re-rendering
    auto sessionSink = std::make_shared<SessionBufferSink>();
    sessionSink->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");

    // 4) Create multi-sink logger and set as default
    std::vector<spdlog::sink_ptr> sinks = {s_uiSink, sessionSink, s_fileSink};
    s_logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());
    s_logger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");
    s_logger->set_level(spdlog::level::debug);

    spdlog::set_default_logger(s_logger);
    spdlog::set_level(spdlog::level::debug);

    // 5) Install Qt message handler to redirect qDebug etc.
    qInstallMessageHandler(qtMessageHandler);
}

void setUILogLevel(spdlog::level::level_enum level)
{
    if (s_uiSink) {
        s_uiSink->set_level(level);
    }
}

spdlog::level::level_enum uiLogLevel()
{
    if (s_uiSink) {
        return s_uiSink->level();
    }
    return spdlog::level::debug;
}

QString logFilePath()
{
    return s_logFilePath;
}

void flushLogs()
{
    if (s_logger) {
        s_logger->flush();
    }
}

bool openLogFolder(bool selectFile)
{
    if (s_logFilePath.isEmpty()) return false;
    QFileInfo fileInfo(s_logFilePath);

    if (selectFile && fileInfo.exists())
    {
#ifdef Q_OS_WIN
        QProcess::startDetached("explorer",
                                QStringList() << "/select," << QDir::toNativeSeparators(fileInfo.absoluteFilePath()));
#elif defined(Q_OS_MAC)
        QProcess::startDetached("open",
                                QStringList() << "-R" << fileInfo.absoluteFilePath());
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
#endif
    }
    else
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }

    return true;
}

void clearLogUi()
{
    // No-op: QTextEdit is managed by spdlog qt_sink; caller should clear directly
}
