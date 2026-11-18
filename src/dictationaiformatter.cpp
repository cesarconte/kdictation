/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dictationaiformatter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

DictationAiFormatter::DictationAiFormatter(QObject *parent)
    : QObject(parent)
{
}

void DictationAiFormatter::refreshAvailableAiModels()
{
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:11434/api/tags")));
    req.setTransferTimeout(3000);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QJsonArray models = doc.object().value(QStringLiteral("models")).toArray();
            QStringList modelNames;
            for (const QJsonValue &val : models) {
                const QString name = val.toObject().value(QStringLiteral("name")).toString();
                if (!name.isEmpty() && !modelNames.contains(name)) {
                    modelNames.append(name);
                }
            }
            if (!modelNames.isEmpty()) {
                Q_EMIT availableAiModelsDiscovered(modelNames);
            }
        }
        reply->deleteLater();
    });
}

void DictationAiFormatter::formatTranscript(const QString &rawTranscript, int aiMode, const QString &aiModel)
{
    const QString model = aiModel.isEmpty() ? QStringLiteral("llama3.2:3b") : aiModel;

    QJsonObject payload;
    payload.insert(QStringLiteral("model"), model);
    payload.insert(QStringLiteral("stream"), false);
    payload.insert(QStringLiteral("keep_alive"), QStringLiteral("15m"));

    QJsonObject options;
    options.insert(QStringLiteral("temperature"), 0.1);
    options.insert(QStringLiteral("num_predict"), qMax(128, rawTranscript.length() * 2));
    options.insert(QStringLiteral("top_p"), 0.9);
    payload.insert(QStringLiteral("options"), options);

    QString systemPrompt;
    if (aiMode == 2) {
        systemPrompt = QStringLiteral(
            "Eres un asistente de redacción y dictado profesional. "
            "Transforma el texto dictado en un texto formal, bien redactado y estructurado con puntuación y sintaxis impecables. "
            "Devuelve ÚNICAMENTE el texto final, sin explicaciones ni introducciones.");
    } else {
        systemPrompt = QStringLiteral(
            "Eres un asistente de dictado por voz de alta precisión. "
            "Tu tarea es pulir el texto eliminando muletillas (eh, este, em, o sea, repeticiones) y corrigiendo ortografía, mayúsculas y signos de puntuación. "
            "Conserva estrictamente las palabras y el sentido original del usuario sin añadir comentarios ni comillas. "
            "Devuelve ÚNICAMENTE el texto final pulido.");
    }

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg.insert(QStringLiteral("role"), QStringLiteral("system"));
    sysMsg.insert(QStringLiteral("content"), systemPrompt);
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), rawTranscript);
    messages.append(userMsg);

    payload.insert(QStringLiteral("messages"), messages);

    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(5000);

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, rawTranscript]() {
        QString result = rawTranscript;
        bool success = false;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QString aiText = doc.object()
                                       .value(QStringLiteral("message"))
                                       .toObject()
                                       .value(QStringLiteral("content"))
                                       .toString()
                                       .trimmed();
            if (!aiText.isEmpty()) {
                result = aiText;
                success = true;
            }
        }
        reply->deleteLater();

        if (success) {
            Q_EMIT formattingFinished(result);
        } else {
            Q_EMIT formattingFailed(rawTranscript, QStringLiteral("Listo (IA no disponible, usando modo local)"));
        }
    });
}
