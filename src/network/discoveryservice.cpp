#include "discoveryservice.h"
#include "core/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkDatagram>

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
    return true;
}

void DiscoveryService::stop()
{
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
