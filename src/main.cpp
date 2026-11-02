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
#include <QQuickWindow>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kdictation"));
    app.setApplicationDisplayName(QStringLiteral("KDictation"));
    app.setOrganizationDomain(QStringLiteral("cesar.org"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Universal AI Voice Dictation for KDE Plasma Wayland"));
    parser.addHelpOption();
    QCommandLineOption toggleOpt({QStringLiteral("t"), QStringLiteral("toggle")},
                                 QStringLiteral("Toggle recording on a running instance or start recording immediately."));
    parser.addOption(toggleOpt);
    parser.process(app);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.cesar.kdictation"))) {
        // Another instance is already running; forward toggle command via D-Bus
        QDBusInterface iface(QStringLiteral("org.cesar.kdictation"),
                             QStringLiteral("/Dictation"),
                             QStringLiteral("org.cesar.kdictation.Dictation"),
                             bus);
        if (iface.isValid()) {
            iface.call(QStringLiteral("Toggle"));
            return 0;
        }
    }

    DictationController controller;
    bus.registerObject(QStringLiteral("/Dictation"),
                       &controller,
                       QDBusConnection::ExportAllSlots);

    // Register global shortcut Meta+Alt+D via KGlobalAccel
    auto *toggleAction = new QAction(&app);
    toggleAction->setObjectName(QStringLiteral("toggle_dictation"));
    toggleAction->setText(QStringLiteral("Alternar Dictado por Voz (KDictation)"));
    KGlobalAccel::setGlobalShortcut(toggleAction, QKeySequence(QStringLiteral("Meta+Alt+D")));
    QObject::connect(toggleAction, &QAction::triggered, &controller, &DictationController::toggleRecording);

    QQmlApplicationEngine qmlEngine;
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("Dictation"), &controller);
    qmlEngine.load(QUrl(QStringLiteral("qrc:/org/cesar/kdictation/qml/main.qml")));

    if (qmlEngine.rootObjects().isEmpty()) {
        return -1;
    }

    if (parser.isSet(toggleOpt)) {
        controller.startRecording();
    }

    return app.exec();
}
