/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dictationhistorymanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

DictationHistoryManager::DictationHistoryManager(QObject *parent)
    : QObject(parent)
{
}

QString DictationHistoryManager::autostartFilePath()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + QStringLiteral("/autostart/org.cesar.kdictation.desktop");
}

bool DictationHistoryManager::isAutostartEnabled()
{
    return QFileInfo::exists(autostartFilePath());
}

void DictationHistoryManager::setAutostartEnabled(bool enabled)
{
    const QString filePath = autostartFilePath();
    if (enabled) {
        const QFileInfo fi(filePath);
        QDir().mkpath(fi.dir().path());
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            file.write("[Desktop Entry]\n"
                       "Type=Application\n"
                       "Name=KDictation\n"
                       "Comment=Universal Voice Dictation Service\n"
                       "Exec=kdictation --daemon\n"
                       "Icon=org.cesar.kdictation\n"
                       "Terminal=false\n"
                       "StartupNotify=false\n"
                       "X-KDE-autostart-phase=2\n");
        }
    } else {
        QFile::remove(filePath);
    }
}

DictationStateData DictationHistoryManager::loadState(const QString &storageDir)
{
    DictationStateData state;
    QFile file(storageDir + QStringLiteral("/history.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return state;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return state;
    }

    const QJsonObject root = doc.object();
    state.language = root.value(QStringLiteral("language")).toString(QStringLiteral("es"));
    state.whisperModel = root.value(QStringLiteral("whisperModel")).toString();
    state.aiMode = root.contains(QStringLiteral("aiMode"))
        ? root.value(QStringLiteral("aiMode")).toInt(0)
        : (root.value(QStringLiteral("smartFormatting")).toBool(false) ? 1 : 0);
    state.aiModel = root.value(QStringLiteral("aiModel")).toString(QStringLiteral("llama3.2:3b"));
    state.directInsertion = root.value(QStringLiteral("directInsertion")).toBool(true);
    state.pushToTalk = root.value(QStringLiteral("pushToTalk")).toBool(false);
    state.closeToBackground = root.value(QStringLiteral("closeToBackground")).toBool(true);

    m_history.clear();
    const QJsonArray arr = root.value(QStringLiteral("history")).toArray();
    for (const QJsonValue &val : arr) {
        if (val.isObject()) {
            m_history.append(val.toObject().toVariantMap());
        }
    }
    if (!m_history.isEmpty()) {
        m_lastTranscript = m_history.first().toMap().value(QStringLiteral("text")).toString();
    } else {
        m_lastTranscript.clear();
    }

    return state;
}

void DictationHistoryManager::saveState(const QString &storageDir, const DictationStateData &data)
{
    QJsonObject root;
    root.insert(QStringLiteral("language"), data.language);
    root.insert(QStringLiteral("whisperModel"), data.whisperModel);
    root.insert(QStringLiteral("smartFormatting"), data.aiMode > 0);
    root.insert(QStringLiteral("aiMode"), data.aiMode);
    root.insert(QStringLiteral("aiModel"), data.aiModel);
    root.insert(QStringLiteral("directInsertion"), data.directInsertion);
    root.insert(QStringLiteral("pushToTalk"), data.pushToTalk);
    root.insert(QStringLiteral("closeToBackground"), data.closeToBackground);

    QJsonArray arr;
    for (const QVariant &item : m_history) {
        arr.append(QJsonObject::fromVariantMap(item.toMap()));
    }
    root.insert(QStringLiteral("history"), arr);

    QFile file(storageDir + QStringLiteral("/history.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void DictationHistoryManager::addHistoryEntry(const QString &storageDir, const DictationStateData &data, const QString &text, const QString &lang)
{
    const QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) {
        return;
    }

    QVariantMap entry;
    entry.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    entry.insert(QStringLiteral("text"), cleaned);
    entry.insert(QStringLiteral("language"), lang);
    entry.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm · dd/MM")));

    m_history.prepend(entry);
    while (m_history.size() > 25) {
        m_history.removeLast();
    }

    m_lastTranscript = cleaned;
    saveState(storageDir, data);

    Q_EMIT lastTranscriptChanged();
    Q_EMIT historyChanged();
}

void DictationHistoryManager::clearHistory(const QString &storageDir, const DictationStateData &data)
{
    m_history.clear();
    m_lastTranscript.clear();
    saveState(storageDir, data);
    Q_EMIT historyChanged();
    Q_EMIT lastTranscriptChanged();
}
