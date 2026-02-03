#ifndef BOOKOOMONITOR_H
#define BOOKOOMONITOR_H

#include "ble/sensordevice.h"

/**
 * @brief Bookoo Espresso Monitor - BLE pressure sensor
 *
 * The Bookoo EM is a pressure sensor that attaches to the portafilter
 * and provides real-time pressure readings during extraction.
 */
class BookooMonitor : public SensorDevice
{
    Q_OBJECT

public:
    explicit BookooMonitor(QObject *parent = nullptr);

    QString sensorType() const override { return "BookooMonitor"; }
    double pressure() const { return m_pressure; }

    // Detection
    static bool isBookooMonitor(const QBluetoothDeviceInfo &device);

protected:
    void setupService() override;
    QBluetoothUuid serviceUuid() const override;
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value) override;

private:
    void parseData(const QByteArray &data);

    double m_pressure = 0;
};

#endif // BOOKOOMONITOR_H
