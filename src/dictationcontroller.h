/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "pipewirerecorder.h"
#include "textinjector.h"
#include "whisperengine.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <memory>

class DictationController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.cesar.kdictation.Dictation")

    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(bool transcribing READ transcribing NOTIFY transcribingChanged)
    Q_PROPERTY(int recordingSeconds READ recordingSeconds NOTIFY recordingSecondsChanged)
    Q_PROPERTY(double audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool smartFormatting READ smartFormatting WRITE setSmartFormatting NOTIFY smartFormattingChanged)
    Q_PROPERTY(bool directInsertion READ directInsertion WRITE setDirectInsertion NOTIFY directInsertionChanged)
    Q_PROPERTY(bool pushToTalk READ pushToTalk WRITE setPushToTalk NOTIFY pushToTalkChanged)
    Q_PROPERTY(QString lastTranscript READ lastTranscript NOTIFY lastTranscriptChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool hudVisible READ hudVisible WRITE setHudVisible NOTIFY hudVisibleChanged)

public:
    explicit DictationController(const QString &storageDir = QString(), QObject *parent = nullptr);
    ~DictationController() override;

    bool recording() const;
    bool transcribing() const;
    int recordingSeconds() const;
    double audioLevel() const;
    QString language() const;
    void setLanguage(const QString &lang);
    bool smartFormatting() const;
    void setSmartFormatting(bool enabled);
    bool directInsertion() const;
    void setDirectInsertion(bool enabled);
    bool pushToTalk() const;
    void setPushToTalk(bool enabled);
    QString lastTranscript() const;
    QString statusMessage() const;
    QVariantList history() const;
    bool hudVisible() const;
    void setHudVisible(bool visible);

    static QString cleanWhisperOutput(const QString &raw);
    static QString applyVoiceCommands(const QString &text);
    static double computeWavRmsLevel(const QByteArray &pcmS16Data);

public Q_SLOTS:
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void addHistoryEntry(const QString &text, const QString &lang = QStringLiteral("es"));

    // D-Bus methods
    void Toggle();
    void StartDictation();
    void StopDictation();
    void SetPushToTalk(bool enabled);
    bool GetPushToTalk() const;
    void Show();
    void Hide();

Q_SIGNALS:
    void recordingChanged();
    void transcribingChanged();
    void recordingSecondsChanged();
    void audioLevelChanged();
    void languageChanged();
    void smartFormattingChanged();
    void directInsertionChanged();
    void pushToTalkChanged();
    void lastTranscriptChanged();
    void statusMessageChanged();
    void historyChanged();
    void hudVisibleChanged();

private:
    void loadState();
    void saveState() const;
    void startWhisperTranscription(const QByteArray &pcmData);
    void runSmartFormattingPass(const QString &rawTranscript);
    void finalizeTranscript(const QString &finalText);

    QString m_storageDir;
    std::unique_ptr<PipeWireRecorder> m_recorder;
    std::unique_ptr<WhisperEngine> m_whisperEngine;
    std::unique_ptr<TextInjector> m_injector;
    QTimer m_timer;
    QNetworkAccessManager m_nam;

    QString m_currentWavPath;
    bool m_recording = false;
    bool m_transcribing = false;
    int m_recordingSeconds = 0;
    double m_audioLevel = 0.0;
    QString m_language = QStringLiteral("es");
    bool m_smartFormatting = false;
    bool m_directInsertion = true;
    bool m_pushToTalk = false;
    QString m_lastTranscript;
    QString m_statusMessage = QStringLiteral("Listo para dictar (Meta+Alt+D)");
    QVariantList m_history;
    bool m_hudVisible = true;
};
