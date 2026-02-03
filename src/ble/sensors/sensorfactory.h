#ifndef SENSORFACTORY_H
#define SENSORFACTORY_H

#include <QBluetoothDeviceInfo>

class SensorDevice;

/**
 * @brief Factory for creating sensor device instances
 */
class SensorFactory
{
public:
    /**
     * Check if a discovered BLE device is a known sensor
     */
    static bool isSensor(const QBluetoothDeviceInfo &device);

    /**
     * Get the sensor type name for a device
     */
    static QString sensorType(const QBluetoothDeviceInfo &device);

    /**
     * Create a sensor device instance for the given BLE device
     */
    static SensorDevice* createSensor(const QBluetoothDeviceInfo &device, QObject *parent = nullptr);
};

#endif // SENSORFACTORY_H
