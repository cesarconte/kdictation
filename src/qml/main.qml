/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * KDictation — Dictado por voz con IA local.
 * Diseño Golden Gate / Liquid Glass (CachyOS.UI).
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import CachyOS.UI

CachyWindow {
    id: root

    title: CachyI18n.i18n("KDictation — Dictado por Voz IA")

    width: 480
    height: 320
    minimumWidth: 420
    minimumHeight: 300
    visible: Dictation ? Dictation.hudVisible : true

    showTrafficLights: true
    showTitleBarDivider: true
    showWindowFrame: true
    resizable: true
    cornerRadius: CachyTheme.radiusWindow
    contentPadded: true

    property string currentView: "dictation" // "dictation" | "history" | "settings"
    property bool expandedHistory: currentView === "history"
    onExpandedHistoryChanged: {
        if (expandedHistory && currentView !== "history") currentView = "history";
        if (!expandedHistory && currentView === "history") currentView = "dictation";
    }

    onClosing: (close) => {
        if (Dictation && Dictation.closeToBackground) {
            close.accepted = false;
            Dictation.hudVisible = false;
        }
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: {
            if (Dictation) {
                Dictation.quitApplication();
            } else {
                Qt.quit();
            }
        }
    }

    function formatTime(sec) {
        const m = Math.floor(sec / 60);
        const s = sec % 60;
        return (m < 10 ? "0" + m : "" + m) + ":" + (s < 10 ? "0" + s : "" + s);
    }

    headerCenter: [
        Item { width: 1; height: 1 }
    ]

    // =========================================================================
    // CABECERA — IZQUIERDA: acción principal y estado
    // =========================================================================
    headerLeading: [
        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Rectangle {
                id: recordButton
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 17
                color: (Dictation && Dictation.recording)
                    ? "#FF3B30"
                    : CachyTheme.withAlpha(CachyTheme.accentColor, 0.18)
                border.width: 1
                border.color: (Dictation && Dictation.recording)
                    ? "#FF3B30"
                    : CachyTheme.accentColor

                Behavior on color { ColorAnimation { duration: CachyTheme.animFast } }

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    source: (Dictation && Dictation.recording)
                        ? "media-playback-stop"
                        : "audio-input-microphone"
                    color: (Dictation && Dictation.recording) ? "#FFFFFF" : CachyTheme.accentColor
                }

                HoverHandler {
                    id: recordHover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: {
                        if (Dictation) {
                            Dictation.toggleRecording();
                        }
                    }
                }

                QQC2.ToolTip.visible: recordHover.hovered
                QQC2.ToolTip.text: (Dictation && Dictation.recording)
                    ? CachyI18n.i18n("Detener la grabación")
                    : CachyI18n.i18n("Empezar a dictar (Meta+Alt+D)")
                QQC2.ToolTip.delay: 400
            }

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: 190
                text: (Dictation && Dictation.recording)
                    ? CachyI18n.i18n("Dictando · ") + root.formatTime(Dictation.recordingSeconds)
                    : CachyI18n.i18n("KDictation IA")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: (Dictation && Dictation.recording) ? "#FF3B30" : CachyTheme.textPrimary
                elide: Text.ElideRight
            }
        }
    ]

    // =========================================================================
    // CABECERA — DERECHA: acciones secundarias
    // =========================================================================
    headerActions: [
        DictationHeaderActions {
            anchors.verticalCenter: parent.verticalCenter
            currentView: root.currentView
            onViewChangeRequested: function(viewName) {
                root.currentView = viewName;
            }
        }
    ]

    // =========================================================================
    // CONTENIDO
    // =========================================================================
    contentData: [
        ColumnLayout {
            anchors.fill: parent
            spacing: CachyTheme.spacingSmall

            // Vúmetro dinámico
            AudioLevelBar {}

            // Mensaje de estado
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: Dictation ? Dictation.statusMessage : ""
                font.pixelSize: 11
                color: CachyTheme.textMuted
                elide: Text.ElideRight
            }

            // Superficie central del contenido
            CachyCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0

                contentItem: Item {
                    // Transcripción actual
                    DictationView {
                        anchors.fill: parent
                        visible: root.currentView === "dictation"
                    }

                    // Historial
                    HistoryView {
                        anchors.fill: parent
                        visible: root.currentView === "history"
                    }

                    // Ajustes
                    SettingsView {
                        anchors.fill: parent
                        visible: root.currentView === "settings"
                        onCloseRequested: root.currentView = "dictation"
                    }
                }
            }
        }
    ]
}
