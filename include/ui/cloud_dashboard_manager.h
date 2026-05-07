#ifndef CLOUD_DASHBOARD_MANAGER_H
#define CLOUD_DASHBOARD_MANAGER_H

#include <QObject>
#include <QProcess>

class QWidget;
class ToastNotification;

/**
 * 云平台看板管理器 - 负责云平台Python看板进程的生命周期管理
 *
 * 职责：
 * 1. 查找 cloud_dashboard 目录
 * 2. 启动/停止 Python 看板进程
 * 3. 打开浏览器访问看板
 *
 * 从 MainWindow 中提取，降低 MainWindow 的职责复杂度。
 */
class CloudDashboardManager : public QObject
{
    Q_OBJECT

public:
    explicit CloudDashboardManager(ToastNotification* toast, QWidget* parent = nullptr);
    ~CloudDashboardManager();

    /// 设置看板地址（默认 http://127.0.0.1:5000）
    void setDashboardUrl(const QString& url);

public slots:
    /// 启动云平台看板（或在已运行时打开浏览器）
    void launch();

private:
    /// 在多个候选路径中查找 cloud_dashboard 目录
    QString findDashboardDir() const;

    QProcess* m_process = nullptr;
    ToastNotification* m_toast = nullptr;
    QString m_dashboardUrl = "http://127.0.0.1:5000";
};

#endif // CLOUD_DASHBOARD_MANAGER_H