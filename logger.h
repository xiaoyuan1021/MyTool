#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextEdit>
#include <QDateTime>

class Logger :public QObject
{
    Q_OBJECT
public:  
    // 获取单例
    static Logger* instance();
    // 设置日志显示控件
    void setTextEdit(QTextEdit* textdEdit);
    // 输出日志
    void info(const QString & message);
    void warning(const QString & message);
    void error(const QString & message);
    void clear();
private:
    Logger();// 私有构造函数（单例模式）

    QTextEdit * m_textEdit;
};

#endif // LOGGER_H
