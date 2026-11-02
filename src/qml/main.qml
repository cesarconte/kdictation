/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    width: 440
    height: expandedHistory ? 320 : 160
    visible: Dictation ? Dictation.hudVisible : true
    title: i18n("KDictation — Dictado por Voz IA")

    property bool expandedHistory: false

    function formatTime(sec) {
        const m = Math.floor(sec / 60);
        const s = sec % 60;
        return (m < 10 ? "0" + m : "" + m) + ":" + (s < 10 ? "0" + s : "" + s);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.mediumSpacing
        spacing: 8

            // Top Pill Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                // Record / Stop Action Button
                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 19
                    color: (Dictation && Dictation.recording)
                        ? Kirigami.Theme.negativeTextColor
                        : Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.22)
                    border.width: 1
                    border.color: (Dictation && Dictation.recording)
                        ? Kirigami.Theme.negativeTextColor
                        : Kirigami.Theme.highlightColor

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: (Dictation && Dictation.recording)
                            ? "media-playback-stop"
                            : "audio-input-microphone"
                        color: (Dictation && Dictation.recording) ? "#ffffff" : Kirigami.Theme.highlightColor
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (Dictation) {
                                Dictation.toggleRecording();
                            }
                        }
                    }
                }

                // Status & Live Audio Level Bar
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        QQC2.Label {
                            text: (Dictation && Dictation.recording)
                                ? "Dictando · " + root.formatTime(Dictation.recordingSeconds)
                                : "KDictation IA"
                            font.bold: true
                            color: (Dictation && Dictation.recording) ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                        }

                        Item { Layout.fillWidth: true }

                        // Language selector pill
                        QQC2.Button {
                            text: Dictation ? Dictation.language.toUpperCase() : "ES"
                            flat: true
                            implicitHeight: 24
                            onClicked: {
                                if (!Dictation) return;
                                const langs = ["es", "en", "fr", "de"];
                                const idx = langs.indexOf(Dictation.language);
                                Dictation.language = langs[(idx + 1) % langs.length];
                            }
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: "Cambiar idioma de Whisper (ES/EN/FR/DE)"
                        }

                        // Smart AI punctuation toggle
                        QQC2.ToolButton {
                            icon.name: "tools-check-spelling"
                            checkable: true
                            checked: Dictation ? Dictation.smartFormatting : false
                            implicitWidth: 26
                            implicitHeight: 26
                            onToggled: {
                                if (Dictation) {
                                    Dictation.smartFormatting = checked;
                                }
                            }
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: "Puntuación inteligente con IA local (Ollama)"
                        }

                        QQC2.ToolButton {
                            icon.name: "view-history"
                            checkable: true
                            checked: root.expandedHistory
                            implicitWidth: 26
                            implicitHeight: 26
                            onToggled: root.expandedHistory = checked
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: "Mostrar historial de dictados"
                        }
                    }

                    // Live Audio Waveform / Level Bar
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.12)

                        Rectangle {
                            width: parent.width * (Dictation ? Math.max(0.04, Dictation.audioLevel) : 0.04)
                            height: parent.height
                            radius: 3
                            color: (Dictation && Dictation.recording) ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor
                            Behavior on width { NumberAnimation { duration: 90 } }
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: Dictation ? Dictation.statusMessage : ""
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }
            }

            // Recessed Last Transcript Box
            LiquidGlassCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                glassStyle: "recessed"
                radius: 12

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    QQC2.Label {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: (Dictation && Dictation.lastTranscript.length > 0)
                            ? Dictation.lastTranscript
                            : "Pulsa el micrófono o Meta+Alt+D para dictar. Comandos: «coma», «punto y aparte», «nueva línea»..."
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: (Dictation && Dictation.lastTranscript.length > 0)
                            ? Kirigami.Theme.textColor
                            : Kirigami.Theme.disabledTextColor
                    }

                    QQC2.ToolButton {
                        icon.name: "edit-copy"
                        enabled: Dictation && Dictation.lastTranscript.length > 0
                        onClicked: {
                            if (Dictation) {
                                Dictation.copyText(Dictation.lastTranscript);
                            }
                        }
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: "Copiar último dictado"
                    }
                }
            }

            // Expandable History Section
            LiquidGlassCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.expandedHistory
                glassStyle: "recessed"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: "Historial reciente"
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.bold: true
                            color: Kirigami.Theme.textColor
                        }
                        Item { Layout.fillWidth: true }
                        QQC2.ToolButton {
                            icon.name: "edit-clear"
                            implicitWidth: 24
                            implicitHeight: 24
                            onClicked: {
                                if (Dictation) {
                                    Dictation.clearHistory();
                                }
                            }
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: "Limpiar historial"
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: Dictation ? Dictation.history : []

                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 32
                            radius: 8
                            color: Kirigami.Theme.backgroundColor
                            border.width: 1
                            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.10)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 6
                                spacing: 6

                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: modelData.text || ""
                                    elide: Text.ElideRight
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    color: Kirigami.Theme.textColor
                                }

                                QQC2.Label {
                                    text: modelData.timestamp || ""
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize - 1
                                    color: Kirigami.Theme.disabledTextColor
                                }

                                QQC2.ToolButton {
                                    icon.name: "edit-copy"
                                    implicitWidth: 24
                                    implicitHeight: 24
                                    onClicked: {
                                        if (Dictation) {
                                            Dictation.copyText(modelData.text);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
}
