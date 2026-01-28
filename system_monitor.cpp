#include "system_monitor.h"
#include "logger.h"
#include <QDebug>

// ========== 平台相关头文件 ==========
#ifdef Q_OS_WIN
#include <windows.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")  // 链接 PDH 库
#endif

#ifdef Q_OS_LINUX
#include <fstream>
#include <sstream>
#include <unistd.h>
#endif

SystemMonitor::SystemMonitor(QObject* parent)
    : QObject(parent)
    , m_cpuLabel(nullptr)
    , m_memoryLabel(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_cpuUsage(0.0)
    , m_memoryUsage(0.0)
    , m_usedMemoryMB(0.0)
    , m_totalMemoryMB(0.0)
#ifdef Q_OS_WIN
    , m_cpuQuery(nullptr)
    , m_cpuCounter(nullptr)
#endif
#ifdef Q_OS_LINUX
    , m_lastTotalTime(0)
    , m_lastIdleTime(0)
#endif
{
    // 初始化平台资源
    initPlatformResources();

    // 连接定时器
    connect(m_updateTimer, &QTimer::timeout, this, &SystemMonitor::updateSystemInfo);

    // 设置默认更新间隔（1秒）
    m_updateTimer->setInterval(1000);

    Logger::instance()->info("系统监控器初始化完成");
}

SystemMonitor::~SystemMonitor()
{
    stopMonitoring();
    cleanupPlatformResources();
}

// ========== 公共接口实现 ==========

void SystemMonitor::setLabels(QLabel* cpuLabel, QLabel* memoryLabel)
{
    m_cpuLabel = cpuLabel;
    m_memoryLabel = memoryLabel;

    // 立即更新一次显示
    updateSystemInfo();
}

void SystemMonitor::setUpdateInterval(int intervalMs)
{
    if (intervalMs < 100) {
        qDebug() << "[SystemMonitor] 更新间隔过小，调整为 100ms";
        intervalMs = 100;
    }

    m_updateTimer->setInterval(intervalMs);
}

void SystemMonitor::startMonitoring()
{
    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
        Logger::instance()->info("系统监控已启动");
    }
}

void SystemMonitor::stopMonitoring()
{
    if (m_updateTimer->isActive()) {
        m_updateTimer->stop();
        Logger::instance()->info("系统监控已停止");
    }
}

// ========== 定时更新槽函数 ==========

void SystemMonitor::updateSystemInfo()
{
    // 1️⃣ 获取 CPU 使用率
    m_cpuUsage = getCpuUsage();

    // 2️⃣ 获取内存使用情况
    m_memoryUsage = getMemoryUsage(m_usedMemoryMB, m_totalMemoryMB);

    // 3️⃣ 更新 UI 显示
    if (m_cpuLabel) {
        m_cpuLabel->setText(QString("CPU: %1%").arg(m_cpuUsage, 0, 'f', 1));
    }

    if (m_memoryLabel) {
        // 显示格式：内存: 1234 MB / 8192 MB (15.1%)
        m_memoryLabel->setText(
            QString("内存: %1 MB / %2 MB (%3%)")
                .arg(m_usedMemoryMB, 0, 'f', 0)
                .arg(m_totalMemoryMB, 0, 'f', 0)
                .arg(m_memoryUsage, 0, 'f', 1)
            );
    }

    // 4️⃣ 发送信号
    emit cpuUsageUpdated(m_cpuUsage);
    emit memoryUsageUpdated(m_usedMemoryMB, m_totalMemoryMB, m_memoryUsage);
}

// ========== Windows 平台实现 ==========

#ifdef Q_OS_WIN

void SystemMonitor::initPlatformResources()
{
    PDH_STATUS status;

    // 创建查询对象
    status = PdhOpenQuery(NULL, 0, (PDH_HQUERY*)&m_cpuQuery);
    if (status != ERROR_SUCCESS) {
        qDebug() << "[SystemMonitor] PdhOpenQuery 失败:" << status;
        return;
    }

    // 添加 CPU 计数器（所有处理器的总使用率）
    status = PdhAddCounter(
        (PDH_HQUERY)m_cpuQuery,
        L"\\Processor(_Total)\\% Processor Time",  // 计数器路径
        0,
        (PDH_HCOUNTER*)&m_cpuCounter
        );

    if (status != ERROR_SUCCESS) {
        qDebug() << "[SystemMonitor] PdhAddCounter 失败:" << status;
        PdhCloseQuery((PDH_HQUERY)m_cpuQuery);
        m_cpuQuery = nullptr;
        return;
    }

    // 第一次采集（需要两次采集才能计算百分比）
    PdhCollectQueryData((PDH_HQUERY)m_cpuQuery);

    //qDebug() << "[SystemMonitor] Windows PDH 初始化成功";
}

void SystemMonitor::cleanupPlatformResources()
{
    if (m_cpuQuery)
    {
        PdhCloseQuery((PDH_HQUERY)m_cpuQuery);
        m_cpuQuery = nullptr;
        m_cpuCounter = nullptr;
    }
}

double SystemMonitor::getCpuUsage()
{
    if (!m_cpuQuery || !m_cpuCounter) {
        return 0.0;
    }

    PDH_STATUS status;
    PDH_FMT_COUNTERVALUE counterValue;

    // 采集数据
    status = PdhCollectQueryData((PDH_HQUERY)m_cpuQuery);
    if (status != ERROR_SUCCESS) {
        qDebug() << "[SystemMonitor] PdhCollectQueryData 失败:" << status;
        return 0.0;
    }

    // 获取格式化的值
    status = PdhGetFormattedCounterValue(
        (PDH_HCOUNTER)m_cpuCounter,
        PDH_FMT_DOUBLE,
        NULL,
        &counterValue
        );

    if (status != ERROR_SUCCESS) {
        qDebug() << "[SystemMonitor] PdhGetFormattedCounterValue 失败:" << status;
        return 0.0;
    }

    return counterValue.doubleValue;
}

double SystemMonitor::getMemoryUsage(double& usedMB, double& totalMB)
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (!GlobalMemoryStatusEx(&memInfo)) {
        qDebug() << "[SystemMonitor] GlobalMemoryStatusEx 失败";
        usedMB = 0.0;
        totalMB = 0.0;
        return 0.0;
    }

    // 转换为 MB
    totalMB = memInfo.ullTotalPhys / (1024.0 * 1024.0);
    double availableMB = memInfo.ullAvailPhys / (1024.0 * 1024.0);
    usedMB = totalMB - availableMB;

    // 计算百分比
    double usagePercent = memInfo.dwMemoryLoad;  // 直接使用系统提供的百分比

    return usagePercent;
}

#endif  // Q_OS_WIN

// ========== Linux 平台实现 ==========

#ifdef Q_OS_LINUX

void SystemMonitor::initPlatformResources()
{
    // Linux 不需要特殊初始化，直接读取 /proc 文件即可
    m_lastTotalTime = 0;
    m_lastIdleTime = 0;

    qDebug() << "[SystemMonitor] Linux /proc 初始化成功";
}

void SystemMonitor::cleanupPlatformResources()
{
    // Linux 不需要清理资源
}

double SystemMonitor::getCpuUsage()
{
    /**
     * 原理说明：
     * /proc/stat 文件的第一行包含 CPU 时间统计
     * 格式：cpu  user nice system idle iowait irq softirq steal guest guest_nice
     *
     * CPU 使用率 = (1 - idle_delta / total_delta) * 100
     */

    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        qDebug() << "[SystemMonitor] 无法打开 /proc/stat";
        return 0.0;
    }

    std::string line;
    std::getline(file, line);
    file.close();

    // 解析第一行（cpu 行）
    std::istringstream ss(line);
    std::string cpu;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

    ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    // 计算总时间和空闲时间
    unsigned long long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long idleTime = idle + iowait;

    // 第一次调用，只记录时间，不计算
    if (m_lastTotalTime == 0) {
        m_lastTotalTime = totalTime;
        m_lastIdleTime = idleTime;
        return 0.0;
    }

    // 计算时间差
    unsigned long long totalDelta = totalTime - m_lastTotalTime;
    unsigned long long idleDelta = idleTime - m_lastIdleTime;

    // 更新上次的值
    m_lastTotalTime = totalTime;
    m_lastIdleTime = idleTime;

    // 计算使用率
    if (totalDelta == 0) {
        return 0.0;
    }

    double usage = (1.0 - (double)idleDelta / totalDelta) * 100.0;
    return usage;
}

double SystemMonitor::getMemoryUsage(double& usedMB, double& totalMB)
{
    /**
     * 原理说明：
     * /proc/meminfo 文件包含内存信息
     * 我们需要读取：
     * - MemTotal: 总内存
     * - MemAvailable: 可用内存（比 MemFree 更准确，因为包括缓存）
     *
     * 已使用内存 = 总内存 - 可用内存
     */

    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        qDebug() << "[SystemMonitor] 无法打开 /proc/meminfo";
        usedMB = 0.0;
        totalMB = 0.0;
        return 0.0;
    }

    unsigned long long memTotal = 0, memAvailable = 0;
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> memTotal;  // 单位是 KB
        }
        else if (line.find("MemAvailable:") == 0) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> memAvailable;  // 单位是 KB
        }

        // 两个值都找到就可以退出了
        if (memTotal > 0 && memAvailable > 0) {
            break;
        }
    }

    file.close();

    // 转换为 MB
    totalMB = memTotal / 1024.0;
    double availableMB = memAvailable / 1024.0;
    usedMB = totalMB - availableMB;

    // 计算百分比
    double usagePercent = (usedMB / totalMB) * 100.0;

    return usagePercent;
}

#endif  // Q_OS_LINUX

// ========== MacOS 平台实现（备用） ==========

#ifdef Q_OS_MAC

void SystemMonitor::initPlatformResources()
{
    qDebug() << "[SystemMonitor] MacOS 平台暂未实现";
}

void SystemMonitor::cleanupPlatformResources()
{
    // MacOS 清理
}

double SystemMonitor::getCpuUsage()
{
    // TODO: 使用 host_processor_info 或 sysctl
    return 0.0;
}

double SystemMonitor::getMemoryUsage(double& usedMB, double& totalMB)
{
    // TODO: 使用 host_statistics 或 sysctl
    usedMB = 0.0;
    totalMB = 0.0;
    return 0.0;
}

#endif  // Q_OS_MAC
