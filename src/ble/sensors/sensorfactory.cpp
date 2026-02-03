#include "sensorfactory.h"
#include "bookoomonitor.h"
#include "ble/sensordevice.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSensorFactory, "bridge.sensor.factory")

bool SensorFactory::isSensor(const QBluetoothDeviceInfo &device)
{
    if (BookooMonitor::isBookooMonitor(device)) {
        return true;
    }
    // Add more sensor types here as they are implemented
    return false;
}

QString SensorFactory::sensorType(const QBluetoothDeviceInfo &device)
{
    if (BookooMonitor::isBookooMonitor(device)) {
        return "BookooMonitor";
    }
    return QString();
}

SensorDevice* SensorFactory::createSensor(const QBluetoothDeviceInfo &device, QObject *parent)
{
    if (BookooMonitor::isBookooMonitor(device)) {
        qCInfo(lcSensorFactory) << "Creating Bookoo Monitor sensor for" << device.name();
        return new BookooMonitor(parent);
    }
    // Add more sensor types here

    qCWarning(lcSensorFactory) << "Unknown sensor type:" << device.name();
    return nullptr;
}
