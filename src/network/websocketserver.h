#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QSet>
#include <QMap>

class Bridge;

/**
 * @brief WebSocket server for real-time data streaming
 *
 * Endpoints (matching ReaPrime API):
 *   /ws/v1/machine/snapshot   - Real-time machine telemetry (~5Hz during shots)
 *   /ws/v1/machine/shotSettings - Shot settings updates
 *   /ws/v1/machine/waterLevels  - Water level notifications
 *   /ws/v1/scale/snapshot     - Real-time scale weight data
 *
 * Clients connect to a specific endpoint and receive JSON messages
 * whenever that data changes.
 */
class WebSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketServer(Bridge *bridge, QObject *parent = nullptr);
    ~WebSocketServer();

    bool start(int port);
    void stop();

    bool isRunning() const { return m_server && m_server->isListening(); }

public slots:
    // Called by Bridge/DE1 when data changes
    void broadcastShotSample(const QJsonObject &sample);
    void broadcastMachineState(const QJsonObject &state);
    void broadcastWaterLevels(const QJsonObject &levels);
    void broadcastScaleWeight(double weight, double flow);
    void broadcastShotSettings(const QJsonObject &settings);

private slots:
    void onNewConnection();
    void onTextMessage(const QString &message);
    void onBinaryMessage(const QByteArray &message);
    void onDisconnected();

private:
    enum class Channel {
        MachineSnapshot,
        ShotSettings,
        WaterLevels,
        ScaleSnapshot,
        Raw
    };

    Channel channelFromPath(const QString &path);
    void broadcast(Channel channel, const QByteArray &data);

    Bridge *m_bridge;
    QWebSocketServer *m_server = nullptr;
    QMap<Channel, QSet<QWebSocket*>> m_subscribers;
};

#endif // WEBSOCKETSERVER_H
