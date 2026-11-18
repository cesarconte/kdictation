/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

class VoicePostProcessor
{
public:
    static QString cleanWhisperOutput(const QString &raw);
    static QString applyVoiceCommands(const QString &text);
    static QString removeFillerWords(const QString &text, const QString &lang = QStringLiteral("es"));
    static double computeWavRmsLevel(const QByteArray &pcmS16Data);
};
