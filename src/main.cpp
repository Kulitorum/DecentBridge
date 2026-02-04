/**
 * DecentBridge - BLE-to-HTTP Bridge for DE1 Espresso Machines
 *
 * A bridge server that connects to DE1 machines and scales via Bluetooth LE
 * and exposes control via REST API and WebSocket for real-time data.
 *
 * Based on code from:
 * - Decenza (C++/Qt) - BLE protocol, scale support
 * - ReaPrime (Dart/Flutter) - REST API design
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QThread>
#include <QUrl>
#include <QBluetoothDeviceInfo>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>

#include "core/bridge.h"
#include "core/settings.h"
#include "ble/de1device.h"
#include "ble/scaledevice.h"
#include "ble/blemanager.h"

Q_LOGGING_CATEGORY(lcMain, "bridge.main")

#ifdef Q_OS_ANDROID
/**
 * Worker thread that runs Bridge and all its services (HTTP, WebSocket, BLE).
 *
 * On Android, the main thread's event loop is tied to the Activity lifecycle.
 * When the Activity goes to background, Android suspends the main event loop,
 * killing HTTP/WebSocket/BLE processing. By running Bridge on its own thread
 * with its own event loop, it stays alive regardless of Activity state.
 */
class BridgeThread : public QThread {
    Q_OBJECT
public:
    BridgeThread(Settings *settings, QObject *parent = nullptr)
        : QThread(parent), m_settings(settings) {}

    Bridge* bridge() const { return m_bridge; }

signals:
    void bridgeReady(Bridge *bridge);

protected:
    void run() override {
        // Create Bridge on this thread. All children (HttpServer, WebSocketServer,
        // BLEManager, DE1Device) are created here and use this thread's event loop.
        Bridge bridge(m_settings);
        m_bridge = &bridge;

        QObject::connect(&bridge, &Bridge::started, []() {
            qCInfo(lcMain) << "Bridge started on worker thread";
        });
        QObject::connect(&bridge, &Bridge::error, [](const QString &err) {
            qCCritical(lcMain) << "Bridge error:" << err;
        });

        if (!bridge.start()) {
            qCCritical(lcMain) << "Failed to start bridge on worker thread";
            m_bridge = nullptr;
            return;
        }

        // Signal main thread that Bridge is ready for QML binding
        emit bridgeReady(&bridge);

        qCInfo(lcMain) << "Worker thread event loop starting";
        exec(); // Run this thread's event loop (independent of main thread)
        qCInfo(lcMain) << "Worker thread event loop exited";
        m_bridge = nullptr;
    }

private:
    Settings *m_settings;
    Bridge *m_bridge = nullptr;
};
#endif

/**
 * QML-facing controller that exposes Bridge state and controls.
 *
 * Caches all values locally for thread safety - on Android, Bridge runs on
 * a worker thread while QML runs on the main thread. Signal/slot connections
 * with Qt::AutoConnection handle cross-thread dispatch automatically.
 */
class BridgeController : public QObject
{
    Q_OBJECT

    // Bridge info
    Q_PROPERTY(QString bridgeName READ bridgeName WRITE setBridgeName NOTIFY bridgeNameChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress CONSTANT)
    Q_PROPERTY(int httpPort READ httpPort CONSTANT)
    Q_PROPERTY(int wsPort READ wsPort CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

    // Device connection status
    Q_PROPERTY(bool de1Connected READ de1Connected NOTIFY de1ConnectedChanged)
    Q_PROPERTY(bool scaleConnected READ scaleConnected NOTIFY scaleConnectedChanged)
    Q_PROPERTY(QString de1Name READ de1Name NOTIFY de1ConnectedChanged)
    Q_PROPERTY(QString scaleName READ scaleName NOTIFY scaleConnectedChanged)

    // Machine real-time data
    Q_PROPERTY(QString machineState READ machineState NOTIFY machineStateChanged)
    Q_PROPERTY(QString machineSubState READ machineSubState NOTIFY machineStateChanged)
    Q_PROPERTY(double groupTemp READ groupTemp NOTIFY metricsChanged)
    Q_PROPERTY(double steamTemp READ steamTemp NOTIFY metricsChanged)
    Q_PROPERTY(double pressure READ pressure NOTIFY metricsChanged)
    Q_PROPERTY(double flow READ flow NOTIFY metricsChanged)

    // Scale real-time data
    Q_PROPERTY(double scaleWeight READ scaleWeight NOTIFY scaleWeightChanged)
    Q_PROPERTY(double scaleFlow READ scaleFlow NOTIFY scaleFlowChanged)

    // BLE scanning
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QVariantList discoveredScales READ discoveredScales NOTIFY discoveredScalesChanged)

public:
    BridgeController(Bridge *bridge, Settings *settings, QObject *parent = nullptr)
        : QObject(parent), m_bridge(bridge), m_settings(settings)
    {
        m_ipAddress = getLocalIpAddress();

        // DE1 connection status
        connect(m_bridge, &Bridge::de1Connected, this, [this]() {
            m_de1Connected = true;
            m_de1Name = m_bridge->de1() ? m_bridge->de1()->name() : QString();
            emit de1ConnectedChanged();
        });
        connect(m_bridge, &Bridge::de1Disconnected, this, [this]() {
            m_de1Connected = false;
            m_de1Name.clear();
            m_machineState = QStringLiteral("Sleep");
            m_machineSubState = QStringLiteral("Ready");
            m_groupTemp = 0; m_steamTemp = 0; m_pressure = 0; m_flow = 0;
            emit de1ConnectedChanged();
            emit machineStateChanged();
            emit metricsChanged();
        });

        // Scale connection - dynamically connect to new scale's signals
        connect(m_bridge, &Bridge::scaleConnected, this, [this]() {
            m_scaleConnected = true;
            m_scaleName = m_bridge->scale() ? m_bridge->scale()->name() : QString();
            connectToScale();
            m_discoveredScales.clear();
            m_discoveredDeviceMap.clear();
            emit scaleConnectedChanged();
            emit discoveredScalesChanged();
        });
        connect(m_bridge, &Bridge::scaleDisconnected, this, [this]() {
            m_scaleConnected = false;
            m_scaleName.clear();
            m_scaleWeight = 0;
            m_scaleFlow = 0;
            emit scaleConnectedChanged();
            emit scaleWeightChanged();
            emit scaleFlowChanged();
        });

        // Settings
        connect(m_settings, &Settings::bridgeNameChanged, this, &BridgeController::bridgeNameChanged);

        // Machine real-time data from DE1
        if (m_bridge->de1()) {
            connect(m_bridge->de1(), &DE1Device::shotSampleReceived,
                    this, [this](const QJsonObject &sample) {
                m_groupTemp = sample[QStringLiteral("groupTemperature")].toDouble();
                m_steamTemp = sample[QStringLiteral("steamTemperature")].toDouble();
                m_pressure = sample[QStringLiteral("pressure")].toDouble();
                m_flow = sample[QStringLiteral("flow")].toDouble();
                emit metricsChanged();

                // Shot samples also carry state info
                QJsonObject stateObj = sample[QStringLiteral("state")].toObject();
                QString newState = stateObj[QStringLiteral("state")].toString();
                QString newSubState = stateObj[QStringLiteral("substate")].toString();
                if (m_machineState != newState || m_machineSubState != newSubState) {
                    m_machineState = newState;
                    m_machineSubState = newSubState;
                    emit machineStateChanged();
                }
            });
            connect(m_bridge->de1(), &DE1Device::stateChanged,
                    this, [this](const QJsonObject &state) {
                m_machineState = state[QStringLiteral("state")].toString();
                m_machineSubState = state[QStringLiteral("substate")].toString();
                emit machineStateChanged();
            });
        }

        // BLE scanning
        if (m_bridge->bleManager()) {
            connect(m_bridge->bleManager(), &BLEManager::scanningChanged,
                    this, [this](bool s) {
                m_scanning = s;
                emit scanningChanged();
            });
            connect(m_bridge->bleManager(), &BLEManager::scaleDiscovered,
                    this, [this](const QBluetoothDeviceInfo &device) {
                QString addr = device.address().toString();
                if (m_discoveredDeviceMap.contains(addr)) return;
                m_discoveredDeviceMap[addr] = device;
                QVariantMap entry;
                entry[QStringLiteral("name")] = device.name();
                entry[QStringLiteral("address")] = addr;
                entry[QStringLiteral("scaleType")] = m_bridge->bleManager()->scaleType(device);
                m_discoveredScales.append(entry);
                emit discoveredScalesChanged();
            });
        }

        // If scale is already connected at construction time
        if (m_bridge->scale() && m_bridge->scale()->isConnected()) {
            m_scaleConnected = true;
            m_scaleName = m_bridge->scale()->name();
            connectToScale();
        }
    }

    // Bridge info
    QString bridgeName() const { return m_settings->bridgeName(); }
    void setBridgeName(const QString &name) { m_settings->setBridgeName(name); }
    QString ipAddress() const { return m_ipAddress; }
    int httpPort() const { return m_settings->httpPort(); }
    int wsPort() const { return m_settings->webSocketPort(); }
    QString version() const { return QStringLiteral(APP_VERSION); }

    // Connection status (cached)
    bool de1Connected() const { return m_de1Connected; }
    bool scaleConnected() const { return m_scaleConnected; }
    QString de1Name() const { return m_de1Name; }
    QString scaleName() const { return m_scaleName; }

    // Machine data (cached)
    QString machineState() const { return m_machineState; }
    QString machineSubState() const { return m_machineSubState; }
    double groupTemp() const { return m_groupTemp; }
    double steamTemp() const { return m_steamTemp; }
    double pressure() const { return m_pressure; }
    double flow() const { return m_flow; }

    // Scale data (cached)
    double scaleWeight() const { return m_scaleWeight; }
    double scaleFlow() const { return m_scaleFlow; }

    // Scanning (cached)
    bool scanning() const { return m_scanning; }
    QVariantList discoveredScales() const { return m_discoveredScales; }

    // Machine control
    Q_INVOKABLE void setMachineState(const QString &state) {
        if (!m_bridge->de1()) return;
        QMetaObject::invokeMethod(m_bridge->de1(), [de1 = m_bridge->de1(), state]() {
            de1->requestState(state);
        });
    }

    // Scale control
    Q_INVOKABLE void tareScale() {
        if (!m_bridge->scale()) return;
        QMetaObject::invokeMethod(m_bridge->scale(), [scale = m_bridge->scale()]() {
            scale->tare();
        });
    }

    Q_INVOKABLE void disconnectScale() {
        QMetaObject::invokeMethod(m_bridge, [bridge = m_bridge]() {
            bridge->disconnectScale();
        });
    }

    // BLE scanning
    Q_INVOKABLE void startScan() {
        m_discoveredScales.clear();
        m_discoveredDeviceMap.clear();
        emit discoveredScalesChanged();
        if (!m_bridge->bleManager()) return;
        QMetaObject::invokeMethod(m_bridge->bleManager(), [mgr = m_bridge->bleManager()]() {
            mgr->startScan();
        });
    }

    Q_INVOKABLE void connectToScale(const QString &address) {
        QMetaObject::invokeMethod(m_bridge, [bridge = m_bridge, address]() {
            for (const auto &dev : bridge->bleManager()->discoveredDevices()) {
                if (dev.address().toString() == address) {
                    bridge->connectToScale(dev);
                    return;
                }
            }
        });
    }

    static QString getLocalIpAddress() {
        for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
            if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
                iface.flags().testFlag(QNetworkInterface::IsRunning) &&
                !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
                for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        return entry.ip().toString();
                    }
                }
            }
        }
        return QStringLiteral("127.0.0.1");
    }

signals:
    void bridgeNameChanged();
    void ipAddressChanged();
    void de1ConnectedChanged();
    void scaleConnectedChanged();
    void machineStateChanged();
    void metricsChanged();
    void scaleWeightChanged();
    void scaleFlowChanged();
    void scanningChanged();
    void discoveredScalesChanged();

private:
    void connectToScale() {
        if (auto *scale = m_bridge->scale()) {
            connect(scale, &ScaleDevice::weightChanged, this, [this](double w) {
                m_scaleWeight = w;
                emit scaleWeightChanged();
            });
            connect(scale, &ScaleDevice::flowRateChanged, this, [this](double f) {
                m_scaleFlow = f;
                emit scaleFlowChanged();
            });
        }
    }

    Bridge *m_bridge;
    Settings *m_settings;
    QString m_ipAddress;

    // Cached values (thread-safe: only modified on main thread via queued signals)
    bool m_de1Connected = false;
    bool m_scaleConnected = false;
    QString m_de1Name;
    QString m_scaleName;
    QString m_machineState = QStringLiteral("Sleep");
    QString m_machineSubState = QStringLiteral("Ready");
    double m_groupTemp = 0;
    double m_steamTemp = 0;
    double m_pressure = 0;
    double m_flow = 0;
    double m_scaleWeight = 0;
    double m_scaleFlow = 0;
    bool m_scanning = false;
    QVariantList m_discoveredScales;
    QMap<QString, QBluetoothDeviceInfo> m_discoveredDeviceMap;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("DecentBridge");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("DecentBridge");

    // Command line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription("BLE-to-HTTP bridge for DE1 espresso machines");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "HTTP server port (default: 8080)",
        "port",
        "8080"
    );
    parser.addOption(portOption);

    QCommandLineOption wsPortOption(
        QStringList() << "w" << "ws-port",
        "WebSocket server port (default: 8081)",
        "port",
        "8081"
    );
    parser.addOption(wsPortOption);

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        "Configuration file path",
        "file"
    );
    parser.addOption(configOption);

    QCommandLineOption verboseOption(
        QStringList() << "v" << "verbose",
        "Enable verbose logging"
    );
    parser.addOption(verboseOption);

    parser.process(app);

    // Configure logging
    if (parser.isSet(verboseOption)) {
        QLoggingCategory::setFilterRules("bridge.*=true");
    } else {
        QLoggingCategory::setFilterRules("bridge.*.debug=false");
    }

    // Load settings
    Settings settings;
    if (parser.isSet(configOption)) {
        settings.loadFromFile(parser.value(configOption));
    }

    settings.setHttpPort(parser.value(portOption).toInt());
    settings.setWebSocketPort(parser.value(wsPortOption).toInt());

    qCInfo(lcMain) << "DecentBridge v" << app.applicationVersion();
    qCInfo(lcMain) << "HTTP server on port" << settings.httpPort();
    qCInfo(lcMain) << "WebSocket server on port" << settings.webSocketPort();
    qCInfo(lcMain) << "Scanning for DE1 and scales...";

#ifdef Q_OS_ANDROID
    // On Android, run Bridge on a dedicated worker thread. The main thread's
    // event loop is controlled by the Android Activity lifecycle - when the
    // Activity goes to background, Android suspends the main event loop,
    // killing all processing (HTTP, WebSocket, BLE). The worker thread has
    // its own event loop that Android doesn't touch.
    BridgeThread bridgeThread(&settings);

    // When Bridge is ready on the worker thread, set up QML on the main thread.
    // Qt::QueuedConnection ensures the lambda runs on the main thread.
    QObject::connect(&bridgeThread, &BridgeThread::bridgeReady, &app,
                     [&app, &settings](Bridge *bridge) {
        auto *controller = new BridgeController(bridge, &settings, &app);

        // Android uses the default OpenGL ES renderer (Software renderer fails on EGL)
        auto *engine = new QQmlApplicationEngine(&app);
        engine->rootContext()->setContextProperty("bridge", controller);

        const QUrl url(QStringLiteral("qrc:/Main.qml"));
        QObject::connect(engine, &QQmlApplicationEngine::objectCreated,
                         &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
        engine->load(url);
    }, Qt::QueuedConnection);

    bridgeThread.start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&bridgeThread]() {
        bridgeThread.quit();
        bridgeThread.wait();
    });
#else
    // On desktop, run everything on the main thread (with QML UI)
    Bridge bridge(&settings);

    QObject::connect(&bridge, &Bridge::started, []() {
        qCInfo(lcMain) << "DecentBridge started successfully";
    });
    QObject::connect(&bridge, &Bridge::error, [](const QString &error) {
        qCCritical(lcMain) << "Bridge error:" << error;
    });

    if (!bridge.start()) {
        qCCritical(lcMain) << "Failed to start bridge";
        return 1;
    }

    BridgeController controller(&bridge, &settings);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("bridge", &controller);

    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);
#endif

    return app.exec();
}

#include "main.moc"
