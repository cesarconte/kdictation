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
            "[00:02.400 --> 00:04.000] desde KDE Plasma [Música]\n"
            "[00:04.000 --> 00:06.000] con PipeWire [Música suave] (ruido)\n");
        QCOMPARE(DictationController::cleanWhisperOutput(raw),
                 QStringLiteral("Hola mundo desde KDE Plasma con PipeWire"));
    }

    void testApplyVoiceCommands()
    {
        const QString input = QStringLiteral("Hola mundo coma esto es una prueba punto y coma funciona muy bien signo de exclamación");
        const QString expected = QStringLiteral("Hola mundo, esto es una prueba; funciona muy bien!");
        QCOMPARE(DictationController::applyVoiceCommands(input), expected);

        const QString multiline = QStringLiteral("Primera línea nueva línea Segunda línea punto final");
        QCOMPARE(DictationController::applyVoiceCommands(multiline),
                 QStringLiteral("Primera línea\nSegunda línea."));

        // Comandos enriquecidos: comillas, interrogación, arroba y viñetas
        const QString enriched = QStringLiteral("abrir comillas superwhisper cerrar comillas abrir interrogación funciona cerrar interrogación contacto arroba test nueva viñeta elemento uno punto final");
        const QString expectedEnriched = QStringLiteral("\"Superwhisper\" ¿Funciona? Contacto@test\n• Elemento uno.");
        QCOMPARE(DictationController::applyVoiceCommands(enriched), expectedEnriched);
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

            // Test clearLastTranscript
            restored.clearLastTranscript();
            QCOMPARE(restored.lastTranscript(), QString());
            QCOMPARE(restored.history().size(), 1);

            // Test deleteHistoryEntry
            const QString remainingId = restored.history().first().toMap().value(QStringLiteral("id")).toString();
            QVERIFY(!remainingId.isEmpty());
            restored.deleteHistoryEntry(remainingId);
            QCOMPARE(restored.history().size(), 0);

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

    void testSavePcmToWavFileValidHeader()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString wavPath = tempDir.path() + QStringLiteral("/test.wav");
        QByteArray dummyPcm;
        // 16000 samples (1 second) of 16-bit mono PCM = 32000 bytes
        dummyPcm.resize(32000);
        dummyPcm.fill(0);

        const bool ok = PipeWireRecorder::savePcmToWavFile(wavPath, dummyPcm, 16000, 1);
        QVERIFY(ok);

        QFile f(wavPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.size(), 44 + 32000);

        const QByteArray header = f.read(44);
        QCOMPARE(header.mid(0, 4), QByteArray("RIFF"));
        QCOMPARE(header.mid(8, 4), QByteArray("WAVE"));
        QCOMPARE(header.mid(12, 4), QByteArray("fmt "));
        QCOMPARE(header.mid(36, 4), QByteArray("data"));
    }

    void testPipeWireRecorderRealCapture()
    {
        PipeWireRecorder recorder;
        QVERIFY(!recorder.isRecording());

        const bool started = recorder.startRecording(16000, 1);
        QVERIFY(started);
        QVERIFY(recorder.isRecording());

        // Wait 300 ms to capture real PipeWire buffers
        QTest::qWait(300);

        const QByteArray pcm = recorder.stopRecording();
        QVERIFY(!recorder.isRecording());
        // In 300ms at 16kHz 16-bit mono (32000 bytes/s), we should have received around 9600 bytes
        QVERIFY(pcm.size() > 0);
    }

    void testWhisperEngineModelDiscovery()
    {
        const QString model = WhisperEngine::findDefaultModel();
        QVERIFY(!model.isEmpty());
        QVERIFY(QFileInfo::exists(model));
    }

    void testWhisperEngineWarmModelTranscription()
    {
        WhisperEngine engine;
        const QString model = WhisperEngine::findDefaultModel();
        QVERIFY(!model.isEmpty());

        const bool loaded = engine.loadModel(model);
        QVERIFY(loaded);
        QVERIFY(engine.isModelLoaded());

        // 1.5 seconds of silence (24000 samples = 48000 bytes)
        QByteArray pcmSilence(48000, 0);
        const QString transcript = engine.transcribeSync(pcmSilence, QStringLiteral("es"));
        // Silence will either be empty or produce a bracketed tag like [BLANK_AUDIO] / [Música]
        const QString cleaned = DictationController::cleanWhisperOutput(transcript);
        QVERIFY(cleaned.isEmpty());
    }

    void testTextInjectorAvailability()
    {
        TextInjector injector;
        // On KDE Plasma 6 Wayland session, org_kde_kwin_fake_input is available
        const bool available = injector.isAvailable();
        if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
            QVERIFY(available);
        }
    }

    void testDirectInsertionToggleAndPersistence()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        {
            DictationController ctrl(tempDir.path());
            QCOMPARE(ctrl.directInsertion(), true); // Default is true (Superwhisper parity)
            ctrl.setDirectInsertion(false);
            QCOMPARE(ctrl.directInsertion(), false);
        }

        {
            DictationController restored(tempDir.path());
            QCOMPARE(restored.directInsertion(), false);
            restored.setDirectInsertion(true);
            QCOMPARE(restored.directInsertion(), true);
        }
    }

    void testPushToTalkToggleAndPersistence()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        {
            DictationController ctrl(tempDir.path());
            QCOMPARE(ctrl.pushToTalk(), false); // Default is false (toggle mode)
            ctrl.setPushToTalk(true);
            QCOMPARE(ctrl.pushToTalk(), true);
            QCOMPARE(ctrl.GetPushToTalk(), true);
        }

        {
            DictationController restored(tempDir.path());
            QCOMPARE(restored.pushToTalk(), true);
            restored.SetPushToTalk(false);
            QCOMPARE(restored.pushToTalk(), false);
            QCOMPARE(restored.GetPushToTalk(), false);
        }
    }

    void testRemoveFillerWords()
    {
        // Prueba de muletillas y repetición en español
        const QString spanishInput = QStringLiteral("bueno pues eh vamos vamos a probar este sistema de dictado");
        const QString expectedSpanish = QStringLiteral("Vamos a probar sistema de dictado");
        QCOMPARE(DictationController::removeFillerWords(spanishInput, QStringLiteral("es")), expectedSpanish);

        // Prueba en inglés
        const QString englishInput = QStringLiteral("hello uh this is is like a test");
        const QString expectedEnglish = QStringLiteral("Hello this is a test");
        QCOMPARE(DictationController::removeFillerWords(englishInput, QStringLiteral("en")), expectedEnglish);
    }

    void testAiModeAndModelPersistence()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        {
            DictationController ctrl(tempDir.path());
            QCOMPARE(ctrl.aiMode(), 0);
            QCOMPARE(ctrl.smartFormatting(), false);

            ctrl.setAiMode(1);
            QCOMPARE(ctrl.aiMode(), 1);
            QCOMPARE(ctrl.smartFormatting(), true);

            ctrl.setAiModel(QStringLiteral("llama3.2:3b"));
            QCOMPARE(ctrl.aiModel(), QStringLiteral("llama3.2:3b"));
        }

        {
            DictationController restored(tempDir.path());
            QCOMPARE(restored.aiMode(), 1);
            QCOMPARE(restored.smartFormatting(), true);
            QCOMPARE(restored.aiModel(), QStringLiteral("llama3.2:3b"));

            restored.setSmartFormatting(false);
            QCOMPARE(restored.aiMode(), 0);
            QCOMPARE(restored.smartFormatting(), false);
        }
    }

    void testWhisperModelDiscoveryAndSelection()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QStringList models = WhisperEngine::discoverModels();
        QVERIFY(!models.isEmpty());

        DictationController ctrl(tempDir.path());
        QVERIFY(!ctrl.availableWhisperModels().isEmpty());
        QVERIFY(!ctrl.whisperModel().isEmpty());

        // Ensure no duplicates exist in availableWhisperModels
        const QStringList modelsList = ctrl.availableWhisperModels();
        const QSet<QString> uniqueModels(modelsList.begin(), modelsList.end());
        QCOMPARE(modelsList.size(), uniqueModels.size());

        if (ctrl.availableWhisperModels().contains(QStringLiteral("ggml-small.bin"))) {
            ctrl.setWhisperModel(QStringLiteral("ggml-small.bin"));
            QCOMPARE(ctrl.whisperModel(), QStringLiteral("ggml-small.bin"));
        }
    }

    void testTranscribePcmDBus()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DictationController ctrl(tempDir.path());
        // Wait up to 1 second for background warm model loading
        for (int i = 0; i < 20 && !ctrl.IsEngineReady(); ++i) {
            QTest::qWait(50);
        }

        if (ctrl.IsEngineReady()) {
            // 1 second of silence
            QByteArray silencePcm(32000, 0);
            const QString result = ctrl.TranscribePcm(silencePcm, QStringLiteral("es"));
            // Silence should be cleaned to empty
            QVERIFY(result.isEmpty());
        }
    }

    void testCloseToBackgroundAndAutostart()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        {
            DictationController ctrl(tempDir.path());
            QCOMPARE(ctrl.closeToBackground(), true);
            ctrl.setCloseToBackground(false);
            QCOMPARE(ctrl.closeToBackground(), false);
        }

        {
            DictationController restored(tempDir.path());
            QCOMPARE(restored.closeToBackground(), false);
            restored.setCloseToBackground(true);
            QCOMPARE(restored.closeToBackground(), true);
        }
    }

    void testInjectTextAndEphemeralMode()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DictationController ctrl(tempDir.path());
        QCOMPARE(ctrl.isEphemeralMode(), false);
        ctrl.setEphemeralMode(true, 10);
        QCOMPARE(ctrl.isEphemeralMode(), true);

        // Injecting text updates history and last transcript
        ctrl.InjectText(QStringLiteral("Texto de prueba desde micrófono remoto"));
        QCOMPARE(ctrl.lastTranscript(), QStringLiteral("Texto de prueba desde micrófono remoto"));
        QVERIFY(!ctrl.history().isEmpty());
    }

    void testTranscribeAndInjectPcm()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        DictationController ctrl(tempDir.path());
        ctrl.setEphemeralMode(true, 60);

        for (int i = 0; i < 20 && !ctrl.IsEngineReady(); ++i) {
            QTest::qWait(50);
        }

        if (ctrl.IsEngineReady()) {
            QByteArray silencePcm(32000, 0);
            const QString result = ctrl.TranscribeAndInjectPcm(silencePcm, QStringLiteral("es"));
            QVERIFY(result.isEmpty());
        }
    }
};

QTEST_MAIN(TestKDictation)
#include "test_kdictation.moc"
