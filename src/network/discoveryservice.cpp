#include "discoveryservice.h"
#include "core/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>

Q_LOGGING_CATEGORY(lcDiscovery, "bridge.discovery")

static const QByteArray DISCOVERY_REQUEST = "DECENTBRIDGE_DISCOVER";

DiscoveryService::DiscoveryService(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

DiscoveryService::~DiscoveryService()
{
    stop();
    stopMdns();
}

bool DiscoveryService::start()
{
    if (m_socket) {
        return true;
    }

    m_socket = new QUdpSocket(this);

    // Bind to discovery port on all interfaces
    if (!m_socket->bind(QHostAddress::Any, DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCWarning(lcDiscovery) << "Failed to bind discovery socket:" << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &DiscoveryService::onReadyRead);

    qCInfo(lcDiscovery) << "Discovery service listening on port" << DISCOVERY_PORT;

    startMdns();

    return true;
}

void DiscoveryService::stop()
{
    stopMdns();

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
        qCInfo(lcDiscovery) << "Discovery service stopped";
    }
}

void DiscoveryService::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();

        if (data.trimmed() == DISCOVERY_REQUEST) {
            qCDebug(lcDiscovery) << "Discovery request from" << datagram.senderAddress().toString();

            // Build response
            QJsonObject response;
            response["name"] = m_settings->bridgeName();
            response["httpPort"] = m_settings->httpPort();
            response["wsPort"] = m_settings->webSocketPort();
            response["version"] = APP_VERSION;

            QByteArray responseData = QJsonDocument(response).toJson(QJsonDocument::Compact);

            // Send response back to sender
            m_socket->writeDatagram(responseData, datagram.senderAddress(), datagram.senderPort());

            qCDebug(lcDiscovery) << "Sent discovery response:" << responseData;
        }
    }
}

void DiscoveryService::startMdns()
{
    if (m_mdnsServer) return;

    m_mdnsServer = new QMdnsEngine::Server(this);
    m_mdnsHostname = new QMdnsEngine::Hostname(m_mdnsServer, this);
    m_mdnsProvider = new QMdnsEngine::Provider(m_mdnsServer, m_mdnsHostname, this);

    connect(m_mdnsHostname, &QMdnsEngine::Hostname::hostnameChanged,
            this, [this](const QByteArray &hostname) {
        qCInfo(lcDiscovery) << "mDNS hostname registered:" << hostname;
    });

    QMdnsEngine::Service service;
    service.setType("_decentbridge._tcp.local.");
    service.setName(m_settings->bridgeName().toUtf8());
    service.setPort(static_cast<quint16>(m_settings->httpPort()));

    QMap<QByteArray, QByteArray> txt;
    txt.insert("version", APP_VERSION);
    txt.insert("ip", localIpAddress().toUtf8());
    txt.insert("port", QByteArray::number(m_settings->httpPort()));
    txt.insert("ws", QByteArray::number(m_settings->webSocketPort()));
    service.setAttributes(txt);

    m_mdnsProvider->update(service);

    qCInfo(lcDiscovery) << "mDNS advertising _decentbridge._tcp on port" << m_settings->httpPort()
                        << "hostname registered:" << m_mdnsHostname->isRegistered()
                        << "hostname:" << m_mdnsHostname->hostname();
}

QString DiscoveryService::localIpAddress()
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    return entry.ip().toString();
                }
            }
        }
    }
    return QString();
}

void DiscoveryService::stopMdns()
{
    delete m_mdnsProvider;
    m_mdnsProvider = nullptr;
    delete m_mdnsHostname;
    m_mdnsHostname = nullptr;
    delete m_mdnsServer;
    m_mdnsServer = nullptr;
}
