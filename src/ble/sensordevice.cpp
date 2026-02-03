#include "sensordevice.h"
#include <QLoggingCategory>
#include <QDateTime>

Q_LOGGING_CATEGORY(lcSensor, "bridge.sensor")

SensorDevice::SensorDevice(QObject *parent)
    : QObject(parent)
{
}

SensorDevice::~SensorDevice()
{
    disconnect();
}

void SensorDevice::connectToDevice(const QBluetoothDeviceInfo &device)
{
    if (m_controller) {
        disconnect();
    }

    m_name = device.name();
    m_address = device.address().toString();
    m_id = QString("%1_%2").arg(sensorType().toLower()).arg(m_address.replace(":", ""));

    qCInfo(lcSensor) << "Connecting to sensor" << m_name << "at" << m_address;

    m_controller = QLowEnergyController::createCentral(device, this);

    connect(m_controller, &QLowEnergyController::connected,
            this, &SensorDevice::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected,
            this, &SensorDevice::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::errorOccurred,
            this, &SensorDevice::onControllerError);
    connect(m_controller, &QLowEnergyController::serviceDiscovered,
            this, &SensorDevice::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &SensorDevice::onServiceDiscoveryFinished);

    m_controller->connectToDevice();
}

void SensorDevice::disconnect()
{
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }

    if (m_controller) {
        m_controller->disconnectFromDevice();
        delete m_controller;
        m_controller = nullptr;
    }

    if (m_connected) {
        m_connected = false;
        emit disconnected();
    }
}

void SensorDevice::onControllerConnected()
{
    qCInfo(lcSensor) << "Connected, discovering services...";
    m_controller->discoverServices();
}

void SensorDevice::onControllerDisconnected()
{
    qCInfo(lcSensor) << "Sensor disconnected:" << m_name;
    m_connected = false;
    emit disconnected();
}

void SensorDevice::onControllerError(QLowEnergyController::Error error)
{
    qCWarning(lcSensor) << "Controller error:" << error;
    emit errorOccurred(QString("BLE error: %1").arg(static_cast<int>(error)));
}

void SensorDevice::onServiceDiscovered(const QBluetoothUuid &uuid)
{
    qCDebug(lcSensor) << "Service discovered:" << uuid.toString();
}

void SensorDevice::onServiceDiscoveryFinished()
{
    qCInfo(lcSensor) << "Service discovery finished";

    m_service = m_controller->createServiceObject(serviceUuid(), this);
    if (!m_service) {
        qCWarning(lcSensor) << "Sensor service not found";
        emit errorOccurred("Sensor service not found");
        disconnect();
        return;
    }

    connect(m_service, &QLowEnergyService::stateChanged, this, [this](QLowEnergyService::ServiceState state) {
        if (state == QLowEnergyService::RemoteServiceDiscovered) {
            setupService();
            m_connected = true;
            emit connected();
        }
    });

    connect(m_service, &QLowEnergyService::characteristicChanged,
            this, &SensorDevice::onCharacteristicChanged);

    m_service->discoverDetails();
}

void SensorDevice::onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    Q_UNUSED(c)
    Q_UNUSED(value)
    // Override in subclasses
}

double SensorDevice::value(const QString &key) const
{
    for (const auto &channel : m_channels) {
        if (channel.key == key) {
            return channel.value;
        }
    }
    return 0;
}

void SensorDevice::updateChannel(const QString &key, double value)
{
    for (auto &channel : m_channels) {
        if (channel.key == key) {
            channel.value = value;
            emit dataUpdated(toSnapshot());
            return;
        }
    }
}

QJsonObject SensorDevice::toJson() const
{
    QJsonObject obj;
    obj["id"] = m_id;
    obj["name"] = m_name;
    obj["type"] = sensorType();

    QJsonArray channels;
    for (const auto &channel : m_channels) {
        QJsonObject ch;
        ch["key"] = channel.key;
        ch["type"] = channel.type;
        ch["unit"] = channel.unit;
        channels.append(ch);
    }
    obj["dataChannels"] = channels;

    return obj;
}

QJsonObject SensorDevice::toSnapshot() const
{
    QJsonObject obj;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    obj["id"] = m_id;

    QJsonObject values;
    for (const auto &channel : m_channels) {
        values[channel.key] = channel.value;
    }
    obj["values"] = values;

    return obj;
}
