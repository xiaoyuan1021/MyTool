#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <atomic>
#include <mqtt/async_client.h>
#include "config/mqtt_config.h"

/**
 * MQTT 客户端封装类
 * 
 * 基于 Paho MQTT C++ 库封装，提供 Qt 信号槽接口。
 * 通过 Qt 事件循环处理消息回调，保证线程安全。
 */
class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(const MqttConfig& config, QObject* parent = nullptr);
    ~MqttClient();

    /// 连接到 Broker
    void connect();

    /// 断开连接
    void disconnect();

    /// 发布消息
    /// @param topic 发布主题
    /// @param payload 消息内容
    /// @param qos QoS 等级 (0/1/2)
    /// @param retained 是否保留消息
    void publish(const QString& topic, const QString& payload, int qos = 1, bool retained = false);

    /// 订阅主题
    /// @param topic 订阅主题
    /// @param qos QoS 等级
    void subscribe(const QString& topic, int qos = 1);

    /// 取消订阅
    void unsubscribe(const QString& topic);

    /// 更新配置并重新连接
    void updateConfig(const MqttConfig& config);

    /// 是否已连接
    bool isConnected() const { return m_connected; }

    /// 获取当前配置
    const MqttConfig& config() const { return m_config; }

signals:
    /// 连接成功
    void connected();

    /// 连接断开
    void disconnected();

    /// 收到消息
    /// @param topic 消息主题
    /// @param payload 消息内容
    void messageReceived(const QString& topic, const QString& payload);

    /// 连接错误
    /// @param error 错误描述
    void connectionError(const QString& error);

    /// 重连尝试
    /// @param attempt 当前尝试次数
    void reconnectAttempt(int attempt);

private:
    /// 设置 Paho 回调
    void setupCallbacks();

    /// 启动自动重连
    void startReconnect();

    /// 停止自动重连
    void stopReconnect();

    MqttConfig m_config;
    std::unique_ptr<mqtt::async_client> m_client;
    std::atomic<bool> m_connected{false};
    int m_reconnectCount = 0;
    QTimer m_reconnectTimer;
};