#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>

/**
 * @brief Application settings for DecentBridge
 */
class Settings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString bridgeName READ bridgeName WRITE setBridgeName NOTIFY bridgeNameChanged)
    Q_PROPERTY(int httpPort READ httpPort WRITE setHttpPort NOTIFY httpPortChanged)
    Q_PROPERTY(int webSocketPort READ webSocketPort WRITE setWebSocketPort NOTIFY webSocketPortChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)
    Q_PROPERTY(QString de1Address READ de1Address WRITE setDe1Address NOTIFY de1AddressChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    // Bridge identity
    QString bridgeName() const { return m_bridgeName; }
    void setBridgeName(const QString &name);

    // Network settings
    int httpPort() const { return m_httpPort; }
    void setHttpPort(int port);

    int webSocketPort() const { return m_webSocketPort; }
    void setWebSocketPort(int port);

    // BLE settings
    bool autoConnect() const { return m_autoConnect; }
    void setAutoConnect(bool enable);

    QString de1Address() const { return m_de1Address; }
    void setDe1Address(const QString &address);

    // Scale settings
    bool autoConnectScale() const { return m_autoConnectScale; }
    void setAutoConnectScale(bool enable);

    // Shot control
    double targetWeight() const { return m_targetWeight; }
    void setTargetWeight(double weight);

    double weightFlowMultiplier() const { return m_weightFlowMultiplier; }
    void setWeightFlowMultiplier(double multiplier);

    // Persistence
    bool loadFromFile(const QString &path);
    bool saveToFile(const QString &path);

signals:
    void bridgeNameChanged();
    void httpPortChanged();
    void webSocketPortChanged();
    void autoConnectChanged();
    void de1AddressChanged();
    void settingsChanged();

private:
    QString m_bridgeName = "DecentBridge";
    int m_httpPort = 8080;
    int m_webSocketPort = 8081;
    bool m_autoConnect = true;
    bool m_autoConnectScale = false;
    QString m_de1Address;
    double m_targetWeight = 36.0;
    double m_weightFlowMultiplier = 1.0;
};

#endif // SETTINGS_H
