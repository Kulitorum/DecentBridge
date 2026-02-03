#include "settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSettings, "bridge.settings")

Settings::Settings(QObject *parent)
    : QObject(parent)
{
}

void Settings::setHttpPort(int port)
{
    if (m_httpPort != port) {
        m_httpPort = port;
        emit httpPortChanged();
        emit settingsChanged();
    }
}

void Settings::setWebSocketPort(int port)
{
    if (m_webSocketPort != port) {
        m_webSocketPort = port;
        emit webSocketPortChanged();
        emit settingsChanged();
    }
}

void Settings::setAutoConnect(bool enable)
{
    if (m_autoConnect != enable) {
        m_autoConnect = enable;
        emit autoConnectChanged();
        emit settingsChanged();
    }
}

void Settings::setDe1Address(const QString &address)
{
    if (m_de1Address != address) {
        m_de1Address = address;
        emit de1AddressChanged();
        emit settingsChanged();
    }
}

void Settings::setAutoConnectScale(bool enable)
{
    if (m_autoConnectScale != enable) {
        m_autoConnectScale = enable;
        emit settingsChanged();
    }
}

void Settings::setTargetWeight(double weight)
{
    if (!qFuzzyCompare(m_targetWeight, weight)) {
        m_targetWeight = weight;
        emit settingsChanged();
    }
}

void Settings::setWeightFlowMultiplier(double multiplier)
{
    if (!qFuzzyCompare(m_weightFlowMultiplier, multiplier)) {
        m_weightFlowMultiplier = multiplier;
        emit settingsChanged();
    }
}

bool Settings::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcSettings) << "Failed to open config file:" << path;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(lcSettings) << "Failed to parse config file:" << error.errorString();
        return false;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("httpPort"))
        m_httpPort = obj["httpPort"].toInt();
    if (obj.contains("webSocketPort"))
        m_webSocketPort = obj["webSocketPort"].toInt();
    if (obj.contains("autoConnect"))
        m_autoConnect = obj["autoConnect"].toBool();
    if (obj.contains("autoConnectScale"))
        m_autoConnectScale = obj["autoConnectScale"].toBool();
    if (obj.contains("de1Address"))
        m_de1Address = obj["de1Address"].toString();
    if (obj.contains("targetWeight"))
        m_targetWeight = obj["targetWeight"].toDouble();
    if (obj.contains("weightFlowMultiplier"))
        m_weightFlowMultiplier = obj["weightFlowMultiplier"].toDouble();

    qCInfo(lcSettings) << "Loaded settings from" << path;
    emit settingsChanged();
    return true;
}

bool Settings::saveToFile(const QString &path)
{
    QJsonObject obj;
    obj["httpPort"] = m_httpPort;
    obj["webSocketPort"] = m_webSocketPort;
    obj["autoConnect"] = m_autoConnect;
    obj["autoConnectScale"] = m_autoConnectScale;
    obj["de1Address"] = m_de1Address;
    obj["targetWeight"] = m_targetWeight;
    obj["weightFlowMultiplier"] = m_weightFlowMultiplier;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcSettings) << "Failed to write config file:" << path;
        return false;
    }

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    qCInfo(lcSettings) << "Saved settings to" << path;
    return true;
}
