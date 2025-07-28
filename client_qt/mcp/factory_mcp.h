#pragma once

#include <QObject>
#include <QMqttClient>

class FactoryMCP : public QObject {
    Q_OBJECT
public:
    explicit FactoryMCP(QMqttClient *mqttClient, QObject *parent = nullptr);
    void sendCommand(const QString &textCommand);

signals:
    void commandSent(const QString &payload);
    void errorOccurred(const QString &reason);

private:
    QMqttClient *m_client;
    QString m_topic = "mcp/command";
};
