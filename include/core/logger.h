#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextEdit>
#include <QDateTime>
#include <QProcess>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

class Logger :public QObject
{
    Q_OBJECT
public:
    static Logger* instance();
    void setTextEdit(QTextEdit* textdEdit);
    void setLogFile(const QString & filePath);
    void enableFileLog(bool enable);
    bool openLogFolder(bool selectFile = true);
    QString logFilePath() const;

    void debug(const QString & message);
    void info(const QString & message);
    void warning(const QString & message);
    void error(const QString & message);
    void clear();
    QStringList getRecentLogs(int count = 100) const;
    void outputLogsWithColor(const QStringList& logs);

signals:
    void logMessage(const QString& message);

private:
    Logger();
    ~Logger();

    QTextEdit * m_textEdit;

    // spdlog logger: qt_color_sink (UI) + basic_file_sink (file)
    std::shared_ptr<spdlog::logger> m_colorLogger;
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> m_fileSink;

    QString m_logFilePath;

    // 线程安全保护
    mutable QMutex m_mutex;
};

#endif // LOGGER_H
