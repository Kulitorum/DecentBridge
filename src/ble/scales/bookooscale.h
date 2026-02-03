#pragma once

#include "../scaledevice.h"
#include "../transport/scalebletransport.h"
#include <QLowEnergyCharacteristic>

class BookooScale : public ScaleDevice {
    Q_OBJECT

public:
    explicit BookooScale(ScaleBleTransport* transport, QObject* parent = nullptr);
    ~BookooScale() override;

    void connectToDevice(const QBluetoothDeviceInfo& device) override;
    QString name() const override { return m_name; }
    QString type() const override { return "bookoo"; }

public slots:
    void tare() override;
    void startTimer() override;
    void stopTimer() override;
    void resetTimer() override;

private slots:
    void onTransportConnected();
    void onTransportDisconnected();
    void onTransportError(const QString& message);
    void onServiceDiscovered(const QBluetoothUuid& uuid);
    void onServicesDiscoveryFinished();
    void onCharacteristicsDiscoveryFinished(const QBluetoothUuid& serviceUuid);
    void onCharacteristicChanged(const QBluetoothUuid& characteristicUuid, const QByteArray& value);
    void onNotificationsEnabled(const QBluetoothUuid& characteristicUuid);

private:
    void sendCommand(const QByteArray& cmd);
    void parseWeightData(const QByteArray& data);

    ScaleBleTransport* m_transport = nullptr;
    QString m_name = "Bookoo";
    bool m_serviceFound = false;
    bool m_characteristicsReady = false;
};
