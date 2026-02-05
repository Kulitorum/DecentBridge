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

    void setSkinRoot(const QString &path);

signals:
    void requestReceived(const QString &method, const QString &path);
    void webSocketUpgradeRequested(QTcpSocket *socket);

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

    // Route handlers - Water Levels
    void handleGetWaterLevels(const HttpRequest &req, HttpResponse &res);

    // Route handlers - Sensors
    void handleGetSensors(const HttpRequest &req, HttpResponse &res);
    void handleGetSensorById(const HttpRequest &req, HttpResponse &res, const QString &id);

    // Route handlers - Settings
    void handleGetSettings(const HttpRequest &req, HttpResponse &res);
    void handlePostSettings(const HttpRequest &req, HttpResponse &res);

    // Route handlers - Key-value store, workflow, shots
    void handleGetStore(const HttpRequest &req, HttpResponse &res, const QString &ns, const QString &key);
    void handlePostStore(const HttpRequest &req, HttpResponse &res, const QString &ns, const QString &key);
    void handleGetWorkflow(const HttpRequest &req, HttpResponse &res);
    void handlePutWorkflow(const HttpRequest &req, HttpResponse &res);
    void handleGetShots(const HttpRequest &req, HttpResponse &res);

    // Route handlers - Profiles
    void handleGetProfiles(const HttpRequest &req, HttpResponse &res);
    void handleGetProfileById(const HttpRequest &req, HttpResponse &res, const QString &id);
    void handlePostProfiles(const HttpRequest &req, HttpResponse &res);
    void handleDeleteProfile(const HttpRequest &req, HttpResponse &res, const QString &id);

    QString storeDir() const;
    QString profilesDir() const;
    void ensureDefaultProfiles();

    // Dev tools
    void handlePutDevSkin(const HttpRequest &req, HttpResponse &res, const QString &filePath);

    // Dashboard
    void handleDashboard(const HttpRequest &req, HttpResponse &res);

    // API Documentation
    void handleApiDocs(const HttpRequest &req, HttpResponse &res);
    void handleApiDocsFile(const HttpRequest &req, HttpResponse &res, const QString &filename);
    void handleFavicon(const HttpRequest &req, HttpResponse &res);

    // Static file serving
    bool serveStaticFile(const HttpRequest &req, HttpResponse &res);
    QString guessMimeType(const QString &filename) const;

    Bridge *m_bridge;
    QTcpServer *m_server = nullptr;
    QMap<QString, RouteHandler> m_getRoutes;
    QMap<QString, RouteHandler> m_postRoutes;
    QMap<QString, RouteHandler> m_putRoutes;
    QMap<QTcpSocket*, QByteArray> m_socketBuffers;
    QString m_skinRoot;
};

#endif // HTTPSERVER_H
