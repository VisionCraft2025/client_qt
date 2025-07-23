#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QByteArray>

class FactoryMCP : public QObject {
    Q_OBJECT
public:
    explicit FactoryMCP(QObject *parent = nullptr);
    ~FactoryMCP();

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    void sendCommand(const QJsonObject& json);

signals:
    void commandResponseReceived(const QByteArray& response); // 서버 응답 수신 시
    void errorOccurred(const QString& reason); // 에러 발생 시
    void connected();
    void disconnected();

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket* m_socket;
    QByteArray m_buffer;
};
