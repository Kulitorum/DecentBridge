#include "bridge.h"
#include "settings.h"
#include "ble/blemanager.h"
#include "ble/de1device.h"
#include "ble/scaledevice.h"
#include "ble/scales/scalefactory.h"
#include "network/httpserver.h"
#include "network/websocketserver.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBridge, "bridge.core")

Bridge::Bridge(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_bleManager(std::make_unique<BLEManager>())
    , m_de1(std::make_unique<DE1Device>())
    , m_httpServer(std::make_unique<HttpServer>(this))
    , m_wsServer(std::make_unique<WebSocketServer>(this))
{
    setupConnections();
}

Bridge::~Bridge()
{
    stop();
}

void Bridge::setupConnections()
{
    // BLE Manager -> Bridge
    connect(m_bleManager.get(), &BLEManager::de1Discovered,
            this, &Bridge::onDe1Discovered);
    connect(m_bleManager.get(), &BLEManager::scaleDiscovered,
            this, &Bridge::onScaleDiscovered);

    // DE1 -> Bridge
    connect(m_de1.get(), &DE1Device::connectedChanged,
            this, &Bridge::onDe1ConnectionChanged);

    // DE1 -> WebSocket (real-time updates)
    connect(m_de1.get(), &DE1Device::shotSampleReceived,
            m_wsServer.get(), &WebSocketServer::broadcastShotSample);
    connect(m_de1.get(), &DE1Device::stateChanged,
            m_wsServer.get(), &WebSocketServer::broadcastMachineState);
    connect(m_de1.get(), &DE1Device::waterLevelsChanged,
            m_wsServer.get(), &WebSocketServer::broadcastWaterLevels);
}

bool Bridge::start()
{
    if (m_running) {
        return true;
    }

    // Start HTTP server
    if (!m_httpServer->start(m_settings->httpPort())) {
        emit error("Failed to start HTTP server on port " +
                   QString::number(m_settings->httpPort()));
        return false;
    }

    // Start WebSocket server
    if (!m_wsServer->start(m_settings->webSocketPort())) {
        emit error("Failed to start WebSocket server on port " +
                   QString::number(m_settings->webSocketPort()));
        m_httpServer->stop();
        return false;
    }

    // Start BLE scanning
    m_bleManager->startScan();

    m_running = true;
    emit started();
    return true;
}

void Bridge::stop()
{
    if (!m_running) {
        return;
    }

    m_bleManager->stopScan();
    m_de1->disconnect();

    if (m_scale) {
        m_scale->disconnect();
        delete m_scale;
        m_scale = nullptr;
    }

    m_httpServer->stop();
    m_wsServer->stop();

    m_running = false;
    emit stopped();
}

void Bridge::onDe1Discovered(const QBluetoothDeviceInfo &device)
{
    if (m_de1->isConnected() || m_de1->isConnecting()) {
        return;
    }

    // Check if we should auto-connect or if address matches
    if (m_settings->autoConnect() ||
        device.address().toString() == m_settings->de1Address()) {
        qCInfo(lcBridge) << "Connecting to DE1:" << device.name();
        m_bleManager->stopScan();
        m_de1->connectToDevice(device);
    }
}

void Bridge::onScaleDiscovered(const QBluetoothDeviceInfo &device)
{
    if (m_scale && m_scale->isConnected()) {
        return;
    }

    if (!m_settings->autoConnectScale()) {
        return;
    }

    // Use ScaleFactory to create the appropriate scale type
    auto scale = ScaleFactory::createScale(device, this);

    if (!scale) {
        qCDebug(lcBridge) << "Unknown scale type:" << device.name();
        return;
    }

    qCInfo(lcBridge) << "Connecting to scale:" << device.name();

    // Clean up old scale if any
    if (m_scale) {
        m_scale->deleteLater();
    }
    m_scale = scale.release();

    // Connect scale signals
    connect(m_scale, &ScaleDevice::connectedChanged, this, [this]() {
        onScaleConnectionChanged(m_scale ? m_scale->isConnected() : false);
    });
    connect(m_scale, &ScaleDevice::weightChanged, this, [this](double weight) {
        m_wsServer->broadcastScaleWeight(weight, m_scale ? m_scale->flowRate() : 0.0);
    });

    m_scale->connectToDevice(device);
}

void Bridge::onDe1ConnectionChanged(bool connected)
{
    if (connected) {
        qCInfo(lcBridge) << "DE1 connected";
        emit de1Connected();
    } else {
        qCInfo(lcBridge) << "DE1 disconnected";
        emit de1Disconnected();

        // Resume scanning
        if (m_running && m_settings->autoConnect()) {
            m_bleManager->startScan();
        }
    }
}

void Bridge::onScaleConnectionChanged(bool connected)
{
    if (connected) {
        qCInfo(lcBridge) << "Scale connected:" << m_scale->name();
        emit scaleConnected();
    } else {
        qCInfo(lcBridge) << "Scale disconnected";
        emit scaleDisconnected();

        // Resume scanning for scales
        if (m_running && m_settings->autoConnectScale()) {
            m_bleManager->startScan();
        }
    }
}
