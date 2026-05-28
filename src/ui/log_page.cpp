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
    
    // 加载日志级别配置
    loadLogLevelConfig();
}

void LogPage::refreshLogs()
{
    // 清空日志显示
    m_ui->textEdit_log->clear();
    
    // 获取最近的日志并根据当前级别过滤显示
    QStringList recentLogs = Logger::instance()->getRecentLogs(1000);
    if (!recentLogs.isEmpty()) {
        Logger::instance()->outputLogsWithColor(recentLogs);
    }
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

void LogPage::on_combo_logLevel_currentIndexChanged(int index)
{
    // 将下拉框索引转换为spdlog级别
    spdlog::level::level_enum level;
    switch (index) {
    case 0:  // 全部
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
    
    // 设置Logger的UI级别
    Logger::instance()->setUILogLevel(level);
    
    // 保存配置
    saveLogLevelConfig(index);
    
    // 刷新日志显示
    refreshLogs();
}

void LogPage::loadLogLevelConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/app_config.json";
    QFile file(configPath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("logLevel")) {
                int level = obj["logLevel"].toInt();
                // 确保索引在有效范围内
                if (level >= 0 && level <= 4) {
                    m_ui->combo_logLevel->setCurrentIndex(level);
                    return;
                }
            }
        }
    }
    
    // 默认设置为Debug级别
    m_ui->combo_logLevel->setCurrentIndex(1);
}

void LogPage::saveLogLevelConfig(int level)
{
    QString configPath = QCoreApplication::applicationDirPath() + "/app_config.json";
    QFile file(configPath);
    
    QJsonObject obj;
    
    // 读取现有配置
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            obj = doc.object();
        }
    }
    
    // 更新日志级别
    obj["logLevel"] = level;
    
    // 写入文件
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}
