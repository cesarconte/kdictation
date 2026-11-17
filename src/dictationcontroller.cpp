/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dictationcontroller.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

#include <cmath>

DictationController::DictationController(const QString &storageDir, QObject *parent)
    : QObject(parent)
    , m_storageDir(storageDir.isEmpty()
                       ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       : storageDir)
    , m_recorder(std::make_unique<PipeWireRecorder>(this))
    , m_whisperEngine(std::make_unique<WhisperEngine>(this))
    , m_injector(std::make_unique<TextInjector>(this))
{
    QDir().mkpath(m_storageDir);
    loadState();
    refreshAvailableAiModels();

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
        QString cleaned = cleanWhisperOutput(rawTranscript);
        cleaned = applyVoiceCommands(cleaned);
        cleaned = removeFillerWords(cleaned, m_language);

        if (cleaned.isEmpty()) {
            m_transcribing = false;
            m_statusMessage = QStringLiteral("No se detectó voz en la grabación");
            Q_EMIT transcribingChanged();
            Q_EMIT statusMessageChanged();
            return;
        }

        if (m_aiMode > 0) {
            runSmartFormattingPass(cleaned);
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
    if (!m_whisperModel.isEmpty()) {
        for (const QString &path : discovered) {
            if (QFileInfo(path).fileName() == m_whisperModel) {
                modelPathToLoad = path;
                break;
            }
        }
    }
    if (modelPathToLoad.isEmpty()) {
        modelPathToLoad = WhisperEngine::findDefaultModel();
        if (!modelPathToLoad.isEmpty()) {
            m_whisperModel = QFileInfo(modelPathToLoad).fileName();
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

bool DictationController::recording() const
{
    return m_recording;
}

bool DictationController::transcribing() const
{
    return m_transcribing;
}

int DictationController::recordingSeconds() const
{
    return m_recordingSeconds;
}

double DictationController::audioLevel() const
{
    return m_audioLevel;
}

QString DictationController::language() const
{
    return m_language;
}

void DictationController::setLanguage(const QString &lang)
{
    const QString trimmed = lang.trimmed().isEmpty() ? QStringLiteral("es") : lang.trimmed();
    if (m_language == trimmed) {
        return;
    }
    m_language = trimmed;
    saveState();
    Q_EMIT languageChanged();
}

QString DictationController::whisperModel() const
{
    return m_whisperModel;
}

void DictationController::setWhisperModel(const QString &modelName)
{
    const QString trimmed = modelName.trimmed();
    if (m_whisperModel == trimmed || trimmed.isEmpty()) {
        return;
    }
    const QStringList discovered = WhisperEngine::discoverModels();
    for (const QString &path : discovered) {
        if (QFileInfo(path).fileName() == trimmed) {
            m_whisperModel = trimmed;
            m_whisperEngine->loadModelAsync(path);
            saveState();
            Q_EMIT whisperModelChanged();
            break;
        }
    }
}

QStringList DictationController::availableWhisperModels() const
{
    return m_availableWhisperModels;
}

bool DictationController::smartFormatting() const
{
    return m_aiMode > 0;
}

void DictationController::setSmartFormatting(bool enabled)
{
    const int newMode = enabled ? (m_aiMode == 0 ? 1 : m_aiMode) : 0;
    setAiMode(newMode);
}

int DictationController::aiMode() const
{
    return m_aiMode;
}

void DictationController::setAiMode(int mode)
{
    const int clamped = std::clamp(mode, 0, 2);
    if (m_aiMode == clamped) {
        return;
    }
    const bool prevSmart = (m_aiMode > 0);
    m_aiMode = clamped;
    m_smartFormatting = (m_aiMode > 0);
    saveState();
    Q_EMIT aiModeChanged();
    if (prevSmart != m_smartFormatting) {
        Q_EMIT smartFormattingChanged();
    }
}

QString DictationController::aiModel() const
{
    return m_aiModel;
}

void DictationController::setAiModel(const QString &model)
{
    const QString trimmed = model.trimmed();
    if (m_aiModel == trimmed || trimmed.isEmpty()) {
        return;
    }
    m_aiModel = trimmed;
    saveState();
    Q_EMIT aiModelChanged();
}

QStringList DictationController::availableAiModels() const
{
    return m_availableAiModels;
}

void DictationController::refreshAvailableAiModels()
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
                m_availableAiModels = modelNames;
                Q_EMIT availableAiModelsChanged();

                if (!m_availableAiModels.contains(m_aiModel)) {
                    // Prefer ultralight / fastest model
                    if (m_availableAiModels.contains(QStringLiteral("llama3.2:3b"))) {
                        m_aiModel = QStringLiteral("llama3.2:3b");
                    } else if (m_availableAiModels.contains(QStringLiteral("qwen2.5:7b"))) {
                        m_aiModel = QStringLiteral("qwen2.5:7b");
                    } else {
                        m_aiModel = m_availableAiModels.first();
                    }
                    Q_EMIT aiModelChanged();
                }
            }
        }
        reply->deleteLater();
    });
}

bool DictationController::directInsertion() const
{
    return m_directInsertion;
}

void DictationController::setDirectInsertion(bool enabled)
{
    if (m_directInsertion == enabled) {
        return;
    }
    m_directInsertion = enabled;
    saveState();
    Q_EMIT directInsertionChanged();
}

bool DictationController::pushToTalk() const
{
    return m_pushToTalk;
}

void DictationController::setPushToTalk(bool enabled)
{
    if (m_pushToTalk == enabled) {
        return;
    }
    m_pushToTalk = enabled;
    saveState();
    if (!m_recording && !m_transcribing) {
        m_statusMessage = m_pushToTalk
            ? QStringLiteral("Modo Push-to-Talk (mantén Meta+Alt+D)")
            : QStringLiteral("Listo para dictar (Meta+Alt+D)");
        Q_EMIT statusMessageChanged();
    }
    Q_EMIT pushToTalkChanged();
}

static QString autostartFilePath()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + QStringLiteral("/autostart/org.cesar.kdictation.desktop");
}

bool DictationController::autostart() const
{
    return QFileInfo::exists(autostartFilePath());
}

void DictationController::setAutostart(bool enabled)
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
    Q_EMIT autostartChanged();
}

bool DictationController::closeToBackground() const
{
    return m_closeToBackground;
}

void DictationController::setCloseToBackground(bool enabled)
{
    if (m_closeToBackground == enabled) {
        return;
    }
    m_closeToBackground = enabled;
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

QString DictationController::lastTranscript() const
{
    return m_lastTranscript;
}

QString DictationController::statusMessage() const
{
    return m_statusMessage;
}

QVariantList DictationController::history() const
{
    return m_history;
}

bool DictationController::hudVisible() const
{
    return m_hudVisible;
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

QString DictationController::cleanWhisperOutput(const QString &raw)
{
    QStringList keptLines;
    static const QRegularExpression timestampRe(
        QStringLiteral(R"(^\s*\[[0-9:.]+\s*-->\s*[0-9:.]+\]\s*)"));
    static const QRegularExpression bracketTagRe(
        QStringLiteral(R"(\[[^\]]*\])"));
    static const QRegularExpression parenTagRe(
        QStringLiteral(R"(\([^\)]*\))"));

    const QStringList lines = raw.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.remove(timestampRe);
        line.remove(bracketTagRe);
        line.remove(parenTagRe);
        line = line.trimmed();
        if (!line.isEmpty()) {
            keptLines.append(line);
        }
    }
    return keptLines.join(QLatin1Char(' ')).simplified();
}

QString DictationController::applyVoiceCommands(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return QString();
    }

    QString out = text;

    // Multi-word punctuation commands first
    struct Replacement {
        QRegularExpression pattern;
        QString replacement;
    };

    static const QVector<Replacement> rules = {
        // Párrafos y saltos
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto y aparte\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(".\n\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto y seguido\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(". ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bnuevo párrafo\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bnueva línea\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bsalto de línea\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n")},

        // Puntuación
        {QRegularExpression(QStringLiteral(R"(\s*\bpuntos suspensivos\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("... ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto y coma\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("; ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bdos puntos\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(": ")},

        // Interrogación y Exclamación
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:abrir|abre)\s+(?:signo de\s+)?interrogación\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" ¿")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:cerrar|cierra|signo de)\s+interrogación\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("? ")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:abrir|abre)\s+(?:signo de\s+)?(?:exclamación|admiración)\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" ¡")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:cerrar|cierra|signo de)\s+(?:exclamación|admiración)\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("! ")},

        // Comillas, Paréntesis y Corchetes
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:abrir|abre)\s+comillas\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" \"")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:cerrar|cierra)\s+comillas\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\" ")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:abrir|abre)\s+paréntesis\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" (")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:cerrar|cierra)\s+paréntesis\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(") ")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:abrir|abre)\s+corchetes\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" [")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:cerrar|cierra)\s+corchetes\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("] ")},

        // Símbolos y listas
        {QRegularExpression(QStringLiteral(R"(\s*\barroba\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("@")},
        {QRegularExpression(QStringLiteral(R"(\s*\bguio?n\s+medio\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" - ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bguio?n\s+bajo\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("_")},
        {QRegularExpression(QStringLiteral(R"(\s*\bbarra\s+(?:inclinada|diagonal)\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("/")},
        {QRegularExpression(QStringLiteral(R"(\s*\basterisco\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("*")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:por ciento|porcentaje)\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("%")},
        {QRegularExpression(QStringLiteral(R"(\s*\b(?:nueva\s+)?viñeta\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n• ")},

        // Coma y Punto final
        {QRegularExpression(QStringLiteral(R"(\s*\bcoma\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(", ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto final\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(".")},

        // English commands
        {QRegularExpression(QStringLiteral(R"(\s*\bnew paragraph\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bnew line\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bfull stop\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(". ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bperiod\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(". ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bsemicolon\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("; ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bcolon\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(": ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bquestion mark\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("? ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bexclamation mark\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("! ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bbullet point\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n• ")},
    };

    for (const auto &rule : rules) {
        out.replace(rule.pattern, rule.replacement);
    }

    // Collapse multiple spaces around punctuation while preserving newlines
    static const QRegularExpression multiSpaceRe(QStringLiteral(R"([ \t]{2,})"));
    out.replace(multiSpaceRe, QStringLiteral(" "));
    static const QRegularExpression spaceBeforePunctRe(QStringLiteral(R"( +([,.;:?!]))"));
    out.replace(spaceBeforePunctRe, QStringLiteral("\\1"));

    // Autocapitalize after sentence terminators (., ?, !, \n), lists, and opening marks
    static const QRegularExpression autoCapRe(
        QStringLiteral(R"((?:^|[.!?\n•¿¡]\s*)[¿¡"'«\s]*([a-záéíóúüñ]))"));
    auto matchIt = autoCapRe.globalMatch(out);
    while (matchIt.hasNext()) {
        auto match = matchIt.next();
        int charPos = match.capturedStart(1);
        if (charPos >= 0 && charPos < out.length()) {
            out[charPos] = out[charPos].toUpper();
        }
    }

    return out.trimmed();
}

QString DictationController::removeFillerWords(const QString &text, const QString &lang)
{
    if (text.trimmed().isEmpty()) {
        return QString();
    }

    QString out = text;

    if (lang.startsWith(QLatin1String("es"), Qt::CaseInsensitive)) {
        // 1. Eliminar repetición accidental de palabras contiguas (tartamudeo: "el el", "vamos vamos")
        static const QRegularExpression stutterRe(
            QStringLiteral(R"(\b([a-zA-ZáéíóúüñÁÉÍÓÚÜÑ]+)\s+\1\b)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(stutterRe, QStringLiteral("\\1"));

        // 2. Muletillas y sonidos de titubeo (eh, em, este, mmm, ah)
        static const QRegularExpression hesitationRe(
            QStringLiteral(R"(\b(?:eh+|em+|este+|mmm+|ah+)\b)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(hesitationRe, QStringLiteral(""));

        // 3. Muletillas conversacionales al inicio de frase o encadenadas
        static const QRegularExpression starterFillerRe(
            QStringLiteral(R"(^(?:\s*(?:bueno|pues|o sea|en plan|digamos que)[,\s]*)+)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(starterFillerRe, QStringLiteral(""));

        static const QRegularExpression inlineFillerRe(
            QStringLiteral(R"((?:,\s*|\s+)(?:o sea|en plan)(?:,\s*|\s+))"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(inlineFillerRe, QStringLiteral(", "));
    } else if (lang.startsWith(QLatin1String("en"), Qt::CaseInsensitive)) {
        static const QRegularExpression stutterRe(
            QStringLiteral(R"(\b([a-zA-Z]+)\s+\1\b)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(stutterRe, QStringLiteral("\\1"));

        static const QRegularExpression hesitationRe(
            QStringLiteral(R"(\b(?:uh+|um+|er+|ah+|like,?\s+|you know,?\s+)\b)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(hesitationRe, QStringLiteral(""));
    }

    // Normalizar signos y espacios residuales
    static const QRegularExpression orphanLeadingPunctRe(QStringLiteral(R"(^[,\s;:.-]+)"));
    out.remove(orphanLeadingPunctRe);
    static const QRegularExpression multiSpaceRe(QStringLiteral(R"([ \t]{2,})"));
    out.replace(multiSpaceRe, QStringLiteral(" "));
    static const QRegularExpression spaceBeforePunctRe(QStringLiteral(R"( +([,.;:?!]))"));
    out.replace(spaceBeforePunctRe, QStringLiteral("\\1"));
    static const QRegularExpression multiCommaRe(QStringLiteral(R"(,{2,})"));
    out.replace(multiCommaRe, QStringLiteral(","));

    out = out.trimmed();
    if (!out.isEmpty()) {
        out[0] = out[0].toUpper();
    }

    return out;
}

double DictationController::computeWavRmsLevel(const QByteArray &pcmS16Data)
{
    const int sampleCount = pcmS16Data.size() / 2;
    if (sampleCount <= 0) {
        return 0.0;
    }

    const auto *samples = reinterpret_cast<const qint16 *>(pcmS16Data.constData());
    double sumSquares = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const double norm = static_cast<double>(samples[i]) / 32768.0;
        sumSquares += norm * norm;
    }
    const double rms = std::sqrt(sumSquares / static_cast<double>(sampleCount));
    return std::clamp(rms * 3.8, 0.0, 1.0);
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
    // Requerir al menos 200 ms de audio (16000 muestras * 2 bytes * 0.2s = 6400 bytes)
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

    m_whisperEngine->transcribeAsync(pcmData, m_language);
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
    m_history.clear();
    m_lastTranscript.clear();
    saveState();
    Q_EMIT historyChanged();
    Q_EMIT lastTranscriptChanged();
}

void DictationController::addHistoryEntry(const QString &text, const QString &lang)
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
    saveState();

    Q_EMIT lastTranscriptChanged();
    Q_EMIT historyChanged();
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

    const QString language = lang.trimmed().isEmpty() ? m_language : lang.trimmed();
    const QString raw = m_whisperEngine->transcribeSync(pcmData, language);
    QString cleaned = cleanWhisperOutput(raw);
    cleaned = applyVoiceCommands(cleaned);
    cleaned = removeFillerWords(cleaned, language);
    return cleaned;
}

QString DictationController::TranscribeAndInjectPcm(const QByteArray &pcmData, const QString &lang)
{
    resetEphemeralTimer();
    const QString cleaned = TranscribePcm(pcmData, lang);
    if (cleaned.isEmpty()) {
        return QString();
    }

    if (m_aiMode > 0) {
        runSmartFormattingPass(cleaned);
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

void DictationController::loadState()
{
    QFile file(m_storageDir + QStringLiteral("/history.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject root = doc.object();
    m_language = root.value(QStringLiteral("language")).toString(QStringLiteral("es"));
    m_whisperModel = root.value(QStringLiteral("whisperModel")).toString();
    m_aiMode = root.contains(QStringLiteral("aiMode"))
        ? root.value(QStringLiteral("aiMode")).toInt(0)
        : (root.value(QStringLiteral("smartFormatting")).toBool(false) ? 1 : 0);
    m_aiModel = root.value(QStringLiteral("aiModel")).toString(QStringLiteral("llama3.2:3b"));
    m_smartFormatting = (m_aiMode > 0);
    m_directInsertion = root.value(QStringLiteral("directInsertion")).toBool(true);
    m_pushToTalk = root.value(QStringLiteral("pushToTalk")).toBool(false);
    m_closeToBackground = root.value(QStringLiteral("closeToBackground")).toBool(true);

    m_statusMessage = m_pushToTalk
        ? QStringLiteral("Modo Push-to-Talk (mantén Meta+Alt+D)")
        : QStringLiteral("Listo para dictar (Meta+Alt+D)");

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
}

void DictationController::saveState() const
{
    QJsonObject root;
    root.insert(QStringLiteral("language"), m_language);
    root.insert(QStringLiteral("whisperModel"), m_whisperModel);
    root.insert(QStringLiteral("smartFormatting"), m_smartFormatting);
    root.insert(QStringLiteral("aiMode"), m_aiMode);
    root.insert(QStringLiteral("aiModel"), m_aiModel);
    root.insert(QStringLiteral("directInsertion"), m_directInsertion);
    root.insert(QStringLiteral("pushToTalk"), m_pushToTalk);
    root.insert(QStringLiteral("closeToBackground"), m_closeToBackground);

    QJsonArray arr;
    for (const QVariant &item : m_history) {
        arr.append(QJsonObject::fromVariantMap(item.toMap()));
    }
    root.insert(QStringLiteral("history"), arr);

    QFile file(m_storageDir + QStringLiteral("/history.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void DictationController::runSmartFormattingPass(const QString &rawTranscript)
{
    const QString model = m_aiModel.isEmpty() ? QStringLiteral("llama3.2:3b") : m_aiModel;
    m_statusMessage = QStringLiteral("Puliendo con IA local (%1)...").arg(model);
    Q_EMIT statusMessageChanged();

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
    if (m_aiMode == 2) {
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
        m_transcribing = false;
        Q_EMIT transcribingChanged();

        QString result = rawTranscript;
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
            }
        } else {
            m_statusMessage = QStringLiteral("Listo (IA no disponible, usando modo local)");
            Q_EMIT statusMessageChanged();
        }
        reply->deleteLater();
        finalizeTranscript(result);
    });
}

void DictationController::finalizeTranscript(const QString &finalText)
{
    addHistoryEntry(finalText, m_language);

    if (m_directInsertion && m_injector) {
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
