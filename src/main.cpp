/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dictationcontroller.h"

#include <KGlobalAccel>

#include <QAction>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QGuiApplication>
#include <QKeySequence>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kdictation"));
    app.setApplicationDisplayName(QStringLiteral("KDictation"));
    app.setOrganizationDomain(QStringLiteral("cesar.org"));

    // Golden Gate parity: the application inherits the Plasma desktop style so
    // menus, tooltips and combo boxes match the rest of the CachyOS.UI
    // ecosystem instead of the raw Basic style.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Universal AI Voice Dictation for KDE Plasma Wayland"));
    parser.addHelpOption();
    QCommandLineOption toggleOpt({QStringLiteral("t"), QStringLiteral("toggle")},
                                 QStringLiteral("Toggle recording on a running instance or start recording immediately."));
    parser.addOption(toggleOpt);
    QCommandLineOption startOpt(QStringLiteral("start"),
                                QStringLiteral("Start recording dictation."));
    parser.addOption(startOpt);
    QCommandLineOption stopOpt(QStringLiteral("stop"),
                               QStringLiteral("Stop recording and transcribe."));
    parser.addOption(stopOpt);
    QCommandLineOption daemonOpt(QStringLiteral("daemon"),
                                 QStringLiteral("Run in background as a resident D-Bus voice service without showing the HUD"));
    parser.addOption(daemonOpt);
    QCommandLineOption ephemeralOpt(QStringLiteral("ephemeral"),
                                    QStringLiteral("Run ephemerally: exit automatically after inactivity timeout"));
    parser.addOption(ephemeralOpt);
    QCommandLineOption idleTimeoutOpt(QStringLiteral("idle-timeout"),
                                      QStringLiteral("Timeout in seconds of inactivity before exiting in ephemeral mode (default: 60)"),
                                      QStringLiteral("seconds"),
                                      QStringLiteral("60"));
    parser.addOption(idleTimeoutOpt);
    QCommandLineOption screenshotOpt(QStringLiteral("screenshot"),
                                     QStringLiteral("Capture window screenshot and exit"),
                                     QStringLiteral("path"));
    parser.addOption(screenshotOpt);
    parser.process(app);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.cesar.kdictation"))) {
        // Another instance is already running; forward command via D-Bus
        QDBusInterface iface(QStringLiteral("org.cesar.kdictation"),
                             QStringLiteral("/Dictation"),
                             QStringLiteral("org.cesar.kdictation.Dictation"),
                             bus);
        if (iface.isValid()) {
            if (parser.isSet(startOpt)) {
                iface.call(QStringLiteral("StartDictation"));
            } else if (parser.isSet(stopOpt)) {
                iface.call(QStringLiteral("StopDictation"));
            } else if (parser.isSet(daemonOpt)) {
                // Already running as daemon/instance
                return 0;
            } else {
                iface.call(QStringLiteral("Toggle"));
            }
            return 0;
        }
    }

    DictationController controller;
    bus.registerObject(QStringLiteral("/Dictation"),
                       &controller,
                       QDBusConnection::ExportAllContents);

    // Register global shortcut Meta+Alt+D via KGlobalAccel with both Toggle and Push-to-Talk support
    auto *toggleAction = new QAction(&app);
    toggleAction->setObjectName(QStringLiteral("toggle_dictation"));
    toggleAction->setText(QStringLiteral("Alternar Dictado por Voz (KDictation)"));
    KGlobalAccel::setGlobalShortcut(toggleAction, QKeySequence(QStringLiteral("Meta+Alt+D")));

    QObject::connect(toggleAction, &QAction::triggered, &controller, [&controller]() {
        if (!controller.pushToTalk()) {
            controller.toggleRecording();
        }
    });

    QObject::connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutActiveChanged,
                     &controller, [&controller, toggleAction](QAction *action, bool active) {
        if (action == toggleAction && controller.pushToTalk()) {
            if (active) {
                controller.startRecording();
            } else {
                controller.stopRecording();
            }
        }
    });

    QQmlApplicationEngine qmlEngine;
    // The CachyOS.UI design system (Liquid Glass) is installed system wide or
    // in the user prefix.
    qmlEngine.addImportPath(QStringLiteral("/home/cesar/.local/lib/qt6/qml"));
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("Dictation"), &controller);
    qmlEngine.load(QUrl(QStringLiteral("qrc:/org/cesar/kdictation/qml/main.qml")));

    if (qmlEngine.rootObjects().isEmpty()) {
        return -1;
    }

    if (parser.isSet(screenshotOpt)) {
        if (auto *win = qobject_cast<QQuickWindow *>(qmlEngine.rootObjects().constFirst())) {
            const QString outPath = parser.value(screenshotOpt);
            QTimer::singleShot(350, win, [win, outPath, &app]() {
                const QImage img = win->grabWindow();
                img.save(outPath);
                app.quit();
            });
        }
    }

    if (parser.isSet(daemonOpt)) {
        controller.setHudVisible(false);
    } else if (parser.isSet(toggleOpt)) {
        controller.startRecording();
    }

    if (parser.isSet(ephemeralOpt) || (parser.isSet(idleTimeoutOpt) && parser.isSet(daemonOpt))) {
        const int timeoutSec = parser.value(idleTimeoutOpt).toInt();
        controller.setEphemeralMode(true, timeoutSec > 0 ? timeoutSec : 60);
    }

    return app.exec();
}
