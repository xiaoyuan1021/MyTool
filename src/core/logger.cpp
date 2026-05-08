#include "logger.h"

// ============================================================
// Qt 全局消息处理：将 qDebug/qInfo/qWarning/qCritical 路由到 spdlog
// ============================================================
static void qtMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx)
    // 避免递归：Logger::info 内部也可能触发 qDebug
    // 通过 spdlog 直接输出，不再经过 Logger 的方法
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

Logger* Logger::instance()
{
    static Logger logger;
    return &logger;
}

void Logger::setTextEdit(QTextEdit *textdEdit)
{
    QMutexLocker locker(&m_mutex);
    m_textEdit = textdEdit;

    if (m_textEdit) {
        // 创建 spdlog qt color sink（UI 彩色输出）
        auto qtSink = std::make_shared<spdlog::sinks::qt_color_sink_mt>(m_textEdit, 1000, false, true);

        // 自定义颜色配置
        QTextCharFormat infoFormat;
        infoFormat.setForeground(Qt::black);
        qtSink->set_level_color(spdlog::level::info, infoFormat);

        QTextCharFormat debugFormat;
        debugFormat.setForeground(QColor(128, 128, 128));
        qtSink->set_level_color(spdlog::level::debug, debugFormat);

        QTextCharFormat warnFormat;
        warnFormat.setForeground(QColor(255, 165, 0));
        qtSink->set_level_color(spdlog::level::warn, warnFormat);

        QTextCharFormat errorFormat;
        errorFormat.setForeground(Qt::red);
        qtSink->set_level_color(spdlog::level::err, errorFormat);

        QTextCharFormat criticalFormat;
        criticalFormat.setForeground(Qt::red);
        qtSink->set_level_color(spdlog::level::critical, criticalFormat);

        // 构造 sinks 列表
        std::vector<spdlog::sink_ptr> sinks = {qtSink};

        // 如果已有 file sink，一并挂上
        if (m_fileSink) {
            sinks.push_back(m_fileSink);
        }

        // 创建支持多 sink 的 logger
        m_colorLogger = std::make_shared<spdlog::logger>("app_logger", sinks.begin(), sinks.end());
        m_colorLogger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");
        m_colorLogger->set_level(spdlog::level::debug);

        // 设为全局默认 logger，使 spdlog::info() 等也会进入 UI + 文件
        spdlog::set_default_logger(m_colorLogger);
    }
}

void Logger::setLogFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    m_logFilePath = filePath;

    try {
        // 创建 spdlog file sink，追加模式
        m_fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            filePath.toStdString(), false);

        if (m_colorLogger) {
            // 挂到已有 logger
            m_colorLogger->sinks().push_back(m_fileSink);
        }
    } catch (const std::exception &) {
        // 文件创建失败时不崩溃
    }
}

void Logger::enableFileLog(bool enable)
{
    QMutexLocker locker(&m_mutex);
    if (m_fileSink) {
        m_fileSink->set_level(enable ? spdlog::level::trace : spdlog::level::off);
    }
}

bool Logger::openLogFolder(bool selectFile)
{
    if (m_logFilePath.isEmpty()) return false;
    QFileInfo fileInfo(m_logFilePath);

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

QString Logger::logFilePath() const
{
    return m_logFilePath;
}

void Logger::debug(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (m_colorLogger) {
        m_colorLogger->debug(message.toStdString());
        emit logMessage(message);
    }
}

void Logger::info(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (m_colorLogger) {
        m_colorLogger->info(message.toStdString());
        emit logMessage(message);
    }
}

void Logger::warning(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (m_colorLogger) {
        m_colorLogger->warn(message.toStdString());
        emit logMessage(message);
    }
}

void Logger::error(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (m_colorLogger) {
        m_colorLogger->error(message.toStdString());
        emit logMessage(message);
    }
}

void Logger::clear()
{
    QMutexLocker locker(&m_mutex);
    if (m_textEdit)
    {
        m_textEdit->clear();
    }
}

QStringList Logger::getRecentLogs(int count) const
{
    QMutexLocker locker(&m_mutex);
    QStringList logs;

    if (!m_logFilePath.isEmpty()) {
        QFile file(m_logFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QStringList allLines;

            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) {
                    allLines.append(line);
                }
            }
            file.close();

            int startIndex = qMax(0, allLines.size() - count);
            for (int i = startIndex; i < allLines.size(); ++i) {
                logs.append(allLines[i]);
            }
        }
    }

    return logs;
}

void Logger::outputLogsWithColor(const QStringList& logs)
{
    QMutexLocker locker(&m_mutex);
    if (!m_colorLogger) {
        return;
    }

    for (const QString& log : logs) {
        int levelStart = log.indexOf('[');
        int levelEnd = log.indexOf(']', levelStart);

        if (levelStart != -1 && levelEnd != -1 && levelEnd > levelStart) {
            QString levelStr = log.mid(levelStart + 1, levelEnd - levelStart - 1).toLower();
            QString message = log.mid(levelEnd + 2);

            if (levelStr == "info") {
                m_colorLogger->info(message.toStdString());
            } else if (levelStr == "warning") {
                m_colorLogger->warn(message.toStdString());
            } else if (levelStr == "error") {
                m_colorLogger->error(message.toStdString());
            } else if (levelStr == "critical") {
                m_colorLogger->critical(message.toStdString());
            } else {
                m_colorLogger->info(log.toStdString());
            }
        } else {
            m_colorLogger->info(log.toStdString());
        }
    }
}

Logger::Logger()
    : m_textEdit(nullptr)
    , m_logFilePath("")
{
    // 安装 Qt 全局消息处理器，将所有 qDebug/qWarning/qCritical 重定向到 spdlog
    qInstallMessageHandler(qtMessageHandler);

    // 设置 spdlog 全局默认级别为 debug
    spdlog::set_level(spdlog::level::debug);
}

Logger::~Logger()
{
    // 恢复 Qt 默认消息处理器
    qInstallMessageHandler(nullptr);

    if (m_colorLogger) {
        m_colorLogger->flush();
    }
    spdlog::shutdown();
}
