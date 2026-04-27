#include "core/mqtt_manager.h"
#include "logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

MqttManager::MqttManager(QObject* parent)
    : QObject(parent)
{
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &MqttManager::onSendHeartbeat);
}

MqttManager::~MqttManager()
{
    stopHeartbeat();
}

void MqttManager::initialize(const MqttConfig& config)
{
    if (m_initialized) return;
    m_initialized = true;

    m_client = new MqttClient(config, this);

    connect(m_client, &MqttClient::connected,
            this, &MqttManager::onMqttConnected);
    connect(m_client, &MqttClient::disconnected,
            this, &MqttManager::mqttDisconnected);
    connect(m_client, &MqttClient::connectionError,
            this, &MqttManager::mqttError);
    connect(m_client, &MqttClient::messageReceived,
            this, &MqttManager::onMqttMessageReceived);

    if (config.enabled) {
        m_client->connect();
    }
}

// ==================== 功能1: 结果上报 ====================

void MqttManager::publishResult(const DetectionResultReport& report)
{
    if (!m_client || !m_client->isConnected()) return;

    QJsonObject json = report.toJson();
    json["deviceId"] = m_client->config().clientId;

    QJsonDocument doc(json);
    m_client->publish(
        m_client->config().publishTopic,
        QString::fromUtf8(doc.toJson(QJsonDocument::Compact)),
        m_client->config().qos);
}

void MqttManager::publishJson(const QString& topic, const QJsonObject& json)
{
    if (!m_client || !m_client->isConnected()) return;

    QJsonDocument doc(json);
    m_client->publish(
        topic,
        QString::fromUtf8(doc.toJson(QJsonDocument::Compact)),
        m_client->config().qos);
}

// ==================== 功能3: 心跳 ====================

void MqttManager::startHeartbeat()
{
    m_uptimeTimer.start();
    if (m_client && m_client->config().heartbeatIntervalMs > 0) {
        m_heartbeatTimer.start(m_client->config().heartbeatIntervalMs);
    }
}

void MqttManager::stopHeartbeat()
{
    m_heartbeatTimer.stop();
}

void MqttManager::onSendHeartbeat()
{
    if (!m_client || !m_client->isConnected()) return;

    QJsonObject payload;
    payload["deviceId"] = m_client->config().clientId;
    payload["status"] = "online";
    payload["ts"] = QDateTime::currentMSecsSinceEpoch();
    payload["uptime"] = m_uptimeTimer.elapsed() / 1000;

    QJsonDocument doc(payload);
    m_client->publish(
        m_client->config().heartbeatTopic,
        QString::fromUtf8(doc.toJson(QJsonDocument::Compact)),
        0);  // 心跳使用 QoS 0，减轻服务器压力
}

// ==================== 功能2: 指令处理 ====================

void MqttManager::onMqttConnected()
{
    if (m_client) {
        m_client->subscribe(m_client->config().subscribeTopic, m_client->config().qos);
    }
    startHeartbeat();
    emit mqttConnected();
}

void MqttManager::onMqttMessageReceived(const QString& topic, const QString& payload)
{
    if (!m_client) return;

    Logger::instance()->info(QString("[MQTT] 收到消息 topic=%1, payload=%2")
        .arg(topic).arg(payload));

    if (topic != m_client->config().subscribeTopic) return;

    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (!doc.isObject()) {
        Logger::instance()->warning("[MQTT] 指令格式错误，不是有效的JSON对象");
        return;
    }

    parseCommand(doc.object());
}

void MqttManager::parseCommand(const QJsonObject& json)
{
    QString cmd = json["cmd"].toString();

    Logger::instance()->info(QString("[MQTT] 解析到指令: %1").arg(cmd));

    if (cmd == "capture") {
        Logger::instance()->info("[MQTT] 触发: 采集检测");
        emit captureRequested();
    } else if (cmd == "start_detection") {
        Logger::instance()->info("[MQTT] 触发: 开始批量检测");
        emit startDetectionRequested();
    } else if (cmd == "stop_detection") {
        Logger::instance()->info("[MQTT] 触发: 停止批量检测");
        emit stopDetectionRequested();
    } else if (cmd == "ping") {
        Logger::instance()->info("[MQTT] 触发: 回复心跳");
        onSendHeartbeat();
    } else {
        Logger::instance()->warning(QString("[MQTT] 未知指令: %1").arg(cmd));
    }
}
