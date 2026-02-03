/**
 * DecentBridge - Headless BLE-to-HTTP Bridge for DE1 Espresso Machines
 *
 * A lightweight server that connects to DE1 machines and scales via Bluetooth LE
 * and exposes control via REST API and WebSocket for real-time data.
 *
 * Based on code from:
 * - Decenza (C++/Qt) - BLE protocol, scale support
 * - ReaPrime (Dart/Flutter) - REST API design
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>

#include "core/bridge.h"
#include "core/settings.h"

Q_LOGGING_CATEGORY(lcMain, "bridge.main")

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("DecentBridge");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("DecentBridge");

    // Command line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription("Headless BLE-to-HTTP bridge for DE1 espresso machines");
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

    return app.exec();
}
