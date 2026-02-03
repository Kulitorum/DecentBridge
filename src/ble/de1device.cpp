#include "de1device.h"
#include "protocol/binarycodec.h"

#include <QLoggingCategory>
#include <QJsonObject>
#include <QDateTime>

Q_LOGGING_CATEGORY(lcDE1, "bridge.de1")

DE1Device::DE1Device(QObject *parent)
    : QObject(parent)
{
}

DE1Device::~DE1Device()
{
    disconnect();
}

void DE1Device::connectToDevice(const QBluetoothDeviceInfo &device)
{
    if (m_controller) {
        disconnect();
    }

    m_name = device.name();
    m_address = device.address().toString();
    m_connecting = true;
    emit connectingChanged(true);
    emit nameChanged();

    qCInfo(lcDE1) << "Connecting to" << m_name << "at" << m_address;

    m_controller = QLowEnergyController::createCentral(device, this);

    connect(m_controller, &QLowEnergyController::connected,
            this, &DE1Device::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected,
            this, &DE1Device::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::errorOccurred,
            this, &DE1Device::onControllerError);
    connect(m_controller, &QLowEnergyController::serviceDiscovered,
            this, &DE1Device::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &DE1Device::onServiceDiscoveryFinished);

    m_controller->connectToDevice();
}

void DE1Device::disconnect()
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
        emit connectedChanged(false);
    }

    if (m_connecting) {
        m_connecting = false;
        emit connectingChanged(false);
    }
}

void DE1Device::onControllerConnected()
{
    qCInfo(lcDE1) << "Connected, discovering services...";
    m_controller->discoverServices();
}

void DE1Device::onControllerDisconnected()
{
    qCInfo(lcDE1) << "Disconnected";
    m_connected = false;
    m_connecting = false;
    emit connectedChanged(false);
    emit connectingChanged(false);
}

void DE1Device::onControllerError(QLowEnergyController::Error error)
{
    qCWarning(lcDE1) << "Controller error:" << error;
    emit this->error(QString("BLE error: %1").arg(static_cast<int>(error)));
}

void DE1Device::onServiceDiscovered(const QBluetoothUuid &uuid)
{
    qCDebug(lcDE1) << "Service discovered:" << uuid.toString();

    if (uuid == DE1::SERVICE_UUID) {
        qCInfo(lcDE1) << "Found DE1 service";
    }
}

void DE1Device::onServiceDiscoveryFinished()
{
    qCInfo(lcDE1) << "Service discovery finished";

    m_service = m_controller->createServiceObject(DE1::SERVICE_UUID, this);
    if (!m_service) {
        qCWarning(lcDE1) << "DE1 service not found";
        emit error("DE1 service not found");
        disconnect();
        return;
    }

    connect(m_service, &QLowEnergyService::stateChanged,
            this, &DE1Device::onServiceStateChanged);
    connect(m_service, &QLowEnergyService::characteristicChanged,
            this, &DE1Device::onCharacteristicChanged);
    connect(m_service, &QLowEnergyService::characteristicRead,
            this, &DE1Device::onCharacteristicRead);

    m_service->discoverDetails();
}

void DE1Device::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    if (state == QLowEnergyService::RemoteServiceDiscovered) {
        qCInfo(lcDE1) << "Service details discovered";
        setupService();
    }
}

void DE1Device::setupService()
{
    m_connecting = false;
    m_connected = true;
    emit connectingChanged(false);
    emit connectedChanged(true);

    subscribeToCharacteristics();

    // Read initial state
    auto stateChar = m_service->characteristic(DE1::Characteristic::STATE_INFO);
    if (stateChar.isValid()) {
        m_service->readCharacteristic(stateChar);
    }

    auto versionChar = m_service->characteristic(DE1::Characteristic::VERSION);
    if (versionChar.isValid()) {
        m_service->readCharacteristic(versionChar);
    }

    auto waterChar = m_service->characteristic(DE1::Characteristic::WATER_LEVELS);
    if (waterChar.isValid()) {
        m_service->readCharacteristic(waterChar);
    }

    auto shotSettingsChar = m_service->characteristic(DE1::Characteristic::SHOT_SETTINGS);
    if (shotSettingsChar.isValid()) {
        m_service->readCharacteristic(shotSettingsChar);
    }
}

void DE1Device::subscribeToCharacteristics()
{
    auto enableNotify = [this](const QBluetoothUuid &uuid) {
        auto characteristic = m_service->characteristic(uuid);
        if (characteristic.isValid()) {
            auto descriptor = characteristic.descriptor(
                QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
            if (descriptor.isValid()) {
                m_service->writeDescriptor(descriptor,
                    QLowEnergyCharacteristic::CCCDEnableNotification);
            }
        }
    };

    enableNotify(DE1::Characteristic::STATE_INFO);
    enableNotify(DE1::Characteristic::SHOT_SAMPLE);
    enableNotify(DE1::Characteristic::WATER_LEVELS);
    enableNotify(DE1::Characteristic::TEMPERATURES);
}

void DE1Device::onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (c.uuid() == DE1::Characteristic::STATE_INFO) {
        parseStateInfo(value);
    } else if (c.uuid() == DE1::Characteristic::SHOT_SAMPLE) {
        parseShotSample(value);
    } else if (c.uuid() == DE1::Characteristic::WATER_LEVELS) {
        parseWaterLevels(value);
    } else if (c.uuid() == DE1::Characteristic::SHOT_SETTINGS) {
        parseShotSettings(value);
    }
}

void DE1Device::onCharacteristicRead(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    onCharacteristicChanged(c, value);

    if (c.uuid() == DE1::Characteristic::VERSION) {
        parseVersions(value);
    }
}

void DE1Device::parseStateInfo(const QByteArray &data)
{
    if (data.size() < 2) return;

    auto oldState = m_state;
    m_state = static_cast<DE1::State>(static_cast<uint8_t>(data[0]));
    m_subState = static_cast<DE1::SubState>(static_cast<uint8_t>(data[1]));

    if (m_state != oldState) {
        qCInfo(lcDE1) << "State:" << stateString() << "/" << subStateString();
    }

    QJsonObject state;
    state["state"] = stateString();
    state["substate"] = subStateString();
    emit stateChanged(state);
}

void DE1Device::parseShotSample(const QByteArray &data)
{
    if (data.size() < 15) return;

    // Parse shot sample (see DE1 protocol docs)
    // Bytes 0-1: Timer (uint16 BE, 0.01s units)
    // Byte 2: GroupPressure (U8P4)
    // Byte 3: GroupFlow (U8P4)
    // Byte 4: MixTemp (U8P1)
    // Byte 5: HeadTemp (U8P4, offset +73)
    // Byte 6: SetMixTemp (U8P1)
    // Byte 7: SetHeadTemp (U8P4, offset +73)
    // Byte 8: SetGroupPressure (U8P4)
    // Byte 9: SetGroupFlow (U8P4)
    // Byte 10: FrameNumber
    // Byte 11: SteamTemp (U8P0)

    m_pressure = BinaryCodec::decodeU8P4(static_cast<uint8_t>(data[2]));
    m_flow = BinaryCodec::decodeU8P4(static_cast<uint8_t>(data[3]));
    m_mixTemp = BinaryCodec::decodeU8P1(static_cast<uint8_t>(data[4]));
    m_headTemp = BinaryCodec::decodeU8P4(static_cast<uint8_t>(data[5])) + 73.0;
    m_targetPressure = BinaryCodec::decodeU8P4(static_cast<uint8_t>(data[8]));
    m_targetFlow = BinaryCodec::decodeU8P4(static_cast<uint8_t>(data[9]));
    m_steamTemp = static_cast<double>(static_cast<uint8_t>(data[11]));

    QJsonObject sample;
    sample["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    sample["pressure"] = m_pressure;
    sample["flow"] = m_flow;
    sample["mixTemperature"] = m_mixTemp;
    sample["groupTemperature"] = m_headTemp;
    sample["targetPressure"] = m_targetPressure;
    sample["targetFlow"] = m_targetFlow;
    sample["steamTemperature"] = m_steamTemp;
    sample["profileFrame"] = static_cast<int>(static_cast<uint8_t>(data[10]));

    QJsonObject stateObj;
    stateObj["state"] = stateString();
    stateObj["substate"] = subStateString();
    sample["state"] = stateObj;

    emit shotSampleReceived(sample);
}

void DE1Device::parseWaterLevels(const QByteArray &data)
{
    if (data.size() < 4) return;

    // Bytes 0-1: Current level (uint16 BE, mm)
    // Bytes 2-3: Start level (uint16 BE, mm)
    m_waterLevel = BinaryCodec::decodeShortBE(data, 0);

    QJsonObject levels;
    levels["currentLevel"] = m_waterLevel;
    levels["startLevel"] = static_cast<int>(BinaryCodec::decodeShortBE(data, 2));

    emit waterLevelsChanged(levels);
}

void DE1Device::parseVersions(const QByteArray &data)
{
    if (data.size() < 7) return;

    // Bytes 0: BLE API version
    // Bytes 1-2: Firmware version (uint16 BE)
    // Bytes 3-6: Firmware build (uint32)
    int fwMajor = static_cast<uint8_t>(data[1]);
    int fwMinor = static_cast<uint8_t>(data[2]);
    m_firmwareVersion = QString("%1.%2").arg(fwMajor).arg(fwMinor);

    qCInfo(lcDE1) << "Firmware version:" << m_firmwareVersion;
}

void DE1Device::parseShotSettings(const QByteArray &data)
{
    if (data.size() < 9) return;

    // ShotSettings characteristic format (9 bytes):
    // Byte 0: Steam setting (0-3)
    // Byte 1: Target steam temp (C)
    // Byte 2: Target steam duration (s)
    // Byte 3: Target hot water temp (C)
    // Byte 4: Target hot water volume (ml)
    // Byte 5: Target hot water duration (s)
    // Byte 6: Target shot volume (ml)
    // Bytes 7-8: Group temp (U16P8 big-endian)

    m_steamSetting = static_cast<uint8_t>(data[0]);
    m_targetSteamTemp = static_cast<uint8_t>(data[1]);
    m_targetSteamDuration = static_cast<uint8_t>(data[2]);
    m_targetHotWaterTemp = static_cast<uint8_t>(data[3]);
    m_targetHotWaterVolume = static_cast<uint8_t>(data[4]);
    m_targetHotWaterDuration = static_cast<uint8_t>(data[5]);
    m_targetShotVolume = static_cast<uint8_t>(data[6]);
    m_targetGroupTemp = BinaryCodec::decodeU16P8(BinaryCodec::decodeShortBE(data, 7));

    qCInfo(lcDE1) << "Shot settings: steam" << m_targetSteamTemp << "C, hotWater"
                  << m_targetHotWaterTemp << "C, group" << m_targetGroupTemp << "C";
}

bool DE1Device::requestState(const QString &stateName)
{
    // Map string to state enum
    static const QMap<QString, DE1::State> stateMap = {
        {"sleep", DE1::State::Sleep},
        {"idle", DE1::State::Idle},
        {"espresso", DE1::State::Espresso},
        {"steam", DE1::State::Steam},
        {"hotWater", DE1::State::HotWater},
        {"flush", DE1::State::HotWaterRinse},
        {"descale", DE1::State::Descale},
        {"clean", DE1::State::Clean},
    };

    auto it = stateMap.find(stateName.toLower());
    if (it == stateMap.end()) {
        return false;
    }

    return requestState(it.value());
}

bool DE1Device::requestState(DE1::State state)
{
    if (!m_connected || !m_service) {
        return false;
    }

    QByteArray data(1, static_cast<char>(state));
    writeCharacteristic(DE1::Characteristic::REQUESTED_STATE, data);

    qCInfo(lcDE1) << "Requesting state:" << DE1::stateToString(state);
    return true;
}

void DE1Device::setUsbCharger(bool enable)
{
    if (!m_connected) return;

    QByteArray data(4, 0);
    data[0] = enable ? 1 : 0;
    writeMMR(DE1::MMR::USB_CHARGER, data);
    m_usbCharger = enable;
}

void DE1Device::setFanThreshold(int temp)
{
    if (!m_connected) return;

    QByteArray data(4, 0);
    data[0] = static_cast<char>(temp);
    writeMMR(DE1::MMR::FAN_THRESHOLD, data);
    m_fanThreshold = temp;
}

void DE1Device::setShotSettings(int steamSetting, int steamTemp, int steamDuration,
                                int hotWaterTemp, int hotWaterVolume, int hotWaterDuration,
                                int shotVolume, double groupTemp)
{
    if (!m_connected || !m_service) return;

    QByteArray data(9, 0);
    data[0] = static_cast<char>(steamSetting);
    data[1] = static_cast<char>(steamTemp);
    data[2] = static_cast<char>(steamDuration);
    data[3] = static_cast<char>(hotWaterTemp);
    data[4] = static_cast<char>(hotWaterVolume);
    data[5] = static_cast<char>(hotWaterDuration);
    data[6] = static_cast<char>(shotVolume);

    uint16_t groupTempEncoded = BinaryCodec::encodeU16P8(groupTemp);
    QByteArray groupTempBE = BinaryCodec::encodeShortBE(groupTempEncoded);
    data[7] = groupTempBE[0];
    data[8] = groupTempBE[1];

    writeCharacteristic(DE1::Characteristic::SHOT_SETTINGS, data);

    // Update local values
    m_steamSetting = steamSetting;
    m_targetSteamTemp = steamTemp;
    m_targetSteamDuration = steamDuration;
    m_targetHotWaterTemp = hotWaterTemp;
    m_targetHotWaterVolume = hotWaterVolume;
    m_targetHotWaterDuration = hotWaterDuration;
    m_targetShotVolume = shotVolume;
    m_targetGroupTemp = groupTemp;

    qCInfo(lcDE1) << "Shot settings updated";
}

QJsonObject DE1Device::shotSettingsToJson() const
{
    QJsonObject obj;
    obj["steamSetting"] = m_steamSetting;
    obj["targetSteamTemp"] = m_targetSteamTemp;
    obj["targetSteamDuration"] = m_targetSteamDuration;
    obj["targetHotWaterTemp"] = m_targetHotWaterTemp;
    obj["targetHotWaterVolume"] = m_targetHotWaterVolume;
    obj["targetHotWaterDuration"] = m_targetHotWaterDuration;
    obj["targetShotVolume"] = m_targetShotVolume;
    obj["groupTemp"] = m_targetGroupTemp;
    return obj;
}

bool DE1Device::uploadProfile(const QJsonObject &profile)
{
    if (!m_connected || !m_service) return false;

    // Profile upload requires:
    // 1. Write header to HEADER_WRITE (0xA00F)
    // 2. Write each frame to FRAME_WRITE (0xA010)

    // Extract profile data
    int numberOfFrames = 0;
    QJsonArray steps = profile["steps"].toArray();
    if (steps.isEmpty()) {
        qCWarning(lcDE1) << "Profile has no steps";
        return false;
    }

    // Build header (20 bytes)
    QByteArray header(20, 0);
    header[0] = 1; // Header version
    header[1] = static_cast<char>(steps.size()); // Number of frames

    // Target volumes (U10P0 format)
    double targetVolume = profile["target_volume"].toDouble(0);
    double targetWeight = profile["target_weight"].toDouble(0);
    uint16_t volEncoded = BinaryCodec::encodeU10P0(targetVolume);
    header[2] = static_cast<char>(volEncoded >> 8);
    header[3] = static_cast<char>(volEncoded & 0xFF);

    // Write header
    writeCharacteristic(DE1::Characteristic::HEADER_WRITE, header);
    qCInfo(lcDE1) << "Profile header written, frames:" << steps.size();

    // Write each frame (8 bytes each)
    for (int i = 0; i < steps.size(); ++i) {
        QJsonObject step = steps[i].toObject();

        QByteArray frame(8, 0);
        frame[0] = static_cast<char>(i); // Frame number

        // Build flags
        uint8_t flags = 0;
        QString pump = step["pump"].toString("pressure");
        if (pump == "flow") flags |= DE1::FrameFlag::CtrlF;
        if (step["transition"].toString() == "smooth") flags |= DE1::FrameFlag::Interpolate;
        frame[1] = static_cast<char>(flags);

        // Target pressure/flow (U8P4)
        frame[2] = BinaryCodec::encodeU8P4(step["pressure"].toDouble(0));
        frame[3] = BinaryCodec::encodeU8P4(step["flow"].toDouble(0));

        // Temperature (U8P1)
        frame[4] = BinaryCodec::encodeU8P1(step["temperature"].toDouble(93));

        // Duration (F8_1_7)
        frame[5] = BinaryCodec::encodeF8_1_7(step["seconds"].toDouble(0));

        // Exit trigger
        frame[6] = 0; // Exit type
        frame[7] = 0; // Exit value

        writeCharacteristic(DE1::Characteristic::FRAME_WRITE, frame);
    }

    qCInfo(lcDE1) << "Profile uploaded:" << profile["title"].toString();
    return true;
}

void DE1Device::writeCharacteristic(const QBluetoothUuid &uuid, const QByteArray &data)
{
    if (!m_service) return;

    auto characteristic = m_service->characteristic(uuid);
    if (characteristic.isValid()) {
        m_service->writeCharacteristic(characteristic, data);
    }
}

void DE1Device::readMMR(uint32_t address)
{
    QByteArray data = BinaryCodec::encodeU24P0(address);
    data.prepend(static_cast<char>(data.size())); // Length byte
    writeCharacteristic(DE1::Characteristic::READ_FROM_MMR, data);
}

void DE1Device::writeMMR(uint32_t address, const QByteArray &payload)
{
    QByteArray data = BinaryCodec::encodeU24P0(address);
    data.append(payload);
    data.prepend(static_cast<char>(data.size())); // Length byte
    writeCharacteristic(DE1::Characteristic::WRITE_TO_MMR, data);
}

QString DE1Device::modelName() const
{
    switch (m_model) {
        case DE1::MachineModel::DE1: return "DE1";
        case DE1::MachineModel::DE1Plus: return "DE1+";
        case DE1::MachineModel::DE1Pro: return "DE1Pro";
        case DE1::MachineModel::DE1XL: return "DE1XL";
        case DE1::MachineModel::DE1Cafe: return "DE1Cafe";
        default: return "Unknown";
    }
}

QJsonObject DE1Device::toSnapshot() const
{
    QJsonObject obj;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonObject stateObj;
    stateObj["state"] = stateString();
    stateObj["substate"] = subStateString();
    obj["state"] = stateObj;

    obj["pressure"] = m_pressure;
    obj["flow"] = m_flow;
    obj["mixTemperature"] = m_mixTemp;
    obj["groupTemperature"] = m_headTemp;
    obj["targetPressure"] = m_targetPressure;
    obj["targetFlow"] = m_targetFlow;
    obj["steamTemperature"] = m_steamTemp;

    return obj;
}

QJsonObject DE1Device::toMachineInfo() const
{
    QJsonObject obj;
    obj["version"] = m_firmwareVersion;
    obj["model"] = modelName();
    obj["serialNumber"] = m_serialNumber;
    obj["GHC"] = m_hasGHC;
    return obj;
}
