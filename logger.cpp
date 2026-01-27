#include "logger.h"

Logger* Logger::instance()
{
    static Logger logger;// 静态局部变量，只创建一次
    return &logger;
}

void Logger::setTextEdit(QTextEdit *textdEdit)
{
    m_textEdit=textdEdit;
}

void Logger::setLogFile(const QString &filePath)
{
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
    m_fileLogEnabled = enable;
}

void Logger::writeToFile(const QString &log)
{
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
    QString time =QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    if(m_textEdit)
    {
        QString log=QString("<span style='color:black'>%1 [info] %2</span>").arg(time,message);
        m_textEdit->append(log);
        m_textEdit->moveCursor(QTextCursor::End);
    }
    QString fileLog=QString("%1 [info] %2").arg(time,message);
    writeToFile(fileLog);
}

void Logger::warning(const QString &message)
{
    QString time =QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    if(m_textEdit)
    {
        QString log=QString("<span style='color:orange'>%1 [warning] %2</span>").arg(time,message);
        m_textEdit->append(log);
        m_textEdit->moveCursor(QTextCursor::End);
    }
    QString fileLog=QString("%1 [warning] %2").arg(time,message);
    writeToFile(fileLog);
}

void Logger::error(const QString &message)
{
    QString time =QDateTime::currentDateTime().toString("MM-dd hh:mm:ss");
    if(m_textEdit)
    {
        QString log=QString("<span style='color:red'>%1 [error] %2</span>").arg(time,message);
        m_textEdit->append(log);
        m_textEdit->moveCursor(QTextCursor::End);
    }
    QString fileLog=QString("%1 [error] %2").arg(time,message);
    writeToFile(fileLog);
}

void Logger::clear()
{
    if(m_textEdit)
    {
        m_textEdit->clear();
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
