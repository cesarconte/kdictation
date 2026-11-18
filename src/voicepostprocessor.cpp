/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "voicepostprocessor.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>

QString VoicePostProcessor::cleanWhisperOutput(const QString &raw)
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

QString VoicePostProcessor::applyVoiceCommands(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return QString();
    }

    QString out = text;

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

QString VoicePostProcessor::removeFillerWords(const QString &text, const QString &lang)
{
    if (text.trimmed().isEmpty()) {
        return QString();
    }

    QString out = text;

    if (lang.startsWith(QLatin1String("es"), Qt::CaseInsensitive)) {
        // Tartamudeo
        static const QRegularExpression stutterRe(
            QStringLiteral(R"(\b([a-zA-ZáéíóúüñÁÉÍÓÚÜÑ]+)\s+\1\b)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(stutterRe, QStringLiteral("\\1"));

        // Muletillas y titubeos
        static const QRegularExpression hesitationRe(
            QStringLiteral(R"(\b(?:eh+|em+|este+|mmm+|ah+)\b)"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(hesitationRe, QStringLiteral(""));

        // Muletillas conversacionales
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

double VoicePostProcessor::computeWavRmsLevel(const QByteArray &pcmS16Data)
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
