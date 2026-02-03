#ifndef DE1DEVICE_H
#define DE1DEVICE_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QJsonObject>

#include "protocol/de1characteristics.h"

/**
 * @brief DE1 espresso machine BLE communication
 *
 * Handles connection to the DE1 via Bluetooth LE and provides
 * methods to read state, send commands, and receive real-time data.
 */
class DE1Device : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ isConnecting NOTIFY connectingChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)

public:
    explicit DE1Device(QObject *parent = nullptr);
    ~DE1Device();

    // Connection
    void connectToDevice(const QBluetoothDeviceInfo &device);
    void disconnect();
    bool isConnected() const { return m_connected; }
    bool isConnecting() const { return m_connecting; }

    // Device info
    QString name() const { return m_name; }
    QString address() const { return m_address; }
    QString firmwareVersion() const { return m_firmwareVersion; }
    QString modelName() const;
    QString serialNumber() const { return m_serialNumber; }
    bool hasGHC() const { return m_hasGHC; }

    // State
    DE1::State state() const { return m_state; }
    DE1::SubState subState() const { return m_subState; }
    QString stateString() const { return DE1::stateToString(m_state); }
    QString subStateString() const { return DE1::subStateToString(m_subState); }

    // Real-time data
    double pressure() const { return m_pressure; }
    double flow() const { return m_flow; }
    double mixTemp() const { return m_mixTemp; }
    double headTemp() const { return m_headTemp; }
    double steamTemp() const { return m_steamTemp; }
    double targetPressure() const { return m_targetPressure; }
    double targetFlow() const { return m_targetFlow; }
    int waterLevel() const { return m_waterLevel; }

    // Commands
    bool requestState(const QString &stateName);
    bool requestState(DE1::State state);

    // Settings
    bool usbChargerEnabled() const { return m_usbCharger; }
    void setUsbCharger(bool enable);
    int fanThreshold() const { return m_fanThreshold; }
    void setFanThreshold(int temp);

    // Shot settings
    int steamSetting() const { return m_steamSetting; }
    int targetSteamTemp() const { return m_targetSteamTemp; }
    int targetSteamDuration() const { return m_targetSteamDuration; }
    int targetHotWaterTemp() const { return m_targetHotWaterTemp; }
    int targetHotWaterVolume() const { return m_targetHotWaterVolume; }
    int targetHotWaterDuration() const { return m_targetHotWaterDuration; }
    int targetShotVolume() const { return m_targetShotVolume; }
    double targetGroupTemp() const { return m_targetGroupTemp; }

    void setShotSettings(int steamSetting, int steamTemp, int steamDuration,
                        int hotWaterTemp, int hotWaterVolume, int hotWaterDuration,
                        int shotVolume, double groupTemp);
    QJsonObject shotSettingsToJson() const;

    // Profile upload
    bool uploadProfile(const QJsonObject &profile);

    // JSON snapshot for API
    QJsonObject toSnapshot() const;
    QJsonObject toMachineInfo() const;

signals:
    void connectedChanged(bool connected);
    void connectingChanged(bool connecting);
    void nameChanged();
    void stateChanged(const QJsonObject &state);
    void shotSampleReceived(const QJsonObject &sample);
    void waterLevelsChanged(const QJsonObject &levels);
    void error(const QString &message);

private slots:
    void onControllerConnected();
    void onControllerDisconnected();
    void onControllerError(QLowEnergyController::Error error);
    void onServiceDiscovered(const QBluetoothUuid &uuid);
    void onServiceDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onCharacteristicRead(const QLowEnergyCharacteristic &c, const QByteArray &value);

private:
    void setupService();
    void subscribeToCharacteristics();
    void parseStateInfo(const QByteArray &data);
    void parseShotSample(const QByteArray &data);
    void parseWaterLevels(const QByteArray &data);
    void parseVersions(const QByteArray &data);
    void parseShotSettings(const QByteArray &data);
    void readMMR(uint32_t address);
    void writeMMR(uint32_t address, const QByteArray &data);
    void writeCharacteristic(const QBluetoothUuid &uuid, const QByteArray &data);

    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_service = nullptr;

    bool m_connected = false;
    bool m_connecting = false;
    QString m_name;
    QString m_address;

    // Machine info
    QString m_firmwareVersion;
    QString m_serialNumber;
    DE1::MachineModel m_model = DE1::MachineModel::DE1;
    bool m_hasGHC = false;

    // State
    DE1::State m_state = DE1::State::Sleep;
    DE1::SubState m_subState = DE1::SubState::Ready;

    // Real-time data
    double m_pressure = 0;
    double m_flow = 0;
    double m_mixTemp = 0;
    double m_headTemp = 0;
    double m_steamTemp = 0;
    double m_targetPressure = 0;
    double m_targetFlow = 0;
    int m_waterLevel = 0;

    // Settings
    bool m_usbCharger = false;
    int m_fanThreshold = 50;

    // Shot settings
    int m_steamSetting = 1;
    int m_targetSteamTemp = 160;
    int m_targetSteamDuration = 120;
    int m_targetHotWaterTemp = 85;
    int m_targetHotWaterVolume = 200;
    int m_targetHotWaterDuration = 60;
    int m_targetShotVolume = 0;
    double m_targetGroupTemp = 93.0;
};

#endif // DE1DEVICE_H
