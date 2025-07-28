#include "factory_mcp.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

FactoryMCP::FactoryMCP(QMqttClient *mqttClient, QObject *parent)
    : QObject(parent), m_client(mqttClient) {}

void FactoryMCP::sendCommand(const QString &textCommand) {
    if(!m_client || m_client->state() != QMqttClient::Connected) {
        emit errorOccurred("MQTT 연결이 되어있지 않습니다.");
        return;
    }

    QJsonObject json;
    json["command"] = textCommand;
    QJsonDocument doc(json);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    m_client->publish(m_topic, payload);
    emit commandSent(payload);
    qDebug() << "[MCP] 전송됨:" << payload;
}
