#include "cloud_dashboard_manager.h"
#include "ui/toast_notification.h"
#include "logger.h"

#include <QDesktopServices>
#include <QDir>
#include <QTimer>
#include <QUrl>
#include <QCoreApplication>

CloudDashboardManager::CloudDashboardManager(ToastNotification* toast, QWidget* parent)
    : QObject(parent)
    , m_toast(toast)
{
}

void CloudDashboardManager::setDashboardUrl(const QString& url)
{
    m_dashboardUrl = url;
}

CloudDashboardManager::~CloudDashboardManager()
{
    // 先断开所有信号，防止 waitForFinished 处理事件时触发已失效的 lambda
    if (m_process) {
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(3000)) {
                m_process->kill();
                m_process->waitForFinished(2000);
            }
        }
    }
}

QString CloudDashboardManager::findDashboardDir() const
{
    QString appDir = QCoreApplication::applicationDirPath();

    // 搜索策略：依次尝试多个可能的相对路径
    QStringList searchPaths = {
        appDir + "/../cloud_dashboard",                    // 开发构建 (build/debug)
        appDir + "/../../cloud_dashboard",                 // 开发构建 (build/)
        QDir::currentPath() + "/cloud_dashboard",          // 当前工作目录
        appDir + "/cloud_dashboard",                       // 可执行文件同目录
    };

    // 尝试从 PROJECT_ROOT_DIR 编译定义获取
#ifdef PROJECT_ROOT_DIR
    searchPaths.prepend(PROJECT_ROOT_DIR "/cloud_dashboard");
#endif

    for (const auto& dir : searchPaths) {
        QDir d(dir);
        if (d.exists("app.py")) {
            return d.absolutePath();
        }
    }

    return QString();
}

void CloudDashboardManager::launch()
{
    // 检查进程是否已在运行
    if (m_process && m_process->state() == QProcess::Running) {
        QDesktopServices::openUrl(QUrl(m_dashboardUrl));
        if (m_toast) m_toast->showMessage("云平台已在运行，已打开浏览器");
        return;
    }

    // 查找 cloud_dashboard 目录
    QString dashboardDir = findDashboardDir();

    if (dashboardDir.isEmpty()) {
        if (m_toast) m_toast->showMessage("未找到 cloud_dashboard 目录");
        spdlog::error("[CloudDashboard] 找不到 cloud_dashboard/app.py");
        return;
    }

    // 启动 Python 看板
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(dashboardDir);
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);

    connect(m_process, &QProcess::started, this, [this]() {
        spdlog::info("[CloudDashboard] 云平台进程已启动");
        if (m_toast) m_toast->showMessage("云平台启动中...");
        // 等一会再打开浏览器，让服务先启动
        QTimer::singleShot(3000, this, [this]() {
            QDesktopServices::openUrl(QUrl(m_dashboardUrl));
            if (m_toast) m_toast->showMessage("☁ 云平台已启动");
        });
    });

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        spdlog::info(QString("[CloudDashboard] 进程退出 code=%1 status=%2")
            .arg(exitCode).arg(status));
        if (m_toast) m_toast->showMessage("云平台已停止");
        m_process->deleteLater();
        m_process = nullptr;
    });

    spdlog::info("[CloudDashboard] 启动云平台...");
    m_process->start("python", {"app.py"});
}
