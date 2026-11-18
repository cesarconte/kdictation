/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>

class DictationAiFormatter : public QObject
{
    Q_OBJECT

public:
    explicit DictationAiFormatter(QObject *parent = nullptr);
    ~DictationAiFormatter() override = default;

    void refreshAvailableAiModels();
    void formatTranscript(const QString &rawTranscript, int aiMode, const QString &aiModel);

Q_SIGNALS:
    void availableAiModelsDiscovered(const QStringList &models);
    void formattingFinished(const QString &formattedText);
    void formattingFailed(const QString &fallbackText, const QString &statusMsg);

private:
    QNetworkAccessManager m_nam;
};
