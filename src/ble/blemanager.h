#ifndef BLEMANAGER_H
#define BLEMANAGER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QList>

/**
 * @brief BLE device discovery manager
 *
 * Scans for DE1 machines and compatible scales.
 */
class BLEManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)

public:
    explicit BLEManager(QObject *parent = nullptr);
    ~BLEManager();

    bool isScanning() const;

    void startScan();
    void stopScan();

    QList<QBluetoothDeviceInfo> discoveredDevices() const { return m_devices; }

    // Device type detection
    bool isDE1(const QBluetoothDeviceInfo &device) const;
    bool isScale(const QBluetoothDeviceInfo &device) const;
    bool isSensor(const QBluetoothDeviceInfo &device) const;
    QString scaleType(const QBluetoothDeviceInfo &device) const;
    QString sensorType(const QBluetoothDeviceInfo &device) const;

signals:
    void scanningChanged(bool scanning);
    void de1Discovered(const QBluetoothDeviceInfo &device);
    void scaleDiscovered(const QBluetoothDeviceInfo &device);
    void sensorDiscovered(const QBluetoothDeviceInfo &device);
    void scanFinished();
    void error(const QString &message);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);

private:
    void requestBluetoothPermission();
    void doStartScan();

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QList<QBluetoothDeviceInfo> m_devices;
};

#endif // BLEMANAGER_H
