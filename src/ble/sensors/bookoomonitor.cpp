#include "bookoomonitor.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBookooMonitor, "bridge.sensor.bookoo")

// Bookoo Espresso Monitor UUIDs
static const QBluetoothUuid BOOKOO_EM_SERVICE(QString("0000FFE0-0000-1000-8000-00805F9B34FB"));
static const QBluetoothUuid BOOKOO_EM_NOTIFY(QString("0000FFE1-0000-1000-8000-00805F9B34FB"));

BookooMonitor::BookooMonitor(QObject *parent)
    : SensorDevice(parent)
{
    // Define data channels
    m_channels.append({
        "pressure",
        "number",
        "bar",
        0
    });
}

bool BookooMonitor::isBookooMonitor(const QBluetoothDeviceInfo &device)
{
    QString name = device.name().toUpper();
    // Bookoo Espresso Monitor typically advertises as "BOOKOO_EM" or similar
    return name.contains("BOOKOO") && (name.contains("EM") || name.contains("MONITOR"));
}

QBluetoothUuid BookooMonitor::serviceUuid() const
{
    return BOOKOO_EM_SERVICE;
}

void BookooMonitor::setupService()
{
    // Subscribe to notifications
    auto characteristic = m_service->characteristic(BOOKOO_EM_NOTIFY);
    if (characteristic.isValid()) {
        auto descriptor = characteristic.descriptor(
            QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (descriptor.isValid()) {
            m_service->writeDescriptor(descriptor,
                QLowEnergyCharacteristic::CCCDEnableNotification);
            qCInfo(lcBookooMonitor) << "Subscribed to pressure notifications";
        }
    }
}

void BookooMonitor::onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (c.uuid() == BOOKOO_EM_NOTIFY) {
        parseData(value);
    }
}

void BookooMonitor::parseData(const QByteArray &data)
{
    if (data.size() < 2) return;

    // Bookoo EM sends pressure as a 16-bit value in 0.1 bar units
    // Format may vary - this is a common pattern
    int rawPressure = (static_cast<uint8_t>(data[0]) << 8) | static_cast<uint8_t>(data[1]);
    m_pressure = rawPressure / 10.0;

    updateChannel("pressure", m_pressure);
    qCDebug(lcBookooMonitor) << "Pressure:" << m_pressure << "bar";
}
