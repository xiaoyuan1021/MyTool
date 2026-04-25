#include "core/mqtt_client.h"
#include <QMetaObject>
#include <QMutexLocker>

MqttClient::MqttClient(const MqttConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_reconnectTimer(this)
{
    // 重连定时器信号槽
    QObject::connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_connected && m_config.enabled) {
            m_reconnectCount++;
            emit reconnectAttempt(m_reconnectCount);

            if (m_config.maxReconnectAttempts > 0 && m_reconnectCount >= m_config.maxReconnectAttempts) {
                stopReconnect();
                emit connectionError("达到最大重连次数限制");
                return;
            }
            connect();
        }
    });
}

MqttClient::~MqttClient()
{
    disconnect();
}

void MqttClient::connect()
{
    if (!m_config.enabled) {
        return;
    }

    if (m_connected) {
        return;
    }

    try {
        // 构造服务器地址
        std::string serverUri = "tcp://" +
            m_config.brokerHost.toStdString() + ":" +
            std::to_string(m_config.brokerPort);

        // 创建异步客户端 (serverURI, clientId, maxBufferedMessages)
        m_client = std::make_unique<mqtt::async_client>(
            serverUri,
            m_config.clientId.toStdString(),
            100
        );

        // 设置回调
        setupCallbacks();

        // 连接选项
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(60);
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(false); // 我们自己管理重连

        // 认证配置
        if (m_config.useAuth) {
            connOpts.set_user_name(m_config.username.toStdString());
            connOpts.set_password(m_config.password.toStdString());
        }

        // 执行连接
        m_client->connect(connOpts)->wait();

        m_connected.store(true, std::memory_order_release);
        m_reconnectCount = 0;
        emit connected();

    } catch (const mqtt::exception& ex) {
        m_connected.store(false, std::memory_order_release);
        emit connectionError(QString::fromStdString(ex.what()));

        // 启动自动重连
        if (m_config.autoReconnect) {
            startReconnect();
        }
    }
}

void MqttClient::disconnect()
{
    stopReconnect();

    if (!m_connected || !m_client) {
        return;
    }

    try {
        auto token = m_client->disconnect();
        token->wait();
    } catch (const mqtt::exception&) {
        // 忽略断开时的异常
    }

    m_connected.store(false, std::memory_order_release);
    m_client.reset();
    emit disconnected();
}

void MqttClient::publish(const QString& topic, const QString& payload, int qos, bool retained)
{
    if (!m_connected.load(std::memory_order_acquire) || !m_client) {
        return;
    }

    try {
        std::string payloadStr = payload.toStdString();
        m_client->publish(
            topic.toStdString(),
            payloadStr.c_str(),
            payloadStr.size(),
            qos,
            retained
        );
    } catch (const mqtt::exception& ex) {
        emit connectionError(QString("发布消息失败: %1").arg(QString::fromStdString(ex.what())));
    }
}

void MqttClient::subscribe(const QString& topic, int qos)
{
    if (!m_connected.load(std::memory_order_acquire) || !m_client) {
        return;
    }

    try {
        m_client->subscribe(topic.toStdString(), qos);
    } catch (const mqtt::exception& ex) {
        emit connectionError(QString("订阅主题失败: %1").arg(QString::fromStdString(ex.what())));
    }
}

void MqttClient::unsubscribe(const QString& topic)
{
    if (!m_connected.load(std::memory_order_acquire) || !m_client) {
        return;
    }

    try {
        m_client->unsubscribe(topic.toStdString());
    } catch (const mqtt::exception& ex) {
        emit connectionError(QString("取消订阅失败: %1").arg(QString::fromStdString(ex.what())));
    }
}

void MqttClient::updateConfig(const MqttConfig& config)
{
    bool wasConnected = m_connected.load(std::memory_order_acquire);
    if (wasConnected) {
        disconnect();
    }
    m_config = config;
    if (wasConnected && m_config.enabled) {
        connect();
    }
}

void MqttClient::setupCallbacks()
{
    if (!m_client) {
        return;
    }

    // 使用 lambda 回调，通过 QMetaObject::invokeMethod 转发到 Qt 主线程
    m_client->set_message_callback([this](mqtt::const_message_ptr msg) {
        QString topic = QString::fromStdString(msg->get_topic());
        QString payload = QString::fromStdString(msg->get_payload_str());

        // 通过 QMetaObject::invokeMethod 确保在 Qt 主线程中触发信号
        QMetaObject::invokeMethod(this, [this, topic, payload]() {
            emit messageReceived(topic, payload);
        }, Qt::QueuedConnection);
    });

    // 连接丢失回调
    m_client->set_connection_lost_handler([this](const std::string& cause) mutable {
        m_connected.store(false, std::memory_order_release);
        QMetaObject::invokeMethod(this, [this, cause]() {
            emit disconnected();
            emit connectionError(QString("连接丢失: %1").arg(QString::fromStdString(cause)));

            // 启动自动重连
            if (m_config.autoReconnect) {
                startReconnect();
            }
        }, Qt::QueuedConnection);
    });
}

void MqttClient::startReconnect()
{
    if (!m_config.autoReconnect) {
        return;
    }

    m_reconnectCount = 0;
    m_reconnectTimer.setInterval(m_config.reconnectIntervalMs);
    m_reconnectTimer.start();
}

void MqttClient::stopReconnect()
{
    m_reconnectTimer.stop();
    m_reconnectCount = 0;
}