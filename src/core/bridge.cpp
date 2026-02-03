#include "bridge.h"
#include "settings.h"
#include "ble/blemanager.h"
#include "ble/de1device.h"
#include "ble/scaledevice.h"
#include "ble/sensordevice.h"
#include "ble/scales/scalefactory.h"
#include "ble/sensors/sensorfactory.h"
#include "network/httpserver.h"
#include "network/websocketserver.h"
#include "network/discoveryservice.h"

#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcBridge, "bridge.core")

Bridge::Bridge(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_bleManager(std::make_unique<BLEManager>())
    , m_de1(std::make_unique<DE1Device>())
    , m_httpServer(std::make_unique<HttpServer>(this))
    , m_wsServer(std::make_unique<WebSocketServer>(this))
    , m_discoveryService(std::make_unique<DiscoveryService>(settings))
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
    connect(m_bleManager.get(), &BLEManager::sensorDiscovered,
            this, &Bridge::onSensorDiscovered);

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

    // Start discovery service (for network discovery)
    if (!m_discoveryService->start()) {
        qCWarning(lcBridge) << "Failed to start discovery service (non-fatal)";
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

    // Disconnect all sensors
    for (auto sensor : m_sensors) {
        sensor->disconnect();
        delete sensor;
    }
    m_sensors.clear();

    m_httpServer->stop();
    m_wsServer->stop();
    m_discoveryService->stop();

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
    // Auto-connect disabled - scales must be connected manually via connectToScale()
    if (!m_settings->autoConnectScale()) {
        return;
    }

    connectToScale(device);
}

void Bridge::connectToScale(const QBluetoothDeviceInfo &device)
{
    qCInfo(lcBridge) << "connectToScale called for:" << device.name() << device.address().toString();

    // Don't connect if already connected
    if (m_scale && m_scale->isConnected()) {
        qCInfo(lcBridge) << "Scale already connected, ignoring:" << device.name();
        return;
    }

    // If a connection is "in progress" but taking too long, allow override
    // This handles cases where previous connection attempt got stuck
    if (m_scaleConnecting) {
        qCWarning(lcBridge) << "Previous connection attempt stuck, cleaning up";
        if (m_scale) {
            m_scale->deleteLater();
            m_scale = nullptr;
        }
        m_scaleConnecting = false;
    }

    // Use ScaleFactory to create the appropriate scale type
    auto scale = ScaleFactory::createScale(device, this);

    if (!scale) {
        qCWarning(lcBridge) << "Unknown scale type, cannot create:" << device.name();
        return;
    }

    qCInfo(lcBridge) << "Connecting to scale:" << device.name() << "type:" << scale->type();
    m_scaleConnecting = true;

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
    // Handle connection errors
    connect(m_scale, &ScaleDevice::errorOccurred, this, [this](const QString &error) {
        qCWarning(lcBridge) << "Scale connection error:" << error;
        m_scaleConnecting = false;
    });

    // Connection timeout - reset flag after 15 seconds if still connecting
    QTimer::singleShot(15000, this, [this]() {
        if (m_scaleConnecting && (!m_scale || !m_scale->isConnected())) {
            qCWarning(lcBridge) << "Scale connection timeout, resetting";
            m_scaleConnecting = false;
        }
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
    m_scaleConnecting = false; // Connection attempt finished (success or failure)

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

void Bridge::disconnectScale()
{
    m_scaleConnecting = false;
    if (m_scale) {
        qCInfo(lcBridge) << "Disconnecting scale:" << m_scale->name();
        m_scale->disconnect();
        m_scale->deleteLater();
        m_scale = nullptr;
    }
}

// Sensor methods
SensorDevice* Bridge::sensor(const QString &id) const
{
    for (auto sensor : m_sensors) {
        if (sensor->id() == id) {
            return sensor;
        }
    }
    return nullptr;
}

void Bridge::onSensorDiscovered(const QBluetoothDeviceInfo &device)
{
    // Auto-connect to sensors when discovered
    connectToSensor(device);
}

void Bridge::connectToSensor(const QBluetoothDeviceInfo &device)
{
    // Check if already connected to this sensor
    QString address = device.address().toString();
    for (auto sensor : m_sensors) {
        if (sensor->address() == address) {
            qCInfo(lcBridge) << "Sensor already connected:" << device.name();
            return;
        }
    }

    auto sensor = SensorFactory::createSensor(device, this);
    if (!sensor) {
        qCWarning(lcBridge) << "Unknown sensor type:" << device.name();
        return;
    }

    qCInfo(lcBridge) << "Connecting to sensor:" << device.name();

    connect(sensor, &SensorDevice::connected, this, &Bridge::onSensorConnected);
    connect(sensor, &SensorDevice::disconnected, this, &Bridge::onSensorDisconnected);
    connect(sensor, &SensorDevice::dataUpdated, this, &Bridge::onSensorDataUpdated);

    m_sensors.append(sensor);
    sensor->connectToDevice(device);
}

void Bridge::disconnectSensor(const QString &id)
{
    for (int i = 0; i < m_sensors.size(); ++i) {
        if (m_sensors[i]->id() == id) {
            qCInfo(lcBridge) << "Disconnecting sensor:" << m_sensors[i]->name();
            m_sensors[i]->disconnect();
            m_sensors[i]->deleteLater();
            m_sensors.removeAt(i);
            emit sensorDisconnected(id);
            return;
        }
    }
}

void Bridge::onSensorConnected()
{
    auto sensor = qobject_cast<SensorDevice*>(sender());
    if (sensor) {
        qCInfo(lcBridge) << "Sensor connected:" << sensor->name();
        emit sensorConnected(sensor);
    }
}

void Bridge::onSensorDisconnected()
{
    auto sensor = qobject_cast<SensorDevice*>(sender());
    if (sensor) {
        QString id = sensor->id();
        qCInfo(lcBridge) << "Sensor disconnected:" << sensor->name();

        // Remove from list
        m_sensors.removeAll(sensor);
        sensor->deleteLater();

        emit sensorDisconnected(id);
    }
}

void Bridge::onSensorDataUpdated(const QJsonObject &data)
{
    auto sensor = qobject_cast<SensorDevice*>(sender());
    if (sensor) {
        emit sensorDataUpdated(sensor->id(), data);
        m_wsServer->broadcastSensorData(sensor->id(), data);
    }
}
