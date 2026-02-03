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
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QNetworkInterface>

#include "core/bridge.h"
#include "core/settings.h"

Q_LOGGING_CATEGORY(lcMain, "bridge.main")

class BridgeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bridgeName READ bridgeName WRITE setBridgeName NOTIFY bridgeNameChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ipAddressChanged)
    Q_PROPERTY(int httpPort READ httpPort CONSTANT)
    Q_PROPERTY(int wsPort READ wsPort CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool de1Connected READ de1Connected NOTIFY de1ConnectedChanged)
    Q_PROPERTY(bool scaleConnected READ scaleConnected NOTIFY scaleConnectedChanged)
    Q_PROPERTY(QString de1Name READ de1Name NOTIFY de1NameChanged)
    Q_PROPERTY(QString scaleName READ scaleName NOTIFY scaleNameChanged)

public:
    BridgeController(Bridge *bridge, Settings *settings, QObject *parent = nullptr)
        : QObject(parent), m_bridge(bridge), m_settings(settings)
    {
        connect(m_bridge, &Bridge::de1Connected, this, [this]() {
            emit de1ConnectedChanged();
            emit de1NameChanged();
        });
        connect(m_bridge, &Bridge::de1Disconnected, this, [this]() {
            emit de1ConnectedChanged();
            emit de1NameChanged();
        });
        connect(m_bridge, &Bridge::scaleConnected, this, [this]() {
            emit scaleConnectedChanged();
            emit scaleNameChanged();
        });
        connect(m_bridge, &Bridge::scaleDisconnected, this, [this]() {
            emit scaleConnectedChanged();
            emit scaleNameChanged();
        });
        connect(m_settings, &Settings::bridgeNameChanged, this, &BridgeController::bridgeNameChanged);

        // Update IP address periodically
        m_ipAddress = getLocalIpAddress();
    }

    QString bridgeName() const { return m_settings->bridgeName(); }
    void setBridgeName(const QString &name) { m_settings->setBridgeName(name); }

    QString ipAddress() const { return m_ipAddress; }
    int httpPort() const { return m_settings->httpPort(); }
    int wsPort() const { return m_settings->webSocketPort(); }
    QString version() const { return "0.1.0"; }

    bool de1Connected() const { return m_bridge->de1() && m_bridge->de1()->isConnected(); }
    bool scaleConnected() const { return m_bridge->scale() && m_bridge->scale()->isConnected(); }

    QString de1Name() const {
        if (m_bridge->de1() && m_bridge->de1()->isConnected()) {
            return m_bridge->de1()->name();
        }
        return QString();
    }

    QString scaleName() const {
        if (m_bridge->scale() && m_bridge->scale()->isConnected()) {
            return m_bridge->scale()->name();
        }
        return QString();
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
        return "127.0.0.1";
    }

signals:
    void bridgeNameChanged();
    void ipAddressChanged();
    void de1ConnectedChanged();
    void scaleConnectedChanged();
    void de1NameChanged();
    void scaleNameChanged();

private:
    Bridge *m_bridge;
    Settings *m_settings;
    QString m_ipAddress;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("DecentBridge");
    app.setApplicationVersion("0.1.0");
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

    // Create and start bridge
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

    qCInfo(lcMain) << "DecentBridge v" << app.applicationVersion();
    qCInfo(lcMain) << "HTTP server on port" << settings.httpPort();
    qCInfo(lcMain) << "WebSocket server on port" << settings.webSocketPort();
    qCInfo(lcMain) << "Scanning for DE1 and scales...";

    // Create controller for QML
    BridgeController controller(&bridge, &settings);

    // Setup QML engine
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("bridge", &controller);

    engine.loadFromModule("DecentBridge", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCCritical(lcMain) << "Failed to load QML UI";
        return 1;
    }

    return app.exec();
}

#include "main.moc"
