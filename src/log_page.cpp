#include "log_page.h"
#include "ui_log_page.h"
#include "logger.h"
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QSizePolicy>

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::LogPage)
{
    m_ui->setupUi(this);

    // Qt会自动连接on_<objectName>_<signalName>命名的槽函数，无需手动连接
}

LogPage::~LogPage()
{
    // m_ui 的所有权由 Qt 管理，不需要手动 delete
    m_ui = nullptr;
}

void LogPage::initialize()
{
    // 将日志输出控件连接到Logger
    Logger::instance()->setTextEdit(m_ui->textEdit_log);
}

void LogPage::refreshLogs()
{
    // 只清空日志显示，不加载旧日志
    m_ui->textEdit_log->clear();
}

void LogPage::appendLog(const QString& message)
{
    m_ui->textEdit_log->append(message);
}

void LogPage::clearLog()
{
    Logger::instance()->clear();
    Logger::instance()->info("日志已清空");
}

void LogPage::on_btn_clearLog_clicked()
{
    clearLog();
}

void LogPage::on_btn_openLog_clicked()
{
    Logger::instance()->openLogFolder(true);
}

void LogPage::on_btn_home_clicked()
{
    emit requestGoHome();
}
