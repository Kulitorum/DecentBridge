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
 * Provides REST API for DE1 espresso machine control and scale interaction.
 * See /api/docs for interactive API documentation (Swagger UI).
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

    // API Documentation
    void handleApiDocs(const HttpRequest &req, HttpResponse &res);
    void handleApiDocsFile(const HttpRequest &req, HttpResponse &res, const QString &filename);

    Bridge *m_bridge;
    QTcpServer *m_server = nullptr;
    QMap<QString, RouteHandler> m_getRoutes;
    QMap<QString, RouteHandler> m_postRoutes;
    QMap<QString, RouteHandler> m_putRoutes;
};

#endif // HTTPSERVER_H
