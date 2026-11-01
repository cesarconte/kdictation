/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dictationcontroller.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtTest>

class TestKDictation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCleanWhisperOutput()
    {
        const QString raw = QStringLiteral(
            "[00:00.000 --> 00:02.400]  Hola mundo [BLANK_AUDIO]\n"
            "[00:02.400 --> 00:04.000] desde KDE Plasma [Música]\n");
        QCOMPARE(DictationController::cleanWhisperOutput(raw),
                 QStringLiteral("Hola mundo desde KDE Plasma"));
    }

    void testApplyVoiceCommands()
    {
        const QString input = QStringLiteral("Hola mundo coma esto es una prueba punto y coma funciona muy bien signo de exclamación");
        const QString expected = QStringLiteral("Hola mundo, esto es una prueba; funciona muy bien!");
        QCOMPARE(DictationController::applyVoiceCommands(input), expected);

        const QString multiline = QStringLiteral("Primera línea nueva línea Segunda línea punto final");
        QCOMPARE(DictationController::applyVoiceCommands(multiline),
                 QStringLiteral("Primera línea\nSegunda línea."));
    }

    void testComputeWavRmsLevel()
    {
        QByteArray silence(200, 0);
        QCOMPARE(DictationController::computeWavRmsLevel(silence), 0.0);

        QByteArray loud;
        const qint16 sample = 16000;
        for (int i = 0; i < 100; ++i) {
            loud.append(reinterpret_cast<const char *>(&sample), sizeof(qint16));
        }
        const double level = DictationController::computeWavRmsLevel(loud);
        QVERIFY(level > 0.5);
        QVERIFY(level <= 1.0);
    }

    void testHistoryAndPersistence()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        {
            DictationController ctrl(tempDir.path());
            ctrl.setLanguage(QStringLiteral("en"));
            ctrl.setSmartFormatting(true);
            ctrl.addHistoryEntry(QStringLiteral("Primer dictado de prueba"), QStringLiteral("es"));
            ctrl.addHistoryEntry(QStringLiteral("Segundo dictado"), QStringLiteral("en"));
            QCOMPARE(ctrl.history().size(), 2);
            QCOMPARE(ctrl.lastTranscript(), QStringLiteral("Segundo dictado"));
        }

        {
            DictationController restored(tempDir.path());
            QCOMPARE(restored.language(), QStringLiteral("en"));
            QCOMPARE(restored.smartFormatting(), true);
            QCOMPARE(restored.history().size(), 2);
            QCOMPARE(restored.lastTranscript(), QStringLiteral("Segundo dictado"));

            restored.clearHistory();
            QCOMPARE(restored.history().size(), 0);
        }
    }

    void testMainQmlCompilesWithoutErrors()
    {
        QTemporaryDir tempDir;
        DictationController ctrl(tempDir.path());

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Dictation"), &ctrl);

        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/org/cesar/kdictation/qml/main.qml")));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    }
};

QTEST_MAIN(TestKDictation)
#include "test_kdictation.moc"
