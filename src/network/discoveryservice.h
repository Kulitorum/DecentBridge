#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <QObject>
#include <QUdpSocket>

#include <qmdnsengine/server.h>
#include <qmdnsengine/hostname.h>
#include <qmdnsengine/provider.h>
#include <qmdnsengine/service.h>

class Settings;

/**
 * @brief Discovery service for DecentBridge (UDP + mDNS)
 *
 * Provides two discovery mechanisms:
 * 1. Custom UDP protocol on port 19741 (legacy)
 * 2. mDNS/Zeroconf advertisement as _decentbridge._tcp
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
    void startMdns();
    void stopMdns();
    static QString localIpAddress();

    Settings *m_settings;
    QUdpSocket *m_socket = nullptr;

    // mDNS
    QMdnsEngine::Server *m_mdnsServer = nullptr;
    QMdnsEngine::Hostname *m_mdnsHostname = nullptr;
    QMdnsEngine::Provider *m_mdnsProvider = nullptr;
};

#endif // DISCOVERYSERVICE_H
