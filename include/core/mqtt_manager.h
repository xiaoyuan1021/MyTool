#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include "core/mqtt_client.h"
#include "data/detection_result_report.h"

/**
 * MQTT 边云协同管理器
 *
 * 三个核心功能：
 * 1. 检测结果上报 — 检测完成后自动通过 MQTT 发送检测报告
 * 2. 云端指令执行 — 订阅云端下发的控制指令并转发为信号
 * 3. 心跳保活 — 定时发送心跳表示设备在线
 */
class MqttManager : public QObject
{
    Q_OBJECT

public:
    explicit MqttManager(QObject* parent = nullptr);
    ~MqttManager();

    /// 初始化（连接、订阅、启动心跳）
    void initialize(const MqttConfig& config);

    bool isInitialized() const { return m_initialized; }
    bool isConnected() const { return m_client && m_client->isConnected(); }

    /// 获取底层客户端
    MqttClient* client() const { return m_client; }

    // ==================== 功能1: 结果上报 ====================

    /// 发布检测结果报告
    void publishResult(const DetectionResultReport& report);

    /// 发布任意 JSON 消息到指定主题
    void publishJson(const QString& topic, const QJsonObject& json);

    // ==================== 功能3: 心跳控制 ====================

    void startHeartbeat();
    void stopHeartbeat();

signals:
    // ==================== 功能2: 云端指令信号 ====================

    /// 采集指令：立即执行一次检测
    void captureRequested();

    /// 开始批量检测
    void startDetectionRequested();

    /// 停止批量检测
    void stopDetectionRequested();

    // 连接状态
    void mqttConnected();
    void mqttDisconnected();
    void mqttError(const QString& msg);

private slots:
    void onMqttConnected();
    void onMqttMessageReceived(const QString& topic, const QString& payload);
    void onSendHeartbeat();

private:
    void parseCommand(const QJsonObject& json);

    MqttClient* m_client = nullptr;
    QTimer m_heartbeatTimer;
    QElapsedTimer m_uptimeTimer;
    bool m_initialized = false;
};
