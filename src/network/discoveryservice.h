#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <QObject>
#include <QUdpSocket>

class Settings;

/**
 * @brief UDP-based discovery service for DecentBridge
 *
 * Listens for discovery broadcasts on port 19741 and responds with
 * bridge information (name, HTTP port, WebSocket port).
 *
 * Discovery protocol:
 *   Request:  "DECENTBRIDGE_DISCOVER"
 *   Response: JSON {"name": "...", "httpPort": 8080, "wsPort": 8081, "version": "0.1.0"}
 */
class DiscoveryService : public QObject
{
    Q_OBJECT

public:
    static constexpr int DISCOVERY_PORT = 19741;

    explicit DiscoveryService(Settings *settings, QObject *parent = nullptr);
    ~DiscoveryService();

    bool start();
    void stop();

    bool isRunning() const { return m_socket != nullptr; }

private slots:
    void onReadyRead();

private:
    Settings *m_settings;
    QUdpSocket *m_socket = nullptr;
};

#endif // DISCOVERYSERVICE_H
