#include "factory_mcp.h"
#include <QJsonDocument>
#include <QHostAddress>
#include <QDebug>

FactoryMCP::FactoryMCP(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &FactoryMCP::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &FactoryMCP::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &FactoryMCP::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &FactoryMCP::onErrorOccurred);
}

FactoryMCP::~FactoryMCP() {
    if (m_socket->isOpen()) {
        m_socket->close();
    }
}

void FactoryMCP::connectToServer(const QString& host, quint16 port) {
    if (m_socket->isOpen()) {
        m_socket->close();
    }
    m_socket->connectToHost(host, port);
}

void FactoryMCP::disconnectFromServer() {
    m_socket->disconnectFromHost();
}

bool FactoryMCP::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void FactoryMCP::sendCommand(const QJsonObject& json) {
    if (!isConnected()) {
        emit errorOccurred("서버에 연결되어 있지 않습니다.");
        return;
    }
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    // 간단하게 길이 prefix 없이 전송 (서버도 한 줄씩 처리한다고 가정했음)
    data.append('\n');
    m_socket->write(data);
    m_socket->flush();
}

void FactoryMCP::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    while (true) {
        int newlineIdx = m_buffer.indexOf('\n');
        if (newlineIdx < 0) break;
        QByteArray line = m_buffer.left(newlineIdx);
        m_buffer.remove(0, newlineIdx + 1);
        emit commandResponseReceived(line);
    }
}

void FactoryMCP::onConnected() {
    emit connected();
}

void FactoryMCP::onDisconnected() {
    emit disconnected();
}

void FactoryMCP::onErrorOccurred(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError);
    emit errorOccurred(m_socket->errorString());
}
