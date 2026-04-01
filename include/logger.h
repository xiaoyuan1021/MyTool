#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextEdit>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/qt_sinks.h"

class Logger :public QObject
{
    Q_OBJECT
public:
    // 获取单例
    static Logger* instance();
    // 设置日志显示控件
    void setTextEdit(QTextEdit* textdEdit);
    //设置路径
    void setLogFile(const QString & filePath);
    void enableFileLog(bool enable);
    bool openLogFolder(bool selectFile = true);
    void getLogFilePath() const;
    // 输出日志
    void info(const QString & message);
    void warning(const QString & message);
    void error(const QString & message);
    void clear();
    QStringList getRecentLogs(int count = 100) const;
    void outputLogsWithColor(const QStringList& logs);

signals:
    void logMessage(const QString& message);

private:
    Logger();// 私有构造函数（单例模式）
    ~Logger();

    void writeToFile(const QString& log);

    QTextEdit * m_textEdit;

    QFile * m_logFile;          //日志对象
    QTextStream * m_logStream;  //文本流对象
    bool m_fileLogEnabled;
    QString m_logFilePath;

    // spdlog logger with color support
    std::shared_ptr<spdlog::logger> m_colorLogger;

};

#endif // LOGGER_H
