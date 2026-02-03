#ifndef SENSORDEVICE_H
#define SENSORDEVICE_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Base class for BLE sensor devices
 *
 * Sensors are external devices that provide additional data like
 * pressure, temperature, or other measurements. Examples include
 * the Bookoo Espresso Monitor.
 */
class SensorDevice : public QObject
{
    Q_OBJECT

public:
    struct DataChannel {
        QString key;
        QString type;  // "number", "boolean", "string"
        QString unit;  // "bar", "celsius", etc.
        double value = 0;
    };

    explicit SensorDevice(QObject *parent = nullptr);
    virtual ~SensorDevice();

    // Connection
    virtual void connectToDevice(const QBluetoothDeviceInfo &device);
    virtual void disconnect();
    bool isConnected() const { return m_connected; }

    // Device info
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString address() const { return m_address; }
    virtual QString sensorType() const { return "generic"; }

    // Data channels
    QList<DataChannel> dataChannels() const { return m_channels; }
    double value(const QString &key) const;

    // JSON representation
    QJsonObject toJson() const;
    QJsonObject toSnapshot() const;

signals:
    void connected();
    void disconnected();
    void dataUpdated(const QJsonObject &data);
    void errorOccurred(const QString &error);

protected slots:
    virtual void onControllerConnected();
    virtual void onControllerDisconnected();
    virtual void onControllerError(QLowEnergyController::Error error);
    virtual void onServiceDiscovered(const QBluetoothUuid &uuid);
    virtual void onServiceDiscoveryFinished();
    virtual void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);

protected:
    virtual void setupService() = 0;
    virtual QBluetoothUuid serviceUuid() const = 0;
    void updateChannel(const QString &key, double value);

    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_service = nullptr;
    bool m_connected = false;
    QString m_id;
    QString m_name;
    QString m_address;
    QList<DataChannel> m_channels;
};

#endif // SENSORDEVICE_H
