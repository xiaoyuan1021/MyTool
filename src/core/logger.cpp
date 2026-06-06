#include "logger.h"

#include <QTextEdit>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

// ============================================================
// Global state (file-scope, not exported)
// ============================================================
static std::shared_ptr<spdlog::logger> s_logger;
static std::shared_ptr<spdlog::sinks::qt_color_sink_mt> s_uiSink;
static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> s_fileSink;
static QString s_logFilePath;

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
// Public API
// ============================================================

void setupLogging(QTextEdit* uiTextEdit, const QString& logDir)
{
    // 1) Create rotating file sink: 5MB max, 3 rotated files
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    s_logFilePath = dir.filePath("app.log");
    s_fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        s_logFilePath.toStdString(), 5 * 1024 * 1024, 3);

    // 2) Create qt_color_sink for UI
    s_uiSink = std::make_shared<spdlog::sinks::qt_color_sink_mt>(uiTextEdit, 1000, false, true);

    // Custom color config
    QTextCharFormat infoFormat;
    infoFormat.setForeground(Qt::black);
    s_uiSink->set_level_color(spdlog::level::info, infoFormat);

    QTextCharFormat debugFormat;
    debugFormat.setForeground(QColor(128, 128, 128));
    s_uiSink->set_level_color(spdlog::level::debug, debugFormat);

    QTextCharFormat warnFormat;
    warnFormat.setForeground(QColor(255, 165, 0));
    s_uiSink->set_level_color(spdlog::level::warn, warnFormat);

    QTextCharFormat errFormat;
    errFormat.setForeground(Qt::red);
    s_uiSink->set_level_color(spdlog::level::err, errFormat);

    QTextCharFormat criticalFormat;
    criticalFormat.setForeground(Qt::red);
    s_uiSink->set_level_color(spdlog::level::critical, criticalFormat);

    // 3) Create multi-sink logger and set as default
    std::vector<spdlog::sink_ptr> sinks = {s_uiSink, s_fileSink};
    s_logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());
    s_logger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");
    s_logger->set_level(spdlog::level::debug);

    spdlog::set_default_logger(s_logger);
    spdlog::set_level(spdlog::level::debug);

    // 4) Install Qt message handler to redirect qDebug etc.
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
