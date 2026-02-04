#include "httpserver.h"
#include "core/bridge.h"
#include "core/settings.h"
#include "ble/blemanager.h"
#include "ble/de1device.h"
#include "ble/scaledevice.h"
#include "ble/sensordevice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QUrlQuery>
#include <QFile>

Q_LOGGING_CATEGORY(lcHttp, "bridge.http")

HttpServer::HttpServer(Bridge *bridge, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
{
    setupRoutes();
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::setupRoutes()
{
    // Root - HTML dashboard
    m_getRoutes["/"] = [this](auto& req, auto& res) { handleDashboard(req, res); };

    // Favicon
    m_getRoutes["/favicon.png"] = [this](auto& req, auto& res) { handleFavicon(req, res); };

    // API Documentation - redirect to trailing slash so relative paths work
    m_getRoutes["/api"] = [](auto&, auto& res) {
        res.statusCode = 302;
        res.statusText = "Found";
        res.headers["Location"] = "/api/docs/";
    };
    m_getRoutes["/api/docs"] = [](auto&, auto& res) {
        res.statusCode = 302;
        res.statusText = "Found";
        res.headers["Location"] = "/api/docs/";
    };
    m_getRoutes["/api/docs/"] = [this](auto& req, auto& res) { handleApiDocs(req, res); };
    m_getRoutes["/api/docs/rest_v1.yml"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "rest_v1.yml"); };
    m_getRoutes["/api/docs/websocket_v1.yml"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "websocket_v1.yml"); };
    // Vendor files (Swagger UI, AsyncAPI, etc.)
    m_getRoutes["/api/docs/vendor/swagger-ui.css"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/swagger-ui.css"); };
    m_getRoutes["/api/docs/vendor/swagger-ui-bundle.js"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/swagger-ui-bundle.js"); };
    m_getRoutes["/api/docs/vendor/swagger-ui-standalone-preset.js"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/swagger-ui-standalone-preset.js"); };
    m_getRoutes["/api/docs/vendor/react.production.min.js"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/react.production.min.js"); };
    m_getRoutes["/api/docs/vendor/react-dom.production.min.js"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/react-dom.production.min.js"); };
    m_getRoutes["/api/docs/vendor/asyncapi-standalone.js"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/asyncapi-standalone.js"); };
    m_getRoutes["/api/docs/vendor/asyncapi.css"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/asyncapi.css"); };
    m_getRoutes["/api/docs/vendor/js-yaml.min.js"] = [this](auto& req, auto& res) { handleApiDocsFile(req, res, "vendor/js-yaml.min.js"); };
    m_getRoutes["/api/docs/favicon.png"] = [this](auto& req, auto& res) { handleFavicon(req, res); };

    // GET routes
    m_getRoutes["/api/v1/devices"] = [this](auto& req, auto& res) { handleGetDevices(req, res); };
    m_getRoutes["/api/v1/devices/scan"] = [this](auto& req, auto& res) { handleScanDevices(req, res); };
    m_getRoutes["/api/v1/devices/discovered"] = [this](auto& req, auto& res) { handleGetDiscoveredDevices(req, res); };
    m_getRoutes["/api/v1/machine/info"] = [this](auto& req, auto& res) { handleGetMachineInfo(req, res); };
    m_getRoutes["/api/v1/machine/state"] = [this](auto& req, auto& res) { handleGetMachineState(req, res); };
    m_getRoutes["/api/v1/machine/settings"] = [this](auto& req, auto& res) { handleGetMachineSettings(req, res); };
    m_getRoutes["/api/v1/machine/shotSettings"] = [this](auto& req, auto& res) { handleGetShotSettings(req, res); };
    m_getRoutes["/api/v1/machine/waterLevels"] = [this](auto& req, auto& res) { handleGetWaterLevels(req, res); };
    m_getRoutes["/api/v1/settings"] = [this](auto& req, auto& res) { handleGetSettings(req, res); };
    m_getRoutes["/api/v1/sensors"] = [this](auto& req, auto& res) { handleGetSensors(req, res); };

    // POST routes
    m_postRoutes["/api/v1/machine/profile"] = [this](auto& req, auto& res) { handlePostProfile(req, res); };
    m_postRoutes["/api/v1/machine/settings"] = [this](auto& req, auto& res) { handlePostMachineSettings(req, res); };
    m_postRoutes["/api/v1/machine/shotSettings"] = [this](auto& req, auto& res) { handlePostShotSettings(req, res); };
    m_postRoutes["/api/v1/settings"] = [this](auto& req, auto& res) { handlePostSettings(req, res); };

    // PUT routes
    m_putRoutes["/api/v1/devices/connect"] = [this](auto& req, auto& res) { handleConnectDevice(req, res); };
    m_putRoutes["/api/v1/scale/tare"] = [this](auto& req, auto& res) { handleTareScale(req, res); };
    m_putRoutes["/api/v1/scale/disconnect"] = [this](auto& req, auto& res) { handleDisconnectScale(req, res); };
}

bool HttpServer::start(int port)
{
    if (m_server) {
        return m_server->isListening();
    }

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, port)) {
        qCWarning(lcHttp) << "Failed to start HTTP server:" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    qCInfo(lcHttp) << "HTTP server listening on port" << port;
    return true;
}

void HttpServer::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

void HttpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &HttpServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &HttpServer::onDisconnected);
    }
}

void HttpServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    m_socketBuffers[socket] += socket->readAll();
    QByteArray &data = m_socketBuffers[socket];

    // Check if we have the complete headers (look for \r\n\r\n)
    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd == -1) return; // Headers not complete yet

    // Extract Content-Length from headers
    int contentLength = 0;
    QString headerStr = QString::fromUtf8(data.left(headerEnd));
    for (const QString &line : headerStr.split("\r\n")) {
        if (line.toLower().startsWith("content-length:")) {
            contentLength = line.mid(15).trimmed().toInt();
            break;
        }
    }

    // Check if we have the complete body
    int bodyStart = headerEnd + 4; // skip \r\n\r\n
    if (data.size() - bodyStart < contentLength) return; // Body not complete yet

    HttpRequest request;

    if (!parseRequest(data, request)) {
        HttpResponse response;
        response.setError(400, "Bad Request");
        socket->write(response.toBytes());
        socket->close();
        m_socketBuffers.remove(socket);
        return;
    }

    m_socketBuffers.remove(socket);
    handleRequest(socket, request);
}

void HttpServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        m_socketBuffers.remove(socket);
        socket->deleteLater();
    }
}

bool HttpServer::parseRequest(const QByteArray &data, HttpRequest &request)
{
    QString str = QString::fromUtf8(data);
    QStringList lines = str.split("\r\n");

    if (lines.isEmpty()) return false;

    // Parse request line: METHOD /path HTTP/1.1
    QStringList requestLine = lines[0].split(' ');
    if (requestLine.size() < 2) return false;

    request.method = requestLine[0];
    QString fullPath = requestLine[1];

    // Split path and query
    int queryIdx = fullPath.indexOf('?');
    if (queryIdx != -1) {
        request.path = fullPath.left(queryIdx);
        request.query = fullPath.mid(queryIdx + 1);
    } else {
        request.path = fullPath;
    }

    // Parse headers
    int i = 1;
    for (; i < lines.size() && !lines[i].isEmpty(); ++i) {
        int colonIdx = lines[i].indexOf(':');
        if (colonIdx > 0) {
            QString key = lines[i].left(colonIdx).trimmed().toLower();
            QString value = lines[i].mid(colonIdx + 1).trimmed();
            request.headers[key] = value;
        }
    }

    // Body is after empty line
    if (i + 1 < lines.size()) {
        request.body = lines.mid(i + 1).join("\r\n").toUtf8();
    }

    return true;
}

void HttpServer::handleRequest(QTcpSocket *socket, const HttpRequest &request)
{
    emit requestReceived(request.method, request.path);
    qCDebug(lcHttp) << request.method << request.path;

    HttpResponse response;
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type";

    // Handle CORS preflight
    if (request.method == "OPTIONS") {
        response.statusCode = 204;
        response.statusText = "No Content";
        socket->write(response.toBytes());
        socket->close();
        return;
    }

    // Route the request
    RouteHandler handler = nullptr;

    if (request.method == "GET") {
        // Check for sensor by ID pattern: /api/v1/sensors/:id
        if (request.path.startsWith("/api/v1/sensors/") && request.path != "/api/v1/sensors") {
            QString sensorId = request.path.section('/', -1);
            handleGetSensorById(request, response, sensorId);
            socket->write(response.toBytes());
            socket->close();
            return;
        }
        handler = m_getRoutes.value(request.path);
    } else if (request.method == "POST") {
        handler = m_postRoutes.value(request.path);
    } else if (request.method == "PUT") {
        // Check for state change pattern: /api/v1/machine/state/:newState
        if (request.path.startsWith("/api/v1/machine/state/")) {
            handleSetMachineState(request, response);
            socket->write(response.toBytes());
            socket->close();
            return;
        }
        handler = m_putRoutes.value(request.path);
    }

    if (handler) {
        handler(request, response);
    } else {
        qCWarning(lcHttp) << "No route for:" << request.method << request.path;
        response.setError(404, "Not Found");
    }

    socket->write(response.toBytes());
    socket->close();
}

// Response helpers
void HttpServer::HttpResponse::setJson(const QByteArray &json)
{
    headers["Content-Type"] = "application/json";
    body = json;
}

void HttpServer::HttpResponse::setError(int code, const QString &message)
{
    statusCode = code;
    statusText = message;
    QJsonObject obj;
    obj["error"] = message;
    setJson(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QByteArray HttpServer::HttpResponse::toBytes() const
{
    QByteArray result;
    result += QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8();
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        result += QString("%1: %2\r\n").arg(it.key(), it.value()).toUtf8();
    }
    result += QString("Content-Length: %1\r\n").arg(body.size()).toUtf8();
    result += "\r\n";
    result += body;
    return result;
}

// Route handlers - Devices
void HttpServer::handleGetDevices(const HttpRequest &, HttpResponse &res)
{
    QJsonArray devices;

    // Add DE1 if connected
    if (m_bridge->de1() && m_bridge->de1()->isConnected()) {
        QJsonObject de1;
        de1["name"] = m_bridge->de1()->name();
        de1["id"] = m_bridge->de1()->address();
        de1["state"] = "connected";
        de1["type"] = "machine";
        devices.append(de1);
    }

    // Add scale if connected
    if (m_bridge->scale() && m_bridge->scale()->isConnected()) {
        QJsonObject scale;
        scale["name"] = m_bridge->scale()->name();
        scale["type"] = "scale";
        scale["scaleType"] = m_bridge->scale()->type();
        scale["state"] = "connected";
        scale["weight"] = m_bridge->scale()->weight();
        devices.append(scale);
    }

    res.setJson(QJsonDocument(devices).toJson(QJsonDocument::Compact));
}

void HttpServer::handleScanDevices(const HttpRequest &req, HttpResponse &res)
{
    QUrlQuery query(req.query);
    bool quick = query.queryItemValue("quick") == "true";

    m_bridge->bleManager()->startScan();

    if (quick) {
        res.setJson("[]");
    } else {
        // Returns immediately - poll /devices/discovered for results
        res.setJson("[]");
    }
}

void HttpServer::handleConnectDevice(const HttpRequest &req, HttpResponse &res)
{
    QUrlQuery query(req.query);
    QString deviceId = query.queryItemValue("deviceId", QUrl::FullyDecoded);

    if (deviceId.isEmpty()) {
        res.setError(400, "deviceId required");
        return;
    }

    // Find the device in discovered devices and connect
    auto devices = m_bridge->bleManager()->discoveredDevices();

    for (const auto &device : devices) {
        QString addr = device.address().toString();
        if (addr == deviceId) {
            qCInfo(lcHttp) << "Connecting to:" << device.name();
            // Determine device type and connect appropriately
            if (m_bridge->bleManager()->isScale(device)) {
                m_bridge->connectToScale(device);
            } else if (m_bridge->bleManager()->isSensor(device)) {
                m_bridge->connectToSensor(device);
            }
            res.setJson("{}");
            return;
        }
    }

    res.setError(404, "Device not found");
}

void HttpServer::handleGetDiscoveredDevices(const HttpRequest &, HttpResponse &res)
{
    QJsonArray devices;

    auto discovered = m_bridge->bleManager()->discoveredDevices();

    int scaleCount = 0;
    int sensorCount = 0;
    for (const auto &device : discovered) {
        QJsonObject obj;
        obj["name"] = device.name();
        obj["address"] = device.address().toString();

        // Use BLEManager's detection logic for consistency
        QString scaleType = m_bridge->bleManager()->scaleType(device);
        QString sensorType = m_bridge->bleManager()->sensorType(device);

        if (!scaleType.isEmpty()) {
            obj["type"] = "scale";
            obj["scaleType"] = scaleType;
            scaleCount++;
        } else if (!sensorType.isEmpty()) {
            obj["type"] = "sensor";
            obj["sensorType"] = sensorType;
            sensorCount++;
        } else if (m_bridge->bleManager()->isDE1(device)) {
            obj["type"] = "machine";
        } else {
            obj["type"] = "unknown";
        }

        devices.append(obj);
    }

    qCDebug(lcHttp) << "Discovered:" << devices.size() << "devices," << scaleCount << "scales," << sensorCount << "sensors";
    res.setJson(QJsonDocument(devices).toJson(QJsonDocument::Compact));
}

// Route handlers - Machine
void HttpServer::handleGetMachineInfo(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonObject info;
    info["version"] = m_bridge->de1()->firmwareVersion();
    info["model"] = m_bridge->de1()->modelName();
    info["serialNumber"] = m_bridge->de1()->serialNumber();
    info["GHC"] = m_bridge->de1()->hasGHC();

    res.setJson(QJsonDocument(info).toJson(QJsonDocument::Compact));
}

void HttpServer::handleGetMachineState(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonObject state;
    state["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonObject stateObj;
    stateObj["state"] = m_bridge->de1()->stateString();
    stateObj["substate"] = m_bridge->de1()->subStateString();
    state["state"] = stateObj;

    state["pressure"] = m_bridge->de1()->pressure();
    state["flow"] = m_bridge->de1()->flow();
    state["mixTemperature"] = m_bridge->de1()->mixTemp();
    state["groupTemperature"] = m_bridge->de1()->headTemp();
    state["targetPressure"] = m_bridge->de1()->targetPressure();
    state["targetFlow"] = m_bridge->de1()->targetFlow();
    state["steamTemperature"] = m_bridge->de1()->steamTemp();

    res.setJson(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

void HttpServer::handleSetMachineState(const HttpRequest &req, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    // Extract state from path: /api/v1/machine/state/{newState}
    QString newState = req.path.section('/', -1);

    if (!m_bridge->de1()->requestState(newState)) {
        res.setError(400, "Invalid state: " + newState);
        return;
    }

    res.setJson("{}");
}

void HttpServer::handlePostProfile(const HttpRequest &req, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(req.body, &error);
    if (error.error != QJsonParseError::NoError) {
        res.setError(400, "Invalid JSON: " + error.errorString());
        return;
    }

    if (!m_bridge->de1()->uploadProfile(doc.object())) {
        res.setError(400, "Failed to upload profile");
        return;
    }

    res.setJson("{}");
}

void HttpServer::handleGetMachineSettings(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonObject settings;
    settings["usb"] = m_bridge->de1()->usbChargerEnabled();
    settings["fan"] = m_bridge->de1()->fanThreshold();
    // TODO: Add more settings

    res.setJson(QJsonDocument(settings).toJson(QJsonDocument::Compact));
}

void HttpServer::handlePostMachineSettings(const HttpRequest &req, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(req.body);
    QJsonObject obj = doc.object();

    if (obj.contains("usb")) {
        m_bridge->de1()->setUsbCharger(obj["usb"].toBool());
    }
    if (obj.contains("fan")) {
        m_bridge->de1()->setFanThreshold(obj["fan"].toInt());
    }

    res.statusCode = 202;
    res.statusText = "Accepted";
    res.setJson("{}");
}

void HttpServer::handleGetShotSettings(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    res.setJson(QJsonDocument(m_bridge->de1()->shotSettingsToJson()).toJson(QJsonDocument::Compact));
}

void HttpServer::handlePostShotSettings(const HttpRequest &req, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(req.body);
    QJsonObject obj = doc.object();

    // Use current values as defaults
    int steamSetting = obj.contains("steamSetting") ? obj["steamSetting"].toInt() : m_bridge->de1()->steamSetting();
    int steamTemp = obj.contains("targetSteamTemp") ? obj["targetSteamTemp"].toInt() : m_bridge->de1()->targetSteamTemp();
    int steamDuration = obj.contains("targetSteamDuration") ? obj["targetSteamDuration"].toInt() : m_bridge->de1()->targetSteamDuration();
    int hotWaterTemp = obj.contains("targetHotWaterTemp") ? obj["targetHotWaterTemp"].toInt() : m_bridge->de1()->targetHotWaterTemp();
    int hotWaterVolume = obj.contains("targetHotWaterVolume") ? obj["targetHotWaterVolume"].toInt() : m_bridge->de1()->targetHotWaterVolume();
    int hotWaterDuration = obj.contains("targetHotWaterDuration") ? obj["targetHotWaterDuration"].toInt() : m_bridge->de1()->targetHotWaterDuration();
    int shotVolume = obj.contains("targetShotVolume") ? obj["targetShotVolume"].toInt() : m_bridge->de1()->targetShotVolume();
    double groupTemp = obj.contains("groupTemp") ? obj["groupTemp"].toDouble() : m_bridge->de1()->targetGroupTemp();

    m_bridge->de1()->setShotSettings(steamSetting, steamTemp, steamDuration,
                                     hotWaterTemp, hotWaterVolume, hotWaterDuration,
                                     shotVolume, groupTemp);

    res.setJson(QJsonDocument(m_bridge->de1()->shotSettingsToJson()).toJson(QJsonDocument::Compact));
}

// Route handlers - Water Levels
void HttpServer::handleGetWaterLevels(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->de1() || !m_bridge->de1()->isConnected()) {
        res.setError(503, "DE1 not connected");
        return;
    }

    QJsonObject levels;
    levels["currentLevel"] = m_bridge->de1()->waterLevel();
    levels["refillLevel"] = 5; // Default refill threshold in mm
    res.setJson(QJsonDocument(levels).toJson(QJsonDocument::Compact));
}

// Route handlers - Sensors
void HttpServer::handleGetSensors(const HttpRequest &, HttpResponse &res)
{
    QJsonArray sensors;
    for (auto sensor : m_bridge->sensors()) {
        if (sensor->isConnected()) {
            sensors.append(sensor->toJson());
        }
    }
    res.setJson(QJsonDocument(sensors).toJson(QJsonDocument::Compact));
}

void HttpServer::handleGetSensorById(const HttpRequest &, HttpResponse &res, const QString &id)
{
    auto sensor = m_bridge->sensor(id);
    if (!sensor || !sensor->isConnected()) {
        res.setError(404, "Sensor not found");
        return;
    }

    res.setJson(QJsonDocument(sensor->toJson()).toJson(QJsonDocument::Compact));
}

// Route handlers - Scale
void HttpServer::handleTareScale(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->scale() || !m_bridge->scale()->isConnected()) {
        res.setError(404, "Scale not connected");
        return;
    }

    m_bridge->scale()->tare();
    res.setJson("{}");
}

void HttpServer::handleDisconnectScale(const HttpRequest &, HttpResponse &res)
{
    if (!m_bridge->scale()) {
        res.setError(404, "No scale");
        return;
    }

    m_bridge->disconnectScale();
    res.setJson("{}");
}

// Route handlers - Settings
void HttpServer::handleGetSettings(const HttpRequest &, HttpResponse &res)
{
    QJsonObject settings;
    settings["bridgeName"] = m_bridge->settings()->bridgeName();
    settings["httpPort"] = m_bridge->settings()->httpPort();
    settings["webSocketPort"] = m_bridge->settings()->webSocketPort();
    settings["autoConnect"] = m_bridge->settings()->autoConnect();
    settings["autoConnectScale"] = m_bridge->settings()->autoConnectScale();
    res.setJson(QJsonDocument(settings).toJson(QJsonDocument::Compact));
}

void HttpServer::handlePostSettings(const HttpRequest &req, HttpResponse &res)
{
    QJsonDocument doc = QJsonDocument::fromJson(req.body);
    QJsonObject obj = doc.object();

    if (obj.contains("bridgeName")) {
        m_bridge->settings()->setBridgeName(obj["bridgeName"].toString());
    }
    if (obj.contains("autoConnect")) {
        m_bridge->settings()->setAutoConnect(obj["autoConnect"].toBool());
    }
    if (obj.contains("autoConnectScale")) {
        m_bridge->settings()->setAutoConnectScale(obj["autoConnectScale"].toBool());
    }

    res.setJson("{}");
}

// Dashboard HTML page
void HttpServer::handleDashboard(const HttpRequest &, HttpResponse &res)
{
    res.headers["Content-Type"] = "text/html; charset=utf-8";
    res.body = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DecentBridge</title>
    <link rel="icon" type="image/png" href="/favicon.png">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #eee;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 800px; margin: 0 auto; }
        h1 {
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            background: linear-gradient(90deg, #00d9ff, #00ff88);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .card {
            background: rgba(255,255,255,0.1);
            border-radius: 16px;
            padding: 20px;
            margin-bottom: 20px;
            backdrop-filter: blur(10px);
        }
        .card h2 {
            font-size: 1.2em;
            margin-bottom: 15px;
            color: #00d9ff;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            display: inline-block;
        }
        .status-dot.connected { background: #00ff88; box-shadow: 0 0 10px #00ff88; }
        .status-dot.disconnected { background: #ff4757; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
        }
        .metric {
            background: rgba(0,0,0,0.2);
            padding: 15px;
            border-radius: 12px;
            text-align: center;
        }
        .metric .value {
            font-size: 2em;
            font-weight: bold;
            color: #fff;
        }
        .metric .label {
            font-size: 0.85em;
            color: #888;
            margin-top: 5px;
        }
        .metric.highlight .value { color: #00ff88; }
        .state-badge {
            display: inline-block;
            padding: 8px 16px;
            border-radius: 20px;
            font-weight: bold;
            font-size: 1.1em;
        }
        .state-Sleep { background: #4a4a6a; }
        .state-Idle { background: #2d5a27; }
        .state-Espresso { background: #8b4513; }
        .state-Steam { background: #4a6fa5; }
        .state-HotWater { background: #5a3d7a; }
        .buttons {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            margin-top: 15px;
        }
        button {
            padding: 12px 24px;
            border: none;
            border-radius: 8px;
            font-size: 1em;
            cursor: pointer;
            transition: transform 0.1s, opacity 0.1s;
        }
        button:hover { transform: scale(1.05); }
        button:active { transform: scale(0.95); }
        .btn-idle { background: #2d5a27; color: #fff; }
        .btn-espresso { background: #8b4513; color: #fff; }
        .btn-steam { background: #4a6fa5; color: #fff; }
        .btn-water { background: #5a3d7a; color: #fff; }
        .btn-sleep { background: #4a4a6a; color: #fff; }
        .btn-tare { background: #00d9ff; color: #000; }
        .btn-disconnect { background: #ff4757; color: #fff; }
        .btn-scan { background: #ff9f43; color: #000; }
        .btn-scan:disabled { background: #666; color: #999; cursor: not-allowed; transform: none; }
        .scale-section { margin-top: 10px; }
        #error { color: #ff4757; text-align: center; padding: 10px; }
        #scan-status {
            margin-top: 10px;
            padding: 10px;
            border-radius: 8px;
            background: rgba(0,0,0,0.2);
            display: none;
        }
        #scan-status.visible { display: block; }
        .scale-list { margin-top: 10px; }
        .scale-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px;
            margin: 5px 0;
            background: rgba(255,255,255,0.1);
            border-radius: 8px;
        }
        .scale-item button { padding: 8px 16px; }
        .spinner {
            display: inline-block;
            width: 16px;
            height: 16px;
            border: 2px solid #fff;
            border-top-color: transparent;
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin-right: 8px;
            vertical-align: middle;
        }
        @keyframes spin { to { transform: rotate(360deg); } }
        .api-info {
            font-size: 0.85em;
            color: #666;
            text-align: center;
            margin-top: 30px;
        }
        .api-info a { color: #00d9ff; }
    </style>
</head>
<body>
    <div class="container">
        <h1>DecentBridge <span style="font-size:0.4em;background:rgba(255,255,255,0.1);padding:0.2em 0.6em;border-radius:1em;vertical-align:middle;">v0.1.0</span></h1>

        <div id="error"></div>

        <div class="card" id="machine-card">
            <h2>
                <span class="status-dot" id="machine-status"></span>
                DE1 Espresso Machine
            </h2>
            <div style="margin-bottom:15px">
                <span class="state-badge" id="machine-state">--</span>
            </div>
            <div class="grid">
                <div class="metric highlight">
                    <div class="value" id="group-temp">--</div>
                    <div class="label">Group Temp</div>
                </div>
                <div class="metric">
                    <div class="value" id="steam-temp">--</div>
                    <div class="label">Steam Temp</div>
                </div>
                <div class="metric">
                    <div class="value" id="pressure">--</div>
                    <div class="label">Pressure (bar)</div>
                </div>
                <div class="metric">
                    <div class="value" id="flow">--</div>
                    <div class="label">Flow (ml/s)</div>
                </div>
            </div>
            <div class="buttons">
                <button class="btn-idle" onclick="setState('idle')">Idle</button>
                <button class="btn-espresso" onclick="setState('espresso')">Espresso</button>
                <button class="btn-steam" onclick="setState('steam')">Steam</button>
                <button class="btn-water" onclick="setState('water')">Hot Water</button>
                <button class="btn-sleep" onclick="setState('sleep')">Sleep</button>
            </div>
        </div>

        <div class="card" id="scale-card">
            <h2>
                <span class="status-dot" id="scale-status"></span>
                Scale
                <span id="scale-name" style="font-weight:normal;color:#888"></span>
            </h2>
            <div class="grid scale-section" id="scale-metrics">
                <div class="metric highlight">
                    <div class="value" id="weight">--</div>
                    <div class="label">Weight (g)</div>
                </div>
                <div class="metric">
                    <div class="value" id="weight-flow">--</div>
                    <div class="label">Flow (g/s)</div>
                </div>
            </div>
            <div class="buttons">
                <button class="btn-tare" onclick="tareScale()" id="btn-tare">Tare</button>
                <button class="btn-disconnect" onclick="disconnectScale()" id="btn-disconnect">Disconnect</button>
                <button class="btn-scan" onclick="scanForScales()" id="btn-scan">Scan for Scale</button>
            </div>
            <div id="scan-status"></div>
            <div id="scale-list" class="scale-list"></div>
        </div>

        <div class="api-info">
            <a href="/api/docs" style="font-weight:bold;">API Documentation</a><br>
            WebSocket: ws://[host]:8081/ws/v1/scale/snapshot
        </div>
    </div>

    )HTML" R"HTML(<script>
        let scaleWs = null;
        let machineWs = null;

        async function fetchData() {
            try {
                // Fetch connected devices
                const devRes = await fetch('/api/v1/devices');
                const devices = await devRes.json();

                const machine = devices.find(d => d.type === 'machine');
                const scale = devices.find(d => d.type === 'scale');

                document.getElementById('machine-status').className =
                    'status-dot ' + (machine ? 'connected' : 'disconnected');
                document.getElementById('scale-status').className =
                    'status-dot ' + (scale ? 'connected' : 'disconnected');
                document.getElementById('scale-name').textContent =
                    scale ? scale.name : '(not connected)';

                // Update weight from API if scale connected
                if (scale && scale.weight !== undefined) {
                    document.getElementById('weight').textContent = scale.weight.toFixed(1);
                }

                // If no scale connected, show discovered scales automatically
                if (!scale && !scanning) {
                    const discRes = await fetch('/api/v1/devices/discovered');
                    const discovered = await discRes.json();
                    const foundScales = discovered.filter(d => d.type === 'scale');
                    const list = document.getElementById('scale-list');
                    const status = document.getElementById('scan-status');

                    if (foundScales.length > 0) {
                        list.innerHTML = foundScales.map(s =>
                            '<div class=\"scale-item\">' +
                            '<span>' + s.name + ' <small style=\"color:#888\">(' + s.scaleType + ')</small></span>' +
                            '<button class=\"btn-tare\" onclick=\"connectScale(\'' + s.address + '\')\">Connect</button>' +
                            '</div>'
                        ).join('');
                        status.className = 'visible';
                        status.innerHTML = foundScales.length + ' scale(s) found. Click Connect to pair.';
                    }
                }

                document.getElementById('error').textContent = '';
            } catch (e) {
                document.getElementById('error').textContent = 'Connection error: ' + e.message;
            }
        }

        function connectScaleWebSocket() {
            const host = window.location.hostname;
            scaleWs = new WebSocket('ws://' + host + ':8081/ws/v1/scale/snapshot');

            scaleWs.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    document.getElementById('weight').textContent = (data.weight || 0).toFixed(1);
                    document.getElementById('weight-flow').textContent = (data.weightFlow || 0).toFixed(1);
                } catch (e) {}
            };

            scaleWs.onclose = () => setTimeout(connectScaleWebSocket, 2000);
            scaleWs.onerror = () => scaleWs.close();
        }

        function connectMachineWebSocket() {
            const host = window.location.hostname;
            machineWs = new WebSocket('ws://' + host + ':8081/ws/v1/machine/snapshot');

            machineWs.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    // Update machine metrics from real-time shot samples
                    if (data.groupTemperature !== undefined) {
                        document.getElementById('group-temp').textContent = Math.round(data.groupTemperature) + '°';
                    }
                    if (data.steamTemperature !== undefined) {
                        document.getElementById('steam-temp').textContent = Math.round(data.steamTemperature) + '°';
                    }
                    if (data.pressure !== undefined) {
                        document.getElementById('pressure').textContent = data.pressure.toFixed(1);
                    }
                    if (data.flow !== undefined) {
                        document.getElementById('flow').textContent = data.flow.toFixed(1);
                    }
                    // Update state if present
                    if (data.state?.state) {
                        const stateName = data.state.state;
                        document.getElementById('machine-state').textContent = stateName;
                        document.getElementById('machine-state').className = 'state-badge state-' + stateName;
                    }
                    // Mark machine as connected
                    document.getElementById('machine-status').className = 'status-dot connected';
                } catch (e) {}
            };

            machineWs.onclose = () => setTimeout(connectMachineWebSocket, 2000);
            machineWs.onerror = () => machineWs.close();
        }

        async function setState(state) {
            try {
                await fetch('/api/v1/machine/state/' + state, { method: 'PUT' });
                setTimeout(fetchData, 500);
            } catch (e) {
                document.getElementById('error').textContent = 'Failed to set state: ' + e.message;
            }
        }

        async function tareScale() {
            try {
                await fetch('/api/v1/scale/tare', { method: 'PUT' });
            } catch (e) {
                document.getElementById('error').textContent = 'Failed to tare: ' + e.message;
            }
        }

        async function disconnectScale() {
            try {
                await fetch('/api/v1/scale/disconnect', { method: 'PUT' });
                document.getElementById('weight').textContent = '--';
                document.getElementById('weight-flow').textContent = '--';
                setTimeout(fetchData, 500);
            } catch (e) {
                document.getElementById('error').textContent = 'Failed to disconnect: ' + e.message;
            }
        }

        let scanning = false;
        let stopScan = false;

        async function scanForScales() {
            if (scanning) return;

            const btn = document.getElementById('btn-scan');
            const status = document.getElementById('scan-status');
            const list = document.getElementById('scale-list');

            scanning = true;
            stopScan = false;
            btn.disabled = true;
            btn.innerHTML = '<span class="spinner"></span>Scanning...';
            status.className = 'visible';
            status.innerHTML = 'Scanning for Bluetooth scales...';
            list.innerHTML = '';

            try {
                // Start scan
                await fetch('/api/v1/devices/scan');

                // Poll for results over 10 seconds
                let foundScales = [];
                for (let i = 0; i < 10 && !stopScan; i++) {
                    await new Promise(r => setTimeout(r, 1000));
                    if (stopScan) break;

                    status.innerHTML = 'Scanning... ' + (10 - i) + 's remaining';

                    const res = await fetch('/api/v1/devices/discovered');
                    const devices = await res.json();
                    foundScales = devices.filter(d => d.type === 'scale');

                    // Update list
                    if (foundScales.length > 0) {
                        list.innerHTML = foundScales.map(s =>
                            '<div class="scale-item">' +
                            '<span>' + s.name + ' <small style="color:#888">(' + s.scaleType + ')</small></span>' +
                            '<button class="btn-tare" onclick="connectScale(\'' + s.address + '\')">Connect</button>' +
                            '</div>'
                        ).join('');
                    }
                }

                if (!stopScan) {
                    if (foundScales.length === 0) {
                        status.innerHTML = 'No scales found. Make sure your scale is on and in pairing mode.';
                    } else {
                        status.innerHTML = 'Found ' + foundScales.length + ' scale(s). Click Connect to pair.';
                    }
                }

            } catch (e) {
                if (!stopScan) {
                    status.innerHTML = 'Scan failed: ' + e.message;
                }
            } finally {
                scanning = false;
                btn.disabled = false;
                btn.innerHTML = 'Scan for Scale';
            }
        }

        function resetScanUI() {
            const btn = document.getElementById('btn-scan');
            const status = document.getElementById('scan-status');
            const list = document.getElementById('scale-list');
            status.className = '';
            status.innerHTML = '';
            list.innerHTML = '';
            btn.disabled = false;
            btn.innerHTML = 'Scan for Scale';
        }

        async function connectScale(address) {
            // Stop scanning immediately
            stopScan = true;
            scanning = false;

            const status = document.getElementById('scan-status');
            const list = document.getElementById('scale-list');
            list.innerHTML = '';
            status.innerHTML = '<span class="spinner"></span>Connecting to scale...';

            try {
                const res = await fetch('/api/v1/devices/connect?deviceId=' + encodeURIComponent(address), {
                    method: 'PUT'
                });

                if (res.ok) {
                    status.innerHTML = 'Connected!';
                    setTimeout(() => {
                        resetScanUI();
                        fetchData();
                    }, 1000);
                } else {
                    status.innerHTML = 'Failed to connect. Try again.';
                }
            } catch (e) {
                status.innerHTML = 'Connection error: ' + e.message;
            }
        }

        // Initial fetch and start polling (slower since WebSocket handles real-time data)
        fetchData();
        setInterval(fetchData, 5000);
        connectScaleWebSocket();
        connectMachineWebSocket();
    </script>
</body>
</html>
)HTML";
}

// API Documentation
void HttpServer::handleApiDocs(const HttpRequest &, HttpResponse &res)
{
    QFile file(":/assets/api/index.html");
    if (file.open(QIODevice::ReadOnly)) {
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        res.body = file.readAll();
    } else {
        res.setError(404, "API docs not found");
    }
}

void HttpServer::handleApiDocsFile(const HttpRequest &, HttpResponse &res, const QString &filename)
{
    QString resourcePath = ":/assets/api/" + filename;
    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly)) {
        if (filename.endsWith(".yml") || filename.endsWith(".yaml")) {
            res.headers["Content-Type"] = "text/yaml; charset=utf-8";
        } else if (filename.endsWith(".json")) {
            res.headers["Content-Type"] = "application/json";
        } else if (filename.endsWith(".js")) {
            res.headers["Content-Type"] = "application/javascript; charset=utf-8";
        } else if (filename.endsWith(".css")) {
            res.headers["Content-Type"] = "text/css; charset=utf-8";
        } else {
            res.headers["Content-Type"] = "text/plain; charset=utf-8";
        }
        res.body = file.readAll();
    } else {
        res.setError(404, "File not found: " + filename);
    }
}

void HttpServer::handleFavicon(const HttpRequest &, HttpResponse &res)
{
    QFile file(":/assets/api/favicon.png");
    if (file.open(QIODevice::ReadOnly)) {
        res.headers["Content-Type"] = "image/png";
        res.headers["Cache-Control"] = "public, max-age=86400";
        res.body = file.readAll();
    } else {
        res.setError(404, "Favicon not found");
    }
}
