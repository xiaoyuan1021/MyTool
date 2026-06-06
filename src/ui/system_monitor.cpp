#include "system_monitor.h"
#include "logger.h"
#include <QDebug>
#include <algorithm>

// ========== 平台相关头文件 ==========
#ifdef Q_OS_WIN
#include <windows.h>
#include <pdh.h>
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
        Logger::instance()->info("[SystemMonitor] 更新间隔过小，调整为 100ms");
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

void SystemMonitor::setupWithLogging(QLabel* cpuLabel, QLabel* memoryLabel, int intervalMs)
{
    setLabels(cpuLabel, memoryLabel);
    setUpdateInterval(intervalMs);
    startMonitoring();

    // 连接日志记录信号
    connect(this, &SystemMonitor::cpuUsageUpdated, [](double usage) {
        if (usage > 80.0) {
            Logger::instance()->warning(QString("CPU 占用率过高: %1%").arg(usage, 0, 'f', 1));
        }
    });

    connect(this, &SystemMonitor::memoryUsageUpdated, [](double usedMB, double totalMB, double percent) {
        if (percent > 90.0) {
            Logger::instance()->warning(QString("内存使用率过高: %1% (%2/%3 MB)")
                .arg(percent, 0, 'f', 1).arg(usedMB, 0, 'f', 0).arg(totalMB, 0, 'f', 0));
        }
    });
}

void SystemMonitor::setupWithStatusBar(QStatusBar* statusBar, int intervalMs)
{
    if (!statusBar) {
        Logger::instance()->error("状态栏指针为空，无法设置系统监控");
        return;
    }

    // 创建状态栏标签
    QLabel* cpuLabel = new QLabel();
    QLabel* memoryLabel = new QLabel();
    
    // 设置标签样式，确保支持富文本显示
    cpuLabel->setStyleSheet("QLabel { color: #333; font-size: 12px; margin-right: 15px; }");
    memoryLabel->setStyleSheet("QLabel { color: #333; font-size: 12px; margin-right: 15px; }");
    
    // 设置文本格式为富文本
    cpuLabel->setTextFormat(Qt::RichText);
    memoryLabel->setTextFormat(Qt::RichText);
    
    // 添加到状态栏
    statusBar->addPermanentWidget(cpuLabel);
    statusBar->addPermanentWidget(memoryLabel);
    
    // 设置标签并启动监控
    setLabels(cpuLabel, memoryLabel);
    setUpdateInterval(intervalMs);
    startMonitoring();

    Logger::instance()->info("系统监控已在状态栏中启动");
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
        m_cpuLabel->setText(
            QString("<img src=':/icons/CPU.png' width='16' height='16' style='vertical-align: middle;'> CPU: %1%")
                .arg(m_cpuUsage, 0, 'f', 1)
        );
    }

    if (m_memoryLabel) {
        // 显示格式：内存: 1234 MB / 8192 MB (15.1%)
        m_memoryLabel->setText(
            QString("<img src=':/icons/memory.png' width='16' height='16' style='vertical-align: middle;'> 内存: %1 MB / %2 MB (%3%)")
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
    PDH_STATUS status = PdhOpenQuery(nullptr, 0, &m_pdhQuery);
    if (status != ERROR_SUCCESS) {
        qDebug() << "[SystemMonitor] PdhOpenQuery failed:" << status;
        return;
    }

    // 优先使用 Processor Information 计数器（Win8.1+，与任务管理器一致）
    status = PdhAddEnglishCounter(m_pdhQuery,
        L"\\Processor Information(_Total)\\% Processor Utility", 0, &m_pdhCpuCounter);
    if (status != ERROR_SUCCESS) {
        // 回退到传统计数器
        status = PdhAddEnglishCounter(m_pdhQuery,
            L"\\Processor(_Total)\\% Processor Time", 0, &m_pdhCpuCounter);
    }

    if (status == ERROR_SUCCESS) {
        PdhCollectQueryData(m_pdhQuery);
        m_pdhInitialized = true;
        Logger::instance()->info("[SystemMonitor] PDH CPU counter initialized successfully");
    } else {
        qDebug() << "[SystemMonitor] PdhAddEnglishCounter failed:" << status;
    }
}

void SystemMonitor::cleanupPlatformResources()
{
    if (m_pdhQuery) {
        PdhCloseQuery(m_pdhQuery);
        m_pdhQuery = nullptr;
        m_pdhCpuCounter = nullptr;
        m_pdhInitialized = false;
    }
}

double SystemMonitor::getCpuUsage()
{
    if (!m_pdhInitialized || !m_pdhCpuCounter) {
        return m_cpuUsage;
    }

    PDH_STATUS status = PdhCollectQueryData(m_pdhQuery);
    if (status != ERROR_SUCCESS) {
        qDebug() << "[SystemMonitor] PdhCollectQueryData failed:" << status;
        return m_cpuUsage;
    }

    PDH_FMT_COUNTERVALUE counterValue;
    status = PdhGetFormattedCounterValue(m_pdhCpuCounter, PDH_FMT_DOUBLE, nullptr, &counterValue);
    if (status == ERROR_SUCCESS) {
        double usage = counterValue.doubleValue;
        return std::clamp(usage, 0.0, 100.0);
    }

    return m_cpuUsage;
}

double SystemMonitor::getMemoryUsage(double& usedMB, double& totalMB)
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (!GlobalMemoryStatusEx(&memInfo)) {
        Logger::instance()->info("[SystemMonitor] GlobalMemoryStatusEx 失败");
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

    Logger::instance()->info("[SystemMonitor] Linux /proc 初始化成功");
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
        Logger::instance()->info("[SystemMonitor] 无法打开 /proc/stat");
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
        Logger::instance()->info("[SystemMonitor] 无法打开 /proc/meminfo");
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
    Logger::instance()->info("[SystemMonitor] MacOS 平台暂未实现");
}

void SystemMonitor::cleanupPlatformResources()
{
    // MacOS 清理
}

double SystemMonitor::getCpuUsage()
{
    // 当前仅实现 Windows PDH 路径，macOS 暂未支持
    return 0.0;
}

double SystemMonitor::getMemoryUsage(double& usedMB, double& totalMB)
{
    // 当前仅实现 Windows PDH 路径，macOS 暂未支持
    usedMB = 0.0;
    totalMB = 0.0;
    return 0.0;
}

#endif  // Q_OS_MAC
