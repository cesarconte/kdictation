/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

struct DictationStateData {
    QString language = QStringLiteral("es");
    QString whisperModel;
    int aiMode = 0;
    QString aiModel = QStringLiteral("llama3.2:3b");
    bool directInsertion = true;
    bool pushToTalk = false;
    bool closeToBackground = true;
};

class DictationHistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit DictationHistoryManager(QObject *parent = nullptr);
    ~DictationHistoryManager() override = default;

    static QString autostartFilePath();
    static bool isAutostartEnabled();
    static void setAutostartEnabled(bool enabled);

    DictationStateData loadState(const QString &storageDir);
    void saveState(const QString &storageDir, const DictationStateData &data);

    void addHistoryEntry(const QString &storageDir, const DictationStateData &data, const QString &text, const QString &lang);
    void clearHistory(const QString &storageDir, const DictationStateData &data);
    void clearLastTranscript(const QString &storageDir, const DictationStateData &data);
    void deleteHistoryEntry(const QString &storageDir, const DictationStateData &data, const QString &id);

    QVariantList history() const { return m_history; }
    QString lastTranscript() const { return m_lastTranscript; }

Q_SIGNALS:
    void historyChanged();
    void lastTranscriptChanged();

private:
    QVariantList m_history;
    QString m_lastTranscript;
};
