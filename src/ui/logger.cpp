#include "logger.h"

Logger* Logger::instance()
{
    static Logger logger;// 静态局部变量，只创建一次
    return &logger;
}

void Logger::setTextEdit(QTextEdit *textdEdit)
{
    QMutexLocker locker(&m_mutex);
    m_textEdit=textdEdit;
    
    // 创建 spdlog qt color logger
    if (m_textEdit) {
        // 设置最大行数为1000，支持UTF-8
        m_colorLogger = spdlog::qt_color_logger_mt("qt_color_logger", m_textEdit, 1000, true);
        
        // 自定义颜色配置
        auto* sink = static_cast<spdlog::sinks::qt_color_sink_mt*>(m_colorLogger->sinks()[0].get());
        
        // 设置info颜色为黑色（或绿色）
        QTextCharFormat infoFormat;
        infoFormat.setForeground(Qt::black);
        sink->set_level_color(spdlog::level::info, infoFormat);
        
        // 设置warning颜色为橙色
        QTextCharFormat warnFormat;
        warnFormat.setForeground(QColor(255, 165, 0)); // 橙色
        sink->set_level_color(spdlog::level::warn, warnFormat);
        
        // 设置error颜色为红色
        QTextCharFormat errorFormat;
        errorFormat.setForeground(Qt::red);
        sink->set_level_color(spdlog::level::err, errorFormat);
        
        // 设置critical颜色为红色
        QTextCharFormat criticalFormat;
        criticalFormat.setForeground(Qt::red);
        sink->set_level_color(spdlog::level::critical, criticalFormat);
        
        // 设置日志格式，包含时间、级别和消息
        m_colorLogger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");
    }
}

void Logger::setLogFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    m_logFilePath=filePath;

    if(m_logFile && m_logFile->isOpen())
    {
        m_logStream->flush();
        m_logFile->close();
    }
    if(m_logFile)
    {
        delete m_logFile;
        m_logFile=nullptr;
    }
    if(m_logStream)
    {
        delete m_logStream;
        m_logStream=nullptr;
    }
    m_logFile=new QFile(filePath);
    if(m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        m_logStream=new QTextStream(m_logFile);
}

void Logger::enableFileLog(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_fileLogEnabled = enable;
}

void Logger::writeToFile(const QString &log)
{
    // 调用者已持有锁（info/warning/error 中已加锁）
    if(m_fileLogEnabled && m_logStream)
    {
        *m_logStream <<log<<"\n";
        m_logStream->flush();
    }
}

bool Logger::openLogFolder(bool selectFile)
{
    if(m_logFilePath.isEmpty()) return false;
    QFileInfo fileInfo(m_logFilePath);

    // 如果需要选中文件且文件存在
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
        // 只打开目录
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }

    return true;
}

void Logger::info(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    QString time = QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    QString log = QString("%1 [info] %2").arg(time, message);
    
    // 使用spdlog输出带颜色的日志
    if (m_colorLogger) {
        m_colorLogger->info(message.toStdString());
    } else if(m_textEdit) {
        // 回退到原有实现
        m_textEdit->append(QString("<span style=\"color:black;\">%1</span>").arg(log));
    }
    
    QString fileLog = QString("%1 [info] %2").arg(time, message);
    writeToFile(fileLog);
    emit logMessage(fileLog);
}

void Logger::warning(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    QString time = QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    QString log = QString("%1 [warning] %2").arg(time, message);
    
    // 使用spdlog输出带颜色的日志
    if (m_colorLogger) {
        m_colorLogger->warn(message.toStdString());
    } else if(m_textEdit) {
        // 回退到原有实现
        m_textEdit->append(QString("<span style=\"color:orange;\">%1</span>").arg(log));
    }
    
    QString fileLog = QString("%1 [warning] %2").arg(time, message);
    writeToFile(fileLog);
    emit logMessage(fileLog);
}

void Logger::error(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    QString time = QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    QString log = QString("%1 [error] %2").arg(time, message);
    
    // 使用spdlog输出带颜色的日志
    if (m_colorLogger) {
        m_colorLogger->error(message.toStdString());
    } else if(m_textEdit) {
        // 回退到原有实现
        m_textEdit->append(QString("<span style=\"color:red;\">%1</span>").arg(log));
    }
    
    QString fileLog = QString("%1 [error] %2").arg(time, message);
    writeToFile(fileLog);
    emit logMessage(fileLog);
}

void Logger::clear()
{
    QMutexLocker locker(&m_mutex);
    if(m_textEdit)
    {
        m_textEdit->clear();
    }
}

QStringList Logger::getRecentLogs(int count) const
{
    QMutexLocker locker(&m_mutex);
    QStringList logs;
    
    // 如果有日志文件，从文件中读取最近的日志
    if (!m_logFilePath.isEmpty()) {
        QFile file(m_logFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QStringList allLines;
            
            // 读取所有行
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) {
                    allLines.append(line);
                }
            }
            file.close();
            
            // 获取最近的日志行
            int startIndex = qMax(0, allLines.size() - count);
            for (int i = startIndex; i < allLines.size(); ++i) {
                logs.append(allLines[i]);
            }
        }
    }
    
    // 如果没有日志文件或读取失败，返回空列表
    return logs;
}

void Logger::outputLogsWithColor(const QStringList& logs)
{
    QMutexLocker locker(&m_mutex);
    if (!m_colorLogger) {
        return;
    }
    
    // 解析每条日志并使用spdlog重新输出（带颜色）
    for (const QString& log : logs) {
        // 解析日志格式："%Y-%m-%d %H:%M:%S [%l] %v"
        // 例如："2026-04-01 19:44:19 [info] 这是一条信息"
        
        // 查找日志级别的开始位置
        int levelStart = log.indexOf('[');
        int levelEnd = log.indexOf(']', levelStart);
        
        if (levelStart != -1 && levelEnd != -1 && levelEnd > levelStart) {
            // 提取日志级别
            QString levelStr = log.mid(levelStart + 1, levelEnd - levelStart - 1).toLower();
            
            // 提取消息（跳过级别后的空格）
            QString message = log.mid(levelEnd + 2);
            
            // 根据日志级别调用相应的spdlog方法
            if (levelStr == "info") {
                m_colorLogger->info(message.toStdString());
            } else if (levelStr == "warning") {
                m_colorLogger->warn(message.toStdString());
            } else if (levelStr == "error") {
                m_colorLogger->error(message.toStdString());
            } else if (levelStr == "critical") {
                m_colorLogger->critical(message.toStdString());
            } else {
                // 默认使用info级别
                m_colorLogger->info(log.toStdString());
            }
        } else {
            // 如果无法解析格式，直接输出整行
            m_colorLogger->info(log.toStdString());
        }
    }
}

Logger::Logger()
    :m_textEdit(nullptr)
    ,m_logFile(nullptr)
    ,m_logStream(nullptr)
    ,m_fileLogEnabled(false)
    ,m_logFilePath("")
{

}

Logger::~Logger()
{
    if(m_logFile && m_logFile->isOpen())
    {
        m_logStream->flush();
        m_logFile->close();
    }
    if(m_logFile)
    {
        delete m_logFile;
    }
    if(m_logStream)
    {
        delete m_logStream;
    }
}
