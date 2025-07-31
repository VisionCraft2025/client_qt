// ai_command.cpp
#include "ai_command.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

GeminiRequester::GeminiRequester(QObject* parent, const QString& apiKey)
    : QObject(parent), apiKey(apiKey) {}

void GeminiRequester::askGemini(QWidget* parent) {
    QString userInput = QInputDialog::getText(parent, "Gemini", "AI에게 물어볼 내용을 입력하세요:");

    if (userInput.isEmpty()) return;

    QNetworkAccessManager* manager = new QNetworkAccessManager(parent);

    QString urlStr = QString("https://generativelanguage.googleapis.com/v1/models/gemini-2.0-flash:generateContent?key=%1")
                         .arg(apiKey);
    //QNetworkRequest request(QUrl(urlStr));
    QNetworkRequest request{ QUrl(urlStr) };


    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["parts"] = QJsonArray{ QJsonObject{{"text", userInput}} };

    QJsonObject body;
    body["contents"] = QJsonArray{ userMessage };

    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, parent, [reply, parent]() {
        QByteArray response = reply->readAll();
        qDebug() << "[Gemini 응답 원문]:" << response;

        QJsonDocument json = QJsonDocument::fromJson(response);
        QString answer;

        if (json.isObject()) {
            QJsonObject obj = json.object();
            if (obj.contains("candidates")) {
                QJsonArray candidates = obj["candidates"].toArray();
                if (!candidates.isEmpty()) {
                    QJsonObject first = candidates[0].toObject();
                    if (first.contains("content")) {
                        QJsonObject content = first["content"].toObject();
                        if (content.contains("parts")) {
                            QJsonArray parts = content["parts"].toArray();
                            if (!parts.isEmpty()) {
                                answer = parts[0].toObject()["text"].toString();
                            }
                        }
                    }
                }
            }
        }

        if (answer.isEmpty()) {
            answer = "답변을 불러올 수 없습니다.";
        }

        QMessageBox::information(parent, "Gemini 응답", answer);
        reply->deleteLater();
    });
}

// 오버로딩

void GeminiRequester::askGemini(QWidget* parent, const QString& userText, std::function<void(QString)> callback) {
    QNetworkAccessManager* manager = new QNetworkAccessManager(parent);

    QString urlStr = QString("https://generativelanguage.googleapis.com/v1/models/gemini-2.0-flash:generateContent?key=%1")
                         .arg(apiKey);
    QNetworkRequest request{ QUrl(urlStr) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["parts"] = QJsonArray{ QJsonObject{{"text", userText}} };

    QJsonObject body;
    body["contents"] = QJsonArray{ userMessage };

    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, parent, [reply, callback]() {
        QString answer = "응답을 불러올 수 없습니다.";
        QByteArray response = reply->readAll();
        QJsonDocument json = QJsonDocument::fromJson(response);

        if (json.isObject()) {
            QJsonObject obj = json.object();
            if (obj.contains("candidates")) {
                QJsonArray candidates = obj["candidates"].toArray();
                if (!candidates.isEmpty()) {
                    QJsonObject first = candidates[0].toObject();
                    if (first.contains("content")) {
                        QJsonObject content = first["content"].toObject();
                        if (content.contains("parts")) {
                            QJsonArray parts = content["parts"].toArray();
                            if (!parts.isEmpty()) {
                                answer = parts[0].toObject()["text"].toString();
                            }
                        }
                    }
                }
            }
        }

        callback(answer);
        reply->deleteLater();
    });
}

