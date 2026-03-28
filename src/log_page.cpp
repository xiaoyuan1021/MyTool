#include "log_page.h"
#include "ui_log_page.h"
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QSizePolicy>

LogPage::LogPage(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::LogPage)
{
    m_ui->setupUi(this);

    // 设置大小策略，使其能够填充父容器
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 按钮信号由Qt自动连接（通过on_<objectName>_<signal>命名规则）
    // 无需手动连接以下按钮：
    // - btn_clearLog
    // - btn_openLog
}

LogPage::~LogPage()
{
    // m_ui 的所有权由 Qt 管理，不需要手动 delete
    m_ui = nullptr;
}

void LogPage::appendLog(const QString& message)
{
    m_ui->textEdit_log->append(message);
}

void LogPage::clearLog()
{
    m_ui->textEdit_log->clear();
}

void LogPage::on_btn_clearLog_clicked()
{
    clearLog();
}

void LogPage::on_btn_openLog_clicked()
{
    QString logPath = QCoreApplication::applicationDirPath() + "/../../logs";
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}
