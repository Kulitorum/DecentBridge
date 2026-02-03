#ifndef BRIDGE_H
#define BRIDGE_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <memory>

class Settings;
class BLEManager;
class DE1Device;
class ScaleDevice;
class HttpServer;
class WebSocketServer;

/**
 * @brief Main bridge orchestrator
 *
 * Coordinates BLE devices (DE1 + scales) with HTTP/WebSocket servers.
 * This is the central controller that wires everything together.
 */
class Bridge : public QObject
{
    Q_OBJECT

public:
    explicit Bridge(Settings *settings, QObject *parent = nullptr);
    ~Bridge();

    bool start();
    void stop();

    bool isRunning() const { return m_running; }

    // Device access
    DE1Device *de1() const { return m_de1.get(); }
    ScaleDevice *scale() const { return m_scale; }
    BLEManager *bleManager() const { return m_bleManager.get(); }

    // Scale control
    void disconnectScale();
    void connectToScale(const QBluetoothDeviceInfo &device);

signals:
    void started();
    void stopped();
    void error(const QString &message);

    // Device signals
    void de1Connected();
    void de1Disconnected();
    void scaleConnected();
    void scaleDisconnected();

private slots:
    void onDe1Discovered(const QBluetoothDeviceInfo &device);
    void onScaleDiscovered(const QBluetoothDeviceInfo &device);
    void onDe1ConnectionChanged(bool connected);
    void onScaleConnectionChanged(bool connected);

private:
    void setupConnections();

    Settings *m_settings;
    std::unique_ptr<BLEManager> m_bleManager;
    std::unique_ptr<DE1Device> m_de1;
    ScaleDevice *m_scale = nullptr; // Owned by factory
    std::unique_ptr<HttpServer> m_httpServer;
    std::unique_ptr<WebSocketServer> m_wsServer;

    bool m_running = false;
    bool m_scaleConnecting = false; // Prevents multiple simultaneous connection attempts
};

#endif // BRIDGE_H
