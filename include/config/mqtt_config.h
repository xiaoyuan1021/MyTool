#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

/**
 * MQTT 配置结构体
 * 
 * 用于存储 MQTT 连接、认证、Topic、上报策略等配置，
 * 支持 JSON 序列化/反序列化。
 */
struct MqttConfig
{
    // ==================== 连接配置 ====================
    bool enabled = false;               ///< 是否启用 MQTT
    QString brokerHost = "localhost";   ///< Broker 地址
    int brokerPort = 1883;              ///< Broker 端口
    QString clientId = "VisionTool";    ///< 客户端 ID

    // ==================== 认证配置 ====================
    bool useAuth = false;               ///< 是否使用认证
    QString username;                   ///< 用户名
    QString password;                   ///< 密码

    // ==================== Topic 配置 ====================
    QString publishTopic = "visiontool/results";     ///< 发布主题
    QString subscribeTopic = "visiontool/commands";  ///< 订阅主题
    int qos = 1;                        ///< QoS 等级 (0/1/2)

    // ==================== 上报策略 ====================
    int reportIntervalMs = 0;           ///< 上报间隔 (ms), 0=每帧立即上报
    bool reportOnStateChange = true;    ///< 仅状态变化时上报
    bool reportRegions = true;          ///< 是否上报区域详情
    bool reportBarcodes = true;         ///< 是否上报条码结果

    // ==================== 重连配置 ====================
    bool autoReconnect = true;          ///< 自动重连
    int reconnectIntervalMs = 5000;     ///< 重连间隔 (ms)
    int maxReconnectAttempts = 10;      ///< 最大重连次数，0=无限重连

    // ==================== JSON 序列化 ====================

    /**
     * 转换为 JSON 对象
     */
    QJsonObject toJson() const
    {
        QJsonObject json;

        // 连接配置
        QJsonObject connectionObj;
        connectionObj["enabled"] = enabled;
        connectionObj["brokerHost"] = brokerHost;
        connectionObj["brokerPort"] = brokerPort;
        connectionObj["clientId"] = clientId;
        json["connection"] = connectionObj;

        // 认证配置
        QJsonObject authObj;
        authObj["useAuth"] = useAuth;
        authObj["username"] = username;
        authObj["password"] = password;  // 注意：实际生产环境应加密存储
        json["auth"] = authObj;

        // Topic 配置
        QJsonObject topicObj;
        topicObj["publishTopic"] = publishTopic;
        topicObj["subscribeTopic"] = subscribeTopic;
        topicObj["qos"] = qos;
        json["topic"] = topicObj;

        // 上报策略
        QJsonObject reportObj;
        reportObj["reportIntervalMs"] = reportIntervalMs;
        reportObj["reportOnStateChange"] = reportOnStateChange;
        reportObj["reportRegions"] = reportRegions;
        reportObj["reportBarcodes"] = reportBarcodes;
        json["report"] = reportObj;

        // 重连配置
        QJsonObject reconnectObj;
        reconnectObj["autoReconnect"] = autoReconnect;
        reconnectObj["reconnectIntervalMs"] = reconnectIntervalMs;
        reconnectObj["maxReconnectAttempts"] = maxReconnectAttempts;
        json["reconnect"] = reconnectObj;

        return json;
    }

    /**
     * 从 JSON 对象解析
     */
    void fromJson(const QJsonObject& json)
    {
        // 连接配置
        if (json.contains("connection")) {
            QJsonObject connectionObj = json["connection"].toObject();
            enabled = connectionObj["enabled"].toBool(false);
            brokerHost = connectionObj["brokerHost"].toString("localhost");
            brokerPort = connectionObj["brokerPort"].toInt(1883);
            clientId = connectionObj["clientId"].toString("VisionTool");
        }

        // 认证配置
        if (json.contains("auth")) {
            QJsonObject authObj = json["auth"].toObject();
            useAuth = authObj["useAuth"].toBool(false);
            username = authObj["username"].toString();
            password = authObj["password"].toString();
        }

        // Topic 配置
        if (json.contains("topic")) {
            QJsonObject topicObj = json["topic"].toObject();
            publishTopic = topicObj["publishTopic"].toString("visiontool/results");
            subscribeTopic = topicObj["subscribeTopic"].toString("visiontool/commands");
            qos = topicObj["qos"].toInt(1);
        }

        // 上报策略
        if (json.contains("report")) {
            QJsonObject reportObj = json["report"].toObject();
            reportIntervalMs = reportObj["reportIntervalMs"].toInt(0);
            reportOnStateChange = reportObj["reportOnStateChange"].toBool(true);
            reportRegions = reportObj["reportRegions"].toBool(true);
            reportBarcodes = reportObj["reportBarcodes"].toBool(true);
        }

        // 重连配置
        if (json.contains("reconnect")) {
            QJsonObject reconnectObj = json["reconnect"].toObject();
            autoReconnect = reconnectObj["autoReconnect"].toBool(true);
            reconnectIntervalMs = reconnectObj["reconnectIntervalMs"].toInt(5000);
            maxReconnectAttempts = reconnectObj["maxReconnectAttempts"].toInt(10);
        }
    }

    /**
     * 序列化为 JSON 字符串
     */
    QString toJsonString() const
    {
        QJsonDocument doc(toJson());
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    /**
     * 从 JSON 字符串解析
     */
    bool fromJsonString(const QString& jsonString)
    {
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        if (!doc.isObject()) {
            return false;
        }
        fromJson(doc.object());
        return true;
    }

    /**
     * 验证配置是否有效
     * @return 错误描述，空字符串表示有效
     */
    QString validate() const
    {
        if (!enabled) {
            return QString();  // 未启用时不做验证
        }
        if (brokerHost.isEmpty()) {
            return "Broker 地址不能为空";
        }
        if (brokerPort < 1 || brokerPort > 65535) {
            return "Broker 端口无效 (1-65535)";
        }
        if (publishTopic.isEmpty()) {
            return "发布主题不能为空";
        }
        if (qos < 0 || qos > 2) {
            return "QoS 等级无效 (0-2)";
        }
        if (useAuth && username.isEmpty()) {
            return "启用认证时用户名不能为空";
        }
        if (reconnectIntervalMs < 1000) {
            return "重连间隔不能小于 1000ms";
        }
        return QString();  // 有效
    }
};