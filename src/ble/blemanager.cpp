#include "blemanager.h"
#include "protocol/de1characteristics.h"

#include <QLoggingCategory>
#include <QCoreApplication>
#include <QPermissions>

Q_LOGGING_CATEGORY(lcBLE, "bridge.ble")

BLEManager::BLEManager(QObject *parent)
    : QObject(parent)
{
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(30000); // 30 seconds

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BLEManager::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BLEManager::onScanFinished);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &BLEManager::onScanError);
}

BLEManager::~BLEManager()
{
    stopScan();
}

bool BLEManager::isScanning() const
{
    return m_agent && m_agent->isActive();
}

void BLEManager::startScan()
{
    if (isScanning()) {
        return;
    }

    m_devices.clear();
    qCInfo(lcBLE) << "Starting BLE scan...";

#ifdef Q_OS_ANDROID
    // Request location permission first (required for BLE scanning on Android)
    QLocationPermission locationPermission;
    locationPermission.setAccuracy(QLocationPermission::Precise);

    if (qApp->checkPermission(locationPermission) == Qt::PermissionStatus::Undetermined) {
        qCInfo(lcBLE) << "Requesting location permission...";
        qApp->requestPermission(locationPermission, this, [this](const QPermission& permission) {
            if (permission.status() == Qt::PermissionStatus::Granted) {
                qCInfo(lcBLE) << "Location permission granted";
                requestBluetoothPermission();
            } else {
                qCWarning(lcBLE) << "Location permission denied";
                emit error("Location permission denied - required for Bluetooth scanning");
            }
        });
        return;
    } else if (qApp->checkPermission(locationPermission) == Qt::PermissionStatus::Denied) {
        qCWarning(lcBLE) << "Location permission denied";
        emit error("Location permission required. Please enable in Settings.");
        return;
    }

    // Location already granted, check Bluetooth
    requestBluetoothPermission();
#else
    // Non-Android: check Bluetooth permission directly
    requestBluetoothPermission();
#endif
}

void BLEManager::requestBluetoothPermission()
{
    QBluetoothPermission bluetoothPermission;
    bluetoothPermission.setCommunicationModes(QBluetoothPermission::Access);

    switch (qApp->checkPermission(bluetoothPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qCInfo(lcBLE) << "Requesting Bluetooth permission...";
        qApp->requestPermission(bluetoothPermission, this, [this](const QPermission& permission) {
            if (permission.status() == Qt::PermissionStatus::Granted) {
                qCInfo(lcBLE) << "Bluetooth permission granted";
                doStartScan();
            } else {
                qCWarning(lcBLE) << "Bluetooth permission denied";
                emit error("Bluetooth permission denied");
            }
        });
        break;
    case Qt::PermissionStatus::Denied:
        qCWarning(lcBLE) << "Bluetooth permission denied";
        emit error("Bluetooth permission required. Please enable in Settings.");
        break;
    case Qt::PermissionStatus::Granted:
        doStartScan();
        break;
    }
}

void BLEManager::doStartScan()
{
    qCInfo(lcBLE) << "Permissions granted, starting scan...";
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    emit scanningChanged(true);
}

void BLEManager::stopScan()
{
    if (m_agent && m_agent->isActive()) {
        m_agent->stop();
        emit scanningChanged(false);
    }
}

void BLEManager::onDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    // Only care about BLE devices
    if (!(device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        return;
    }

    // Check for duplicate (same address already in list)
    QString address = device.address().toString();
    for (const auto &existing : m_devices) {
        if (existing.address().toString() == address) {
            return; // Already have this device
        }
    }

    m_devices.append(device);

    if (isDE1(device)) {
        qCInfo(lcBLE) << "Found DE1:" << device.name() << device.address().toString();
        emit de1Discovered(device);
    } else if (isScale(device)) {
        QString type = scaleType(device);
        qCInfo(lcBLE) << "Found" << type << "scale:" << device.name() << device.address().toString();
        emit scaleDiscovered(device);
    }
}

void BLEManager::onScanFinished()
{
    qCInfo(lcBLE) << "Scan finished, found" << m_devices.size() << "devices";
    emit scanningChanged(false);
    emit scanFinished();
}

void BLEManager::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    qCWarning(lcBLE) << "Scan error:" << error << m_agent->errorString();
    emit this->error(m_agent->errorString());
}

bool BLEManager::isDE1(const QBluetoothDeviceInfo &device) const
{
    // Check if it's a known scale first - scales are NOT DE1 machines
    if (isScale(device)) {
        return false;
    }

    QString name = device.name().toLower();

    // DE1 devices advertise as "DE1" or contain "decent"
    if (name.startsWith("de1") || name.contains("decent")) {
        return true;
    }

    // Check for DE1 service UUID in advertisement
    for (const auto &uuid : device.serviceUuids()) {
        if (uuid == DE1::SERVICE_UUID) {
            return true;
        }
    }

    return false;
}

bool BLEManager::isScale(const QBluetoothDeviceInfo &device) const
{
    return !scaleType(device).isEmpty();
}

QString BLEManager::scaleType(const QBluetoothDeviceInfo &device) const
{
    QString name = device.name();
    QString nameLower = name.toLower();

    // Detect scale by name pattern (case-insensitive where needed)
    if (name.startsWith("Decent Scale")) return "Decent";
    if (nameLower.startsWith("acaia") || nameLower.startsWith("proch")) return "Acaia";
    if (nameLower.startsWith("pyxis")) return "Acaia Pyxis";
    if (nameLower.startsWith("felicita")) return "Felicita";
    if (nameLower.startsWith("skale")) return "Skale";
    if (nameLower.startsWith("bookoo")) return "Bookoo";
    if (nameLower.startsWith("eureka")) return "Eureka";
    if (nameLower.startsWith("difluid")) return "DiFluid";
    if (nameLower.startsWith("hiroia") || nameLower.startsWith("jimmy")) return "Hiroia";
    if (nameLower.startsWith("varia")) return "Varia";
    if (nameLower.startsWith("smartchef")) return "SmartChef";

    return QString();
}
