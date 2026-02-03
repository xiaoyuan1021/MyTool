#pragma once

#include <QObject>
#include <QTimer>
#include <QLabel>

/**
 * @brief 系统资源监控类
 *
 * 功能：
 * 1. 实时监控 CPU 占用率
 * 2. 实时监控内存使用情况
 * 3. 自动更新到指定的 QLabel 控件
 *
 * 设计思路：
 * - 使用 QTimer 定时查询系统信息（默认1秒更新一次）
 * - 跨平台实现：Windows 使用 PDH，Linux 使用 /proc 文件系统
 * - 通过信号槽机制更新 UI，保证线程安全
 */
class SystemMonitor : public QObject
{
    Q_OBJECT

public:
    explicit SystemMonitor(QObject* parent = nullptr);
    ~SystemMonitor();

    // ========== 配置接口 ==========

    /**
     * @brief 设置显示控件
     * @param cpuLabel CPU 占用率显示标签
     * @param memoryLabel 内存使用率显示标签
     */
    void setLabels(QLabel* cpuLabel, QLabel* memoryLabel);

    /**
     * @brief 设置更新间隔
     * @param intervalMs 更新间隔（毫秒），建议 500-2000ms
     */
    void setUpdateInterval(int intervalMs);

    /**
     * @brief 开始监控
     */
    void startMonitoring();

    /**
     * @brief 停止监控
     */
    void stopMonitoring();

    // ========== 数据获取接口 ==========

    /**
     * @brief 获取当前 CPU 占用率
     * @return CPU 占用率（0.0 - 100.0）
     */
    double getCurrentCpuUsage() const { return m_cpuUsage; }

    /**
     * @brief 获取当前内存使用率
     * @return 内存使用率（0.0 - 100.0）
     */
    double getCurrentMemoryUsage() const { return m_memoryUsage; }

    /**
     * @brief 获取已使用内存（MB）
     */
    double getUsedMemoryMB() const { return m_usedMemoryMB; }

    /**
     * @brief 获取总内存（MB）
     */
    double getTotalMemoryMB() const { return m_totalMemoryMB; }

signals:
    /**
     * @brief CPU 使用率更新信号
     * @param usage 占用率百分比
     */
    void cpuUsageUpdated(double usage);

    /**
     * @brief 内存使用率更新信号
     * @param usedMB 已使用内存（MB）
     * @param totalMB 总内存（MB）
     * @param usagePercent 使用率百分比
     */
    void memoryUsageUpdated(double usedMB, double totalMB, double usagePercent);

private slots:
    /**
     * @brief 定时器触发的更新槽函数
     */
    void updateSystemInfo();

private:
    // ========== 平台相关实现 ==========

    /**
     * @brief 获取 CPU 占用率（平台相关）
     * @return CPU 占用率（0.0 - 100.0）
     */
    double getCpuUsage();

    /**
     * @brief 获取内存使用情况（平台相关）
     * @param usedMB 输出：已使用内存
     * @param totalMB 输出：总内存
     * @return 内存使用率百分比
     */
    double getMemoryUsage(double& usedMB, double& totalMB);

    /**
     * @brief 初始化平台相关资源
     */
    void initPlatformResources();

    /**
     * @brief 清理平台相关资源
     */
    void cleanupPlatformResources();

private:
    // ========== UI 控件 ==========
    QLabel* m_cpuLabel;       // CPU 显示标签
    QLabel* m_memoryLabel;    // 内存显示标签

    // ========== 定时器 ==========
    QTimer* m_updateTimer;    // 更新定时器

    // ========== 缓存数据 ==========
    double m_cpuUsage;        // 当前 CPU 占用率
    double m_memoryUsage;     // 当前内存使用率
    double m_usedMemoryMB;    // 已使用内存（MB）
    double m_totalMemoryMB;   // 总内存（MB）

    // ========== 平台相关数据（Windows） ==========
#ifdef Q_OS_WIN
    void* m_cpuQuery;         // PDH 查询句柄
    void* m_cpuCounter;       // CPU 计数器句柄
#endif

    // ========== 平台相关数据（Linux） ==========
#ifdef Q_OS_LINUX
    unsigned long long m_lastTotalTime;   // 上次总 CPU 时间
    unsigned long long m_lastIdleTime;    // 上次空闲时间
#endif
};
