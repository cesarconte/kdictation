/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dictationcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>

#include <algorithm>

DictationController::DictationController(const QString &storageDir, QObject *parent)
    : QObject(parent)
    , m_storageDir(storageDir.isEmpty()
                       ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       : storageDir)
    , m_historyManager(this)
    , m_aiFormatter(this)
    , m_recorder(std::make_unique<PipeWireRecorder>(this))
    , m_whisperEngine(std::make_unique<WhisperEngine>(this))
    , m_injector(std::make_unique<TextInjector>(this))
{
    QDir().mkpath(m_storageDir);
    m_state = m_historyManager.loadState(m_storageDir);
    m_smartFormatting = (m_state.aiMode > 0);

    m_statusMessage = m_state.pushToTalk
        ? QStringLiteral("Modo Push-to-Talk (mantén Meta+Alt+D)")
        : QStringLiteral("Listo para dictar (Meta+Alt+D)");

    connect(&m_historyManager, &DictationHistoryManager::historyChanged,
            this, &DictationController::historyChanged);
    connect(&m_historyManager, &DictationHistoryManager::lastTranscriptChanged,
            this, &DictationController::lastTranscriptChanged);

    connect(&m_aiFormatter, &DictationAiFormatter::availableAiModelsDiscovered,
            this, [this](const QStringList &models) {
                m_availableAiModels = models;
                Q_EMIT availableAiModelsChanged();

                if (!m_availableAiModels.contains(m_state.aiModel)) {
                    if (m_availableAiModels.contains(QStringLiteral("llama3.2:3b"))) {
                        m_state.aiModel = QStringLiteral("llama3.2:3b");
                    } else if (m_availableAiModels.contains(QStringLiteral("qwen2.5:7b"))) {
                        m_state.aiModel = QStringLiteral("qwen2.5:7b");
                    } else if (!m_availableAiModels.isEmpty()) {
                        m_state.aiModel = m_availableAiModels.first();
                    }
                    saveState();
                    Q_EMIT aiModelChanged();
                }
            });

    connect(&m_aiFormatter, &DictationAiFormatter::formattingFinished,
            this, [this](const QString &formattedText) {
                m_transcribing = false;
                Q_EMIT transcribingChanged();
                finalizeTranscript(formattedText);
            });

    connect(&m_aiFormatter, &DictationAiFormatter::formattingFailed,
            this, [this](const QString &fallbackText, const QString &statusMsg) {
                m_transcribing = false;
                m_statusMessage = statusMsg;
                Q_EMIT transcribingChanged();
                Q_EMIT statusMessageChanged();
                finalizeTranscript(fallbackText);
            });

    m_aiFormatter.refreshAvailableAiModels();

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_recordingSeconds++;
        Q_EMIT recordingSecondsChanged();
    });

    m_ephemeralTimer.setSingleShot(true);
    connect(&m_ephemeralTimer, &QTimer::timeout, this, [this]() {
        if (m_ephemeralMode && !m_recording && !m_transcribing && !m_hudVisible) {
            qInfo() << "KDictation: Inactividad en modo efímero alcanzada (" << m_ephemeralTimeoutSeconds
                    << "s). Liberando recursos y saliendo.";
            QCoreApplication::quit();
        }
    });

    connect(m_recorder.get(), &PipeWireRecorder::audioLevelChanged, this, [this](double level) {
        if (m_recording) {
            m_audioLevel = level;
            Q_EMIT audioLevelChanged();
        }
    });

    connect(m_recorder.get(), &PipeWireRecorder::errorOccurred, this, [this](const QString &errMsg) {
        m_statusMessage = errMsg;
        Q_EMIT statusMessageChanged();
    });

    connect(m_whisperEngine.get(), &WhisperEngine::transcriptionReady, this, [this](const QString &rawTranscript) {
        QString cleaned = VoicePostProcessor::cleanWhisperOutput(rawTranscript);
        cleaned = VoicePostProcessor::applyVoiceCommands(cleaned);
        cleaned = VoicePostProcessor::removeFillerWords(cleaned, m_state.language);

        if (cleaned.isEmpty()) {
            m_transcribing = false;
            m_statusMessage = QStringLiteral("No se detectó voz en la grabación");
            Q_EMIT transcribingChanged();
            Q_EMIT statusMessageChanged();
            return;
        }

        if (m_state.aiMode > 0) {
            const QString model = m_state.aiModel.isEmpty() ? QStringLiteral("llama3.2:3b") : m_state.aiModel;
            m_statusMessage = QStringLiteral("Puliendo con IA local (%1)...").arg(model);
            Q_EMIT statusMessageChanged();
            m_aiFormatter.formatTranscript(cleaned, m_state.aiMode, m_state.aiModel);
        } else {
            m_transcribing = false;
            Q_EMIT transcribingChanged();
            finalizeTranscript(cleaned);
        }
    });

    connect(m_whisperEngine.get(), &WhisperEngine::transcriptionFailed, this, [this](const QString &errMsg) {
        m_transcribing = false;
        m_statusMessage = errMsg;
        Q_EMIT transcribingChanged();
        Q_EMIT statusMessageChanged();
    });

    // Descubrir modelos Whisper en disco (sin duplicados)
    const QStringList discovered = WhisperEngine::discoverModels();
    for (const QString &path : discovered) {
        const QString name = QFileInfo(path).fileName();
        if (!m_availableWhisperModels.contains(name)) {
            m_availableWhisperModels.append(name);
        }
    }

    // Precargar modelo Whisper en memoria en segundo plano (Warm Model)
    QString modelPathToLoad;
    if (!m_state.whisperModel.isEmpty()) {
        for (const QString &path : discovered) {
            if (QFileInfo(path).fileName() == m_state.whisperModel) {
                modelPathToLoad = path;
                break;
            }
        }
    }
    if (modelPathToLoad.isEmpty()) {
        modelPathToLoad = WhisperEngine::findDefaultModel();
        if (!modelPathToLoad.isEmpty()) {
            m_state.whisperModel = QFileInfo(modelPathToLoad).fileName();
        }
    }

    if (!modelPathToLoad.isEmpty()) {
        m_whisperEngine->loadModelAsync(modelPathToLoad);
    }
}

DictationController::~DictationController()
{
    if (m_recorder && m_recorder->isRecording()) {
        m_recorder->stopRecording();
    }
}

void DictationController::setLanguage(const QString &lang)
{
    const QString trimmed = lang.trimmed().isEmpty() ? QStringLiteral("es") : lang.trimmed();
    if (m_state.language == trimmed) {
        return;
    }
    m_state.language = trimmed;
    saveState();
    Q_EMIT languageChanged();
}

void DictationController::setWhisperModel(const QString &modelName)
{
    const QString trimmed = modelName.trimmed();
    if (m_state.whisperModel == trimmed || trimmed.isEmpty()) {
        return;
    }
    const QStringList discovered = WhisperEngine::discoverModels();
    for (const QString &path : discovered) {
        if (QFileInfo(path).fileName() == trimmed) {
            m_state.whisperModel = trimmed;
            m_whisperEngine->loadModelAsync(path);
            saveState();
            Q_EMIT whisperModelChanged();
            break;
        }
    }
}

void DictationController::setSmartFormatting(bool enabled)
{
    const int newMode = enabled ? (m_state.aiMode == 0 ? 1 : m_state.aiMode) : 0;
    setAiMode(newMode);
}

void DictationController::setAiMode(int mode)
{
    const int clamped = std::clamp(mode, 0, 2);
    if (m_state.aiMode == clamped) {
        return;
    }
    const bool prevSmart = (m_state.aiMode > 0);
    m_state.aiMode = clamped;
    m_smartFormatting = (m_state.aiMode > 0);
    saveState();
    Q_EMIT aiModeChanged();
    if (prevSmart != m_smartFormatting) {
        Q_EMIT smartFormattingChanged();
    }
}

void DictationController::setAiModel(const QString &model)
{
    const QString trimmed = model.trimmed();
    if (m_state.aiModel == trimmed || trimmed.isEmpty()) {
        return;
    }
    m_state.aiModel = trimmed;
    saveState();
    Q_EMIT aiModelChanged();
}

void DictationController::refreshAvailableAiModels()
{
    m_aiFormatter.refreshAvailableAiModels();
}

void DictationController::setDirectInsertion(bool enabled)
{
    if (m_state.directInsertion == enabled) {
        return;
    }
    m_state.directInsertion = enabled;
    saveState();
    Q_EMIT directInsertionChanged();
}

void DictationController::setPushToTalk(bool enabled)
{
    if (m_state.pushToTalk == enabled) {
        return;
    }
    m_state.pushToTalk = enabled;
    saveState();
    if (!m_recording && !m_transcribing) {
        m_statusMessage = m_state.pushToTalk
            ? QStringLiteral("Modo Push-to-Talk (mantén Meta+Alt+D)")
            : QStringLiteral("Listo para dictar (Meta+Alt+D)");
        Q_EMIT statusMessageChanged();
    }
    Q_EMIT pushToTalkChanged();
}

bool DictationController::autostart() const
{
    return DictationHistoryManager::isAutostartEnabled();
}

void DictationController::setAutostart(bool enabled)
{
    DictationHistoryManager::setAutostartEnabled(enabled);
    Q_EMIT autostartChanged();
}

void DictationController::setCloseToBackground(bool enabled)
{
    if (m_state.closeToBackground == enabled) {
        return;
    }
    m_state.closeToBackground = enabled;
    saveState();
    Q_EMIT closeToBackgroundChanged();
}

void DictationController::quitApplication()
{
    QGuiApplication::quit();
}

void DictationController::SetPushToTalk(bool enabled)
{
    setPushToTalk(enabled);
}

bool DictationController::GetPushToTalk() const
{
    return pushToTalk();
}

void DictationController::setHudVisible(bool visible)
{
    if (m_hudVisible == visible) {
        return;
    }
    m_hudVisible = visible;
    if (m_hudVisible) {
        m_ephemeralTimer.stop();
    } else if (m_ephemeralMode) {
        resetEphemeralTimer();
    }
    Q_EMIT hudVisibleChanged();
}

void DictationController::setEphemeralMode(bool enabled, int timeoutSeconds)
{
    m_ephemeralMode = enabled;
    m_ephemeralTimeoutSeconds = timeoutSeconds > 0 ? timeoutSeconds : 60;
    if (m_ephemeralMode && !m_hudVisible) {
        resetEphemeralTimer();
    } else {
        m_ephemeralTimer.stop();
    }
}

bool DictationController::isEphemeralMode() const
{
    return m_ephemeralMode;
}

void DictationController::resetEphemeralTimer()
{
    if (m_ephemeralMode && !m_hudVisible) {
        m_ephemeralTimer.start(m_ephemeralTimeoutSeconds * 1000);
    }
}

void DictationController::toggleRecording()
{
    if (m_recording) {
        stopRecording();
    } else {
        startRecording();
    }
}

void DictationController::startRecording()
{
    if (m_recording || m_transcribing) {
        return;
    }

    setHudVisible(true);

    if (!m_recorder || !m_recorder->startRecording(16000, 1)) {
        m_statusMessage = QStringLiteral("Error al iniciar captura con PipeWire nativo");
        Q_EMIT statusMessageChanged();
        return;
    }

    m_recording = true;
    m_recordingSeconds = 0;
    m_audioLevel = 0.05;
    m_statusMessage = QStringLiteral("Escuchando (PipeWire nativo)... habla ahora");
    m_timer.start();

    Q_EMIT recordingChanged();
    Q_EMIT recordingSecondsChanged();
    Q_EMIT audioLevelChanged();
    Q_EMIT statusMessageChanged();
}

void DictationController::stopRecording()
{
    if (!m_recording) {
        return;
    }

    m_timer.stop();
    m_recording = false;
    m_audioLevel = 0.0;
    Q_EMIT recordingChanged();
    Q_EMIT audioLevelChanged();

    const QByteArray pcmData = m_recorder ? m_recorder->stopRecording() : QByteArray();
    if (pcmData.size() < 6400) {
        m_statusMessage = QStringLiteral("Audio demasiado corto");
        Q_EMIT statusMessageChanged();
        return;
    }

    startWhisperTranscription(pcmData);
}

void DictationController::startWhisperTranscription(const QByteArray &pcmData)
{
    m_transcribing = true;
    m_statusMessage = QStringLiteral("Transcribiendo con libwhisper (Warm Model)...");
    Q_EMIT transcribingChanged();
    Q_EMIT statusMessageChanged();

    m_whisperEngine->transcribeAsync(pcmData, m_state.language);
}

void DictationController::copyText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    TextInjector::copyToClipboard(text);
    m_statusMessage = QStringLiteral("Copiado al portapapeles de Wayland");
    Q_EMIT statusMessageChanged();
}

void DictationController::clearHistory()
{
    m_historyManager.clearHistory(m_storageDir, m_state);
}

void DictationController::addHistoryEntry(const QString &text, const QString &lang)
{
    m_historyManager.addHistoryEntry(m_storageDir, m_state, text, lang);
}

void DictationController::Toggle()
{
    resetEphemeralTimer();
    toggleRecording();
}

void DictationController::StartDictation()
{
    resetEphemeralTimer();
    startRecording();
}

void DictationController::StopDictation()
{
    resetEphemeralTimer();
    stopRecording();
}

void DictationController::Show()
{
    setHudVisible(true);
}

void DictationController::Hide()
{
    if (m_recording) {
        stopRecording();
    }
    setHudVisible(false);
}

QString DictationController::TranscribePcm(const QByteArray &pcmData, const QString &lang)
{
    resetEphemeralTimer();
    if (!m_whisperEngine || !m_whisperEngine->isModelLoaded() || pcmData.isEmpty()) {
        return QString();
    }

    const QString language = lang.trimmed().isEmpty() ? m_state.language : lang.trimmed();
    const QString raw = m_whisperEngine->transcribeSync(pcmData, language);
    QString cleaned = VoicePostProcessor::cleanWhisperOutput(raw);
    cleaned = VoicePostProcessor::applyVoiceCommands(cleaned);
    cleaned = VoicePostProcessor::removeFillerWords(cleaned, language);
    return cleaned;
}

QString DictationController::TranscribeAndInjectPcm(const QByteArray &pcmData, const QString &lang)
{
    resetEphemeralTimer();
    const QString cleaned = TranscribePcm(pcmData, lang);
    if (cleaned.isEmpty()) {
        return QString();
    }

    if (m_state.aiMode > 0) {
        const QString model = m_state.aiModel.isEmpty() ? QStringLiteral("llama3.2:3b") : m_state.aiModel;
        m_statusMessage = QStringLiteral("Puliendo con IA local (%1)...").arg(model);
        Q_EMIT statusMessageChanged();
        m_aiFormatter.formatTranscript(cleaned, m_state.aiMode, m_state.aiModel);
    } else {
        finalizeTranscript(cleaned);
    }
    return cleaned;
}

void DictationController::InjectText(const QString &text)
{
    resetEphemeralTimer();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    finalizeTranscript(trimmed);
}

bool DictationController::IsEngineReady() const
{
    return m_whisperEngine && m_whisperEngine->isModelLoaded();
}

void DictationController::saveState() const
{
    const_cast<DictationHistoryManager &>(m_historyManager).saveState(m_storageDir, m_state);
}

void DictationController::finalizeTranscript(const QString &finalText)
{
    addHistoryEntry(finalText, m_state.language);

    if (m_state.directInsertion && m_injector) {
        m_injector->injectText(finalText, TextInjector::InsertionMode::AutoPaste);
        m_statusMessage = QStringLiteral("Dictado insertado directamente en la ventana activa");
    } else {
        if (m_injector) {
            m_injector->injectText(finalText, TextInjector::InsertionMode::ClipboardOnly);
        } else {
            TextInjector::copyToClipboard(finalText);
        }
        m_statusMessage = QStringLiteral("Dictado copiado al portapapeles (Ctrl+V para pegar)");
    }
    Q_EMIT statusMessageChanged();
}
