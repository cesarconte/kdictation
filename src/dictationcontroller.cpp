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
#include <csignal>

DictationController::DictationController(const QString &storageDir, QObject *parent)
    : QObject(parent)
    , m_storageDir(storageDir.isEmpty()
                       ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       : storageDir)
{
    QDir().mkpath(m_storageDir);
    loadState();

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_recordingSeconds++;
        Q_EMIT recordingSecondsChanged();
    });

    m_levelTimer.setInterval(120);
    connect(&m_levelTimer, &QTimer::timeout, this, &DictationController::updateLiveLevel);
}

DictationController::~DictationController()
{
    if (m_recordProcess && m_recordProcess->state() != QProcess::NotRunning) {
        m_recordProcess->terminate();
        m_recordProcess->waitForFinished(1000);
    }
    if (m_whisperProcess && m_whisperProcess->state() != QProcess::NotRunning) {
        m_whisperProcess->kill();
        m_whisperProcess->waitForFinished(500);
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

bool DictationController::smartFormatting() const
{
    return m_smartFormatting;
}

void DictationController::setSmartFormatting(bool enabled)
{
    if (m_smartFormatting == enabled) {
        return;
    }
    m_smartFormatting = enabled;
    saveState();
    Q_EMIT smartFormattingChanged();
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
    Q_EMIT hudVisibleChanged();
}

QString DictationController::cleanWhisperOutput(const QString &raw)
{
    QStringList keptLines;
    static const QRegularExpression timestampRe(
        QStringLiteral(R"(^\s*\[[0-9:.]+\s*-->\s*[0-9:.]+\]\s*)"));
    static const QRegularExpression blankTagRe(
        QStringLiteral(R"(\[(BLANK_AUDIO|Música|Music|Silence|Silencio|inaudible)\])"),
        QRegularExpression::CaseInsensitiveOption);

    const QStringList lines = raw.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.remove(timestampRe);
        line.remove(blankTagRe);
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
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto y aparte\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(".\n\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bnuevo párrafo\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bnueva línea\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n")},
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto y coma\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("; ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bdos puntos\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(": ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bsigno de interrogación\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("? ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bsigno de exclamación\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("! ")},
        {QRegularExpression(QStringLiteral(R"(\s*\babrir interrogación\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" ¿")},
        {QRegularExpression(QStringLiteral(R"(\s*\bcerrar interrogación\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("? ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bcoma\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(", ")},
        {QRegularExpression(QStringLiteral(R"(\s*\bpunto final\b\s*)"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(".")},
    };

    for (const auto &rule : rules) {
        out.replace(rule.pattern, rule.replacement);
    }

    // Collapse multiple spaces around punctuation while preserving newlines
    static const QRegularExpression multiSpaceRe(QStringLiteral(R"([ \t]{2,})"));
    out.replace(multiSpaceRe, QStringLiteral(" "));
    static const QRegularExpression spaceBeforePunctRe(QStringLiteral(R"( +([,.;:?!]))"));
    out.replace(spaceBeforePunctRe, QStringLiteral("\\1"));

    return out.trimmed();
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
    m_currentWavPath = m_storageDir + QStringLiteral("/dictation_capture.wav");
    QFile::remove(m_currentWavPath);

    m_recordProcess = new QProcess(this);
    m_recordProcess->setProgram(QStringLiteral("pw-record"));
    m_recordProcess->setArguments({
        QStringLiteral("--format=s16"),
        QStringLiteral("--rate=16000"),
        QStringLiteral("--channels=1"),
        m_currentWavPath
    });

    m_recordProcess->start();
    if (!m_recordProcess->waitForStarted(2000)) {
        m_statusMessage = QStringLiteral("Error al iniciar pw-record");
        Q_EMIT statusMessageChanged();
        m_recordProcess->deleteLater();
        m_recordProcess = nullptr;
        return;
    }

    m_recording = true;
    m_recordingSeconds = 0;
    m_audioLevel = 0.08;
    m_statusMessage = QStringLiteral("Escuchando... habla ahora");
    m_timer.start();
    m_levelTimer.start();

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
    m_levelTimer.stop();
    m_recording = false;
    m_audioLevel = 0.0;
    Q_EMIT recordingChanged();
    Q_EMIT audioLevelChanged();

    if (m_recordProcess) {
        if (m_recordProcess->state() != QProcess::NotRunning) {
            ::kill(static_cast<pid_t>(m_recordProcess->processId()), SIGINT);
            if (!m_recordProcess->waitForFinished(1500)) {
                m_recordProcess->terminate();
                m_recordProcess->waitForFinished(1000);
            }
        }
        m_recordProcess->deleteLater();
        m_recordProcess = nullptr;
    }

    QFileInfo wavInfo(m_currentWavPath);
    if (!wavInfo.exists() || wavInfo.size() <= 512) {
        m_statusMessage = QStringLiteral("Audio demasiado corto");
        Q_EMIT statusMessageChanged();
        return;
    }

    runWhisperTranscription(m_currentWavPath);
}

void DictationController::copyText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (auto *clip = QGuiApplication::clipboard()) {
        clip->setText(text, QClipboard::Clipboard);
        clip->setText(text, QClipboard::Selection);
    }

    QProcess wlCopy;
    wlCopy.start(QStringLiteral("wl-copy"), {text});
    wlCopy.waitForFinished(1000);

    m_statusMessage = QStringLiteral("Copiado al portapapeles de Wayland");
    Q_EMIT statusMessageChanged();
}

void DictationController::clearHistory()
{
    m_history.clear();
    saveState();
    Q_EMIT historyChanged();
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
    toggleRecording();
}

void DictationController::StartDictation()
{
    startRecording();
}

void DictationController::StopDictation()
{
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
    m_smartFormatting = root.value(QStringLiteral("smartFormatting")).toBool(false);

    m_history.clear();
    const QJsonArray arr = root.value(QStringLiteral("history")).toArray();
    for (const QJsonValue &val : arr) {
        if (val.isObject()) {
            m_history.append(val.toObject().toVariantMap());
        }
    }
    if (!m_history.isEmpty()) {
        m_lastTranscript = m_history.first().toMap().value(QStringLiteral("text")).toString();
    }
}

void DictationController::saveState() const
{
    QJsonObject root;
    root.insert(QStringLiteral("language"), m_language);
    root.insert(QStringLiteral("smartFormatting"), m_smartFormatting);

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

void DictationController::updateLiveLevel()
{
    QFile wavFile(m_currentWavPath);
    if (!wavFile.open(QIODevice::ReadOnly)) {
        return;
    }
    const qint64 sz = wavFile.size();
    if (sz <= 44) {
        return;
    }
    const qint64 windowBytes = qMin<qint64>(3840, sz - 44);
    wavFile.seek(sz - windowBytes);
    const QByteArray pcmChunk = wavFile.read(windowBytes);
    m_audioLevel = computeWavRmsLevel(pcmChunk);
    Q_EMIT audioLevelChanged();
}

void DictationController::runWhisperTranscription(const QString &wavPath)
{
    const QString whisperBin = findWhisperBinary();
    const QString modelPath = findWhisperModel();

    if (whisperBin.isEmpty() || modelPath.isEmpty()) {
        m_statusMessage = QStringLiteral("Whisper o modelo ggml-base.bin no encontrado");
        Q_EMIT statusMessageChanged();
        return;
    }

    m_transcribing = true;
    m_statusMessage = QStringLiteral("Transcribiendo con Whisper IA...");
    Q_EMIT transcribingChanged();
    Q_EMIT statusMessageChanged();

    m_whisperProcess = new QProcess(this);
    m_whisperProcess->setProgram(whisperBin);
    m_whisperProcess->setArguments({
        QStringLiteral("-m"), modelPath,
        QStringLiteral("-l"), m_language,
        QStringLiteral("-nt"),
        QStringLiteral("-f"), wavPath
    });

    connect(m_whisperProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        const QString stdoutText = QString::fromUtf8(m_whisperProcess->readAllStandardOutput());
        m_whisperProcess->deleteLater();
        m_whisperProcess = nullptr;

        if (exitCode != 0) {
            m_transcribing = false;
            m_statusMessage = QStringLiteral("Error al ejecutar Whisper");
            Q_EMIT transcribingChanged();
            Q_EMIT statusMessageChanged();
            return;
        }

        const QString cleaned = applyVoiceCommands(cleanWhisperOutput(stdoutText));
        if (cleaned.isEmpty()) {
            m_transcribing = false;
            m_statusMessage = QStringLiteral("No se detectó voz en la grabación");
            Q_EMIT transcribingChanged();
            Q_EMIT statusMessageChanged();
            return;
        }

        if (m_smartFormatting) {
            runSmartFormattingPass(cleaned);
        } else {
            m_transcribing = false;
            Q_EMIT transcribingChanged();
            finalizeTranscript(cleaned);
        }
    });

    m_whisperProcess->start();
}

void DictationController::runSmartFormattingPass(const QString &rawTranscript)
{
    m_statusMessage = QStringLiteral("Puliendo puntuación con IA local...");
    Q_EMIT statusMessageChanged();

    QJsonObject payload;
    payload.insert(QStringLiteral("model"), QStringLiteral("qwen2.5:7b"));
    payload.insert(QStringLiteral("stream"), false);

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg.insert(QStringLiteral("role"), QStringLiteral("system"));
    sysMsg.insert(QStringLiteral("content"),
                  QStringLiteral("Eres un corrector de dictado por voz. Corrige únicamente la puntuación, mayúsculas y acentos del texto dictado sin cambiar las palabras ni añadir comentarios. Devuelve SOLO el texto corregido."));
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), rawTranscript);
    messages.append(userMsg);

    payload.insert(QStringLiteral("messages"), messages);

    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(8000);

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
        }
        reply->deleteLater();
        finalizeTranscript(result);
    });
}

void DictationController::finalizeTranscript(const QString &finalText)
{
    addHistoryEntry(finalText, m_language);
    copyText(finalText);
    m_statusMessage = QStringLiteral("Dictado copiado al portapapeles (Ctrl+V para pegar)");
    Q_EMIT statusMessageChanged();
}

QString DictationController::findWhisperBinary()
{
    const QString localBin = QDir::homePath() + QStringLiteral("/.local/bin/whisper-cli");
    if (QFileInfo::exists(localBin)) {
        return localBin;
    }
    return QStandardPaths::findExecutable(QStringLiteral("whisper-cli"));
}

QString DictationController::findWhisperModel()
{
    const QString kiriModel = QDir::homePath() + QStringLiteral("/.local/share/kiri/models/ggml-base.bin");
    if (QFileInfo::exists(kiriModel)) {
        return kiriModel;
    }
    return QString();
}
