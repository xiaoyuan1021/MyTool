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

void Logger::info(const QString &message)
{
    if(!m_textEdit) return;
    QString time =QDateTime::currentDateTime().toString("hh:mm:ss");
    QString log=QString("<span style='color:black'>%1 [info] %2</span>").arg(time,message);

    m_textEdit->append(log);
    m_textEdit->moveCursor(QTextCursor::End);
}

void Logger::warning(const QString &message)
{
    if(!m_textEdit) return;
    QString time =QDateTime::currentDateTime().toString("hh:mm:ss");
    QString log=QString("<span style='color:orange'>%1 [info] %2</span>").arg(time,message);

    m_textEdit->append(log);
    m_textEdit->moveCursor(QTextCursor::End);
}

void Logger::error(const QString &message)
{
    if(!m_textEdit) return;
    QString time =QDateTime::currentDateTime().toString("hh:mm:ss");
    QString log=QString("<span style='color:red'>%1 [info] %2</span>").arg(time,message);

    m_textEdit->append(log);
    m_textEdit->moveCursor(QTextCursor::End);
}

void Logger::clear()
{
    if(m_textEdit)
    {
        m_textEdit->clear();
    }
}

Logger::Logger():m_textEdit(nullptr)
{

}
