/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "dictationaiformatter.h"
#include "dictationhistorymanager.h"
#include "pipewirerecorder.h"
#include "textinjector.h"
#include "voicepostprocessor.h"
#include "whisperengine.h"

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
    Q_PROPERTY(QString whisperModel READ whisperModel WRITE setWhisperModel NOTIFY whisperModelChanged)
    Q_PROPERTY(QStringList availableWhisperModels READ availableWhisperModels NOTIFY availableWhisperModelsChanged)
    Q_PROPERTY(bool smartFormatting READ smartFormatting WRITE setSmartFormatting NOTIFY smartFormattingChanged)
    Q_PROPERTY(int aiMode READ aiMode WRITE setAiMode NOTIFY aiModeChanged)
    Q_PROPERTY(QString aiModel READ aiModel WRITE setAiModel NOTIFY aiModelChanged)
    Q_PROPERTY(QStringList availableAiModels READ availableAiModels NOTIFY availableAiModelsChanged)
    Q_PROPERTY(bool directInsertion READ directInsertion WRITE setDirectInsertion NOTIFY directInsertionChanged)
    Q_PROPERTY(bool pushToTalk READ pushToTalk WRITE setPushToTalk NOTIFY pushToTalkChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool closeToBackground READ closeToBackground WRITE setCloseToBackground NOTIFY closeToBackgroundChanged)
    Q_PROPERTY(QString lastTranscript READ lastTranscript NOTIFY lastTranscriptChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool hudVisible READ hudVisible WRITE setHudVisible NOTIFY hudVisibleChanged)

public:
    explicit DictationController(const QString &storageDir = QString(), QObject *parent = nullptr);
    ~DictationController() override;

    bool recording() const { return m_recording; }
    bool transcribing() const { return m_transcribing; }
    int recordingSeconds() const { return m_recordingSeconds; }
    double audioLevel() const { return m_audioLevel; }
    QString language() const { return m_state.language; }
    void setLanguage(const QString &lang);
    QString whisperModel() const { return m_state.whisperModel; }
    void setWhisperModel(const QString &modelName);
    QStringList availableWhisperModels() const { return m_availableWhisperModels; }
    bool smartFormatting() const { return m_state.aiMode > 0; }
    void setSmartFormatting(bool enabled);
    int aiMode() const { return m_state.aiMode; }
    void setAiMode(int mode);
    QString aiModel() const { return m_state.aiModel; }
    void setAiModel(const QString &model);
    QStringList availableAiModels() const { return m_availableAiModels; }
    bool directInsertion() const { return m_state.directInsertion; }
    void setDirectInsertion(bool enabled);
    bool pushToTalk() const { return m_state.pushToTalk; }
    void setPushToTalk(bool enabled);
    bool autostart() const;
    void setAutostart(bool enabled);
    bool closeToBackground() const { return m_state.closeToBackground; }
    void setCloseToBackground(bool enabled);
    QString lastTranscript() const { return m_historyManager.lastTranscript(); }
    QString statusMessage() const { return m_statusMessage; }
    QVariantList history() const { return m_historyManager.history(); }
    bool hudVisible() const { return m_hudVisible; }
    void setHudVisible(bool visible);

    // Static forwarders preserving backward compatibility & tests
    static QString cleanWhisperOutput(const QString &raw)
    {
        return VoicePostProcessor::cleanWhisperOutput(raw);
    }
    static QString applyVoiceCommands(const QString &text)
    {
        return VoicePostProcessor::applyVoiceCommands(text);
    }
    static QString removeFillerWords(const QString &text, const QString &lang = QStringLiteral("es"))
    {
        return VoicePostProcessor::removeFillerWords(text, lang);
    }
    static double computeWavRmsLevel(const QByteArray &pcmS16Data)
    {
        return VoicePostProcessor::computeWavRmsLevel(pcmS16Data);
    }

public Q_SLOTS:
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void addHistoryEntry(const QString &text, const QString &lang = QStringLiteral("es"));
    Q_INVOKABLE void refreshAvailableAiModels();
    Q_INVOKABLE void quitApplication();

    // D-Bus methods
    void Toggle();
    void StartDictation();
    void StopDictation();
    void SetPushToTalk(bool enabled);
    bool GetPushToTalk() const;
    void Show();
    void Hide();
    QString TranscribePcm(const QByteArray &pcmData, const QString &lang = QStringLiteral("es"));
    QString TranscribeAndInjectPcm(const QByteArray &pcmData, const QString &lang = QStringLiteral("es"));
    void InjectText(const QString &text);
    bool IsEngineReady() const;

    void setEphemeralMode(bool enabled, int timeoutSeconds = 60);
    bool isEphemeralMode() const;
    void resetEphemeralTimer();

Q_SIGNALS:
    void recordingChanged();
    void transcribingChanged();
    void recordingSecondsChanged();
    void audioLevelChanged();
    void languageChanged();
    void whisperModelChanged();
    void availableWhisperModelsChanged();
    void smartFormattingChanged();
    void aiModeChanged();
    void aiModelChanged();
    void availableAiModelsChanged();
    void directInsertionChanged();
    void pushToTalkChanged();
    void autostartChanged();
    void closeToBackgroundChanged();
    void lastTranscriptChanged();
    void statusMessageChanged();
    void historyChanged();
    void hudVisibleChanged();

private:
    void saveState() const;
    void startWhisperTranscription(const QByteArray &pcmData);
    void finalizeTranscript(const QString &finalText);

    QString m_storageDir;
    DictationStateData m_state;
    DictationHistoryManager m_historyManager;
    DictationAiFormatter m_aiFormatter;

    std::unique_ptr<PipeWireRecorder> m_recorder;
    std::unique_ptr<WhisperEngine> m_whisperEngine;
    std::unique_ptr<TextInjector> m_injector;
    QTimer m_timer;
    QTimer m_ephemeralTimer;
    bool m_ephemeralMode = false;
    int m_ephemeralTimeoutSeconds = 60;

    bool m_recording = false;
    bool m_transcribing = false;
    int m_recordingSeconds = 0;
    double m_audioLevel = 0.0;
    QStringList m_availableWhisperModels;
    QStringList m_availableAiModels;
    bool m_smartFormatting = false;
    QString m_statusMessage;
    bool m_hudVisible = false;
};
