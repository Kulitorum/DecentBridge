#include "websocketserver.h"
#include "core/bridge.h"
#include "ble/de1device.h"
#include "ble/scaledevice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(lcWebSocket, "bridge.websocket")

WebSocketServer::WebSocketServer(Bridge *bridge, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
{
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start(int port)
{
    if (m_server) {
        return m_server->isListening();
    }

    m_server = new QWebSocketServer(
        "DecentBridge",
        QWebSocketServer::NonSecureMode,
        this
    );

    if (!m_server->listen(QHostAddress::Any, port)) {
        qCWarning(lcWebSocket) << "Failed to start WebSocket server:" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);

    qCInfo(lcWebSocket) << "WebSocket server listening on port" << port;
    return true;
}

void WebSocketServer::stop()
{
    if (m_server) {
        // Close all client connections
        for (auto &subscribers : m_subscribers) {
            for (QWebSocket *socket : subscribers) {
                socket->close();
            }
        }
        m_subscribers.clear();

        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

void WebSocketServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QWebSocket *socket = m_server->nextPendingConnection();

        connect(socket, &QWebSocket::textMessageReceived, this, &WebSocketServer::onTextMessage);
        connect(socket, &QWebSocket::binaryMessageReceived, this, &WebSocketServer::onBinaryMessage);
        connect(socket, &QWebSocket::disconnected, this, &WebSocketServer::onDisconnected);

        // Determine channel from request path
        QUrl requestUrl = socket->requestUrl();
        Channel channel = channelFromPath(requestUrl.path());

        m_subscribers[channel].insert(socket);
        qCDebug(lcWebSocket) << "Client connected to" << requestUrl.path();

        // Send initial state immediately
        switch (channel) {
            case Channel::MachineSnapshot:
                if (m_bridge->de1() && m_bridge->de1()->isConnected()) {
                    // TODO: Send current snapshot
                }
                break;
            case Channel::ScaleSnapshot:
                if (m_bridge->scale() && m_bridge->scale()->isConnected()) {
                    QJsonObject obj;
                    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                    obj["weight"] = m_bridge->scale()->weight();
                    obj["weightFlow"] = m_bridge->scale()->flowRate();
                    obj["batteryLevel"] = m_bridge->scale()->batteryLevel();
                    socket->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
                }
                break;
            default:
                break;
        }
    }
}

void WebSocketServer::onTextMessage(const QString &message)
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    // Handle incoming commands (for raw channel)
    qCDebug(lcWebSocket) << "Received:" << message;

    // Parse JSON command if on raw channel
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        // TODO: Handle raw DE1 commands
    }
}

void WebSocketServer::onBinaryMessage(const QByteArray &message)
{
    Q_UNUSED(message)
    // Binary messages not currently used
}

void WebSocketServer::onDisconnected()
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) return;

    // Remove from all subscriber lists
    for (auto &subscribers : m_subscribers) {
        subscribers.remove(socket);
    }

    socket->deleteLater();
    qCDebug(lcWebSocket) << "Client disconnected";
}

WebSocketServer::Channel WebSocketServer::channelFromPath(const QString &path)
{
    if (path == "/ws/v1/machine/snapshot") {
        return Channel::MachineSnapshot;
    } else if (path == "/ws/v1/machine/shotSettings") {
        return Channel::ShotSettings;
    } else if (path == "/ws/v1/machine/waterLevels") {
        return Channel::WaterLevels;
    } else if (path == "/ws/v1/scale/snapshot") {
        return Channel::ScaleSnapshot;
    } else if (path == "/ws/v1/machine/raw") {
        return Channel::Raw;
    }
    return Channel::MachineSnapshot; // Default
}

void WebSocketServer::broadcast(Channel channel, const QByteArray &data)
{
    for (QWebSocket *socket : m_subscribers[channel]) {
        if (socket->isValid()) {
            socket->sendTextMessage(QString::fromUtf8(data));
        }
    }
}

void WebSocketServer::broadcastShotSample(const QJsonObject &sample)
{
    QByteArray data = QJsonDocument(sample).toJson(QJsonDocument::Compact);
    broadcast(Channel::MachineSnapshot, data);
}

void WebSocketServer::broadcastMachineState(const QJsonObject &state)
{
    QByteArray data = QJsonDocument(state).toJson(QJsonDocument::Compact);
    broadcast(Channel::MachineSnapshot, data);
}

void WebSocketServer::broadcastWaterLevels(const QJsonObject &levels)
{
    QByteArray data = QJsonDocument(levels).toJson(QJsonDocument::Compact);
    broadcast(Channel::WaterLevels, data);
}

void WebSocketServer::broadcastScaleWeight(double weight, double flow)
{
    QJsonObject obj;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    obj["weight"] = weight;
    obj["weightFlow"] = flow;

    if (m_bridge->scale()) {
        obj["batteryLevel"] = m_bridge->scale()->batteryLevel();
    }

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    broadcast(Channel::ScaleSnapshot, data);
}

void WebSocketServer::broadcastShotSettings(const QJsonObject &settings)
{
    QByteArray data = QJsonDocument(settings).toJson(QJsonDocument::Compact);
    broadcast(Channel::ShotSettings, data);
}
