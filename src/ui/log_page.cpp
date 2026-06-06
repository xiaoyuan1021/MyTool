#include "log_page.h"
#include "ui_log_page.h"
#include "logger.h"
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QSizePolicy>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QTextCursor>
#include <QTextCharFormat>

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::LogPage)
{
    m_ui->setupUi(this);
}

LogPage::~LogPage()
{
    m_ui = nullptr;
}

void LogPage::initialize()
{
    // Initialize spdlog with rotating file sink + UI sink
    QString logDir = QCoreApplication::applicationDirPath() + "/logs";
    setupLogging(m_ui->textEdit_log, logDir);

    // Load log level config
    loadLogLevelConfig();
}

void LogPage::refreshLogs()
{
    m_ui->textEdit_log->clear();

    // Read from log file directly (bypass spdlog to avoid double formatting)
    QString filePath = logFilePath();
    if (filePath.isEmpty()) return;

    QStringList recentLogs;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                recentLogs.append(line);
            }
        }
        file.close();
    }

    // Keep only last 1000 entries
    if (recentLogs.size() > 1000) {
        recentLogs = recentLogs.mid(recentLogs.size() - 1000);
    }

    spdlog::level::level_enum currentLevel = uiLogLevel();

    QTextCursor cursor = m_ui->textEdit_log->textCursor();
    cursor.movePosition(QTextCursor::End);

    for (const QString& entry : recentLogs) {
        // File log format: 2026-06-06 15:59:44 [info] message
        int levelStart = entry.indexOf('[');
        int levelEnd = entry.indexOf(']', levelStart);
        if (levelStart == -1 || levelEnd == -1 || levelEnd <= levelStart) {
            cursor.insertText(entry + "\n");
            continue;
        }

        QString levelStr = entry.mid(levelStart + 1, levelEnd - levelStart - 1).toLower();

        // Parse level — fix: spdlog outputs [warn], not [warning]
        spdlog::level::level_enum logLevel = spdlog::level::info;
        if (levelStr == "debug") logLevel = spdlog::level::debug;
        else if (levelStr == "info") logLevel = spdlog::level::info;
        else if (levelStr == "warn") logLevel = spdlog::level::warn;
        else if (levelStr == "error") logLevel = spdlog::level::err;
        else if (levelStr == "critical") logLevel = spdlog::level::critical;

        if (logLevel < currentLevel) continue;

        // Color format
        QTextCharFormat fmt;
        switch (logLevel) {
        case spdlog::level::debug:    fmt.setForeground(QColor(128, 128, 128)); break;
        case spdlog::level::info:     fmt.setForeground(Qt::black); break;
        case spdlog::level::warn:     fmt.setForeground(QColor(255, 165, 0)); break;
        case spdlog::level::err:      fmt.setForeground(Qt::red); break;
        case spdlog::level::critical: fmt.setForeground(Qt::red); break;
        default: fmt.setForeground(Qt::black); break;
        }

        cursor.insertText(entry + "\n", fmt);
    }

    m_ui->textEdit_log->setTextCursor(cursor);
}

void LogPage::appendLog(const QString& message)
{
    m_ui->textEdit_log->append(message);
}

void LogPage::clearLog()
{
    m_ui->textEdit_log->clear();
    spdlog::info("日志已清空");
}

void LogPage::on_btn_clearLog_clicked()
{
    clearLog();
}

void LogPage::on_btn_openLog_clicked()
{
    openLogFolder(true);
}

void LogPage::on_btn_home_clicked()
{
    emit requestGoHome();
}

void LogPage::on_combo_logLevel_currentIndexChanged(int index)
{
    // Convert combo index to spdlog level
    spdlog::level::level_enum level;
    switch (index) {
    case 0:  // All
        level = spdlog::level::trace;
        break;
    case 1:  // Debug
        level = spdlog::level::debug;
        break;
    case 2:  // Info
        level = spdlog::level::info;
        break;
    case 3:  // Warning
        level = spdlog::level::warn;
        break;
    case 4:  // Error
        level = spdlog::level::err;
        break;
    default:
        level = spdlog::level::debug;
        break;
    }

    // Set UI log level
    setUILogLevel(level);

    // Save config
    saveLogLevelConfig(index);

    // Refresh display
    refreshLogs();
}

void LogPage::loadLogLevelConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/app_config.json";
    QFile file(configPath);

    int level = 1; // Default Debug

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("logLevel")) {
                int saved = obj["logLevel"].toInt();
                if (saved >= 0 && saved <= 4) {
                    level = saved;
                }
            }
        }
    }

    // Block signals to prevent setCurrentIndex from triggering refreshLogs
    const QSignalBlocker blocker(m_ui->combo_logLevel);
    m_ui->combo_logLevel->setCurrentIndex(level);

    // Manually set log level (bypass combo signal)
    spdlog::level::level_enum logLevel;
    switch (level) {
    case 0: logLevel = spdlog::level::trace; break;
    case 1: logLevel = spdlog::level::debug; break;
    case 2: logLevel = spdlog::level::info; break;
    case 3: logLevel = spdlog::level::warn; break;
    case 4: logLevel = spdlog::level::err; break;
    default: logLevel = spdlog::level::debug; break;
    }
    setUILogLevel(logLevel);
}

void LogPage::saveLogLevelConfig(int level)
{
    QString configPath = QCoreApplication::applicationDirPath() + "/app_config.json";
    QFile file(configPath);

    QJsonObject obj;

    // Read existing config
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            obj = doc.object();
        }
    }

    // Update log level
    obj["logLevel"] = level;

    // Write back
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}
