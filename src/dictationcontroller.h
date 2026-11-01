/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

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
    void Show();
    void Hide();

Q_SIGNALS:
    void recordingChanged();
    void transcribingChanged();
    void recordingSecondsChanged();
    void audioLevelChanged();
    void languageChanged();
    void smartFormattingChanged();
    void lastTranscriptChanged();
    void statusMessageChanged();
    void historyChanged();
    void hudVisibleChanged();

private:
    void loadState();
    void saveState() const;
    void updateLiveLevel();
    void runWhisperTranscription(const QString &wavPath);
    void runSmartFormattingPass(const QString &rawTranscript);
    void finalizeTranscript(const QString &finalText);
    static QString findWhisperBinary();
    static QString findWhisperModel();

    QString m_storageDir;
    QProcess *m_recordProcess = nullptr;
    QProcess *m_whisperProcess = nullptr;
    QTimer m_timer;
    QTimer m_levelTimer;
    QNetworkAccessManager m_nam;

    QString m_currentWavPath;
    bool m_recording = false;
    bool m_transcribing = false;
    int m_recordingSeconds = 0;
    double m_audioLevel = 0.0;
    QString m_language = QStringLiteral("es");
    bool m_smartFormatting = false;
    QString m_lastTranscript;
    QString m_statusMessage = QStringLiteral("Listo para dictar (Meta+Alt+D)");
    QVariantList m_history;
    bool m_hudVisible = true;
};
