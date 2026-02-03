#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <functional>

class Bridge;

/**
 * @brief Lightweight HTTP REST server
 *
 * Implements the REA API specification for DE1 control.
 * Endpoints:
 *   GET  /api/v1/devices          - List available devices
 *   GET  /api/v1/devices/scan     - Trigger device scan
 *   PUT  /api/v1/devices/connect  - Connect to a device
 *   GET  /api/v1/machine/info     - Get machine info
 *   GET  /api/v1/machine/state    - Get current state
 *   PUT  /api/v1/machine/state/:s - Request state change
 *   POST /api/v1/machine/profile  - Upload profile
 *   GET  /api/v1/machine/settings - Get machine settings
 *   POST /api/v1/machine/settings - Set machine settings
 *   PUT  /api/v1/scale/tare       - Tare scale
 *   GET  /api/v1/settings         - Get bridge settings
 *   POST /api/v1/settings         - Set bridge settings
 */
class HttpServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpServer(Bridge *bridge, QObject *parent = nullptr);
    ~HttpServer();

    bool start(int port);
    void stop();

    bool isRunning() const { return m_server && m_server->isListening(); }

signals:
    void requestReceived(const QString &method, const QString &path);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct HttpRequest {
        QString method;
        QString path;
        QString query;
        QMap<QString, QString> headers;
        QByteArray body;
    };

    struct HttpResponse {
        int statusCode = 200;
        QString statusText = "OK";
        QMap<QString, QString> headers;
        QByteArray body;

        void setJson(const QByteArray &json);
        void setError(int code, const QString &message);
        QByteArray toBytes() const;
    };

    using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

    void setupRoutes();
    void handleRequest(QTcpSocket *socket, const HttpRequest &request);
    bool parseRequest(const QByteArray &data, HttpRequest &request);

    // Route handlers - Devices
    void handleGetDevices(const HttpRequest &req, HttpResponse &res);
    void handleScanDevices(const HttpRequest &req, HttpResponse &res);
    void handleConnectDevice(const HttpRequest &req, HttpResponse &res);
    void handleGetDiscoveredDevices(const HttpRequest &req, HttpResponse &res);

    // Route handlers - Machine
    void handleGetMachineInfo(const HttpRequest &req, HttpResponse &res);
    void handleGetMachineState(const HttpRequest &req, HttpResponse &res);
    void handleSetMachineState(const HttpRequest &req, HttpResponse &res);
    void handlePostProfile(const HttpRequest &req, HttpResponse &res);
    void handleGetMachineSettings(const HttpRequest &req, HttpResponse &res);
    void handlePostMachineSettings(const HttpRequest &req, HttpResponse &res);
    void handleGetShotSettings(const HttpRequest &req, HttpResponse &res);
    void handlePostShotSettings(const HttpRequest &req, HttpResponse &res);

    // Route handlers - Scale
    void handleTareScale(const HttpRequest &req, HttpResponse &res);
    void handleDisconnectScale(const HttpRequest &req, HttpResponse &res);

    // Route handlers - Settings
    void handleGetSettings(const HttpRequest &req, HttpResponse &res);
    void handlePostSettings(const HttpRequest &req, HttpResponse &res);

    // Dashboard
    void handleDashboard(const HttpRequest &req, HttpResponse &res);

    Bridge *m_bridge;
    QTcpServer *m_server = nullptr;
    QMap<QString, RouteHandler> m_getRoutes;
    QMap<QString, RouteHandler> m_postRoutes;
    QMap<QString, RouteHandler> m_putRoutes;
};

#endif // HTTPSERVER_H
