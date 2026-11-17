/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later

    KDictation — Dictado por voz con IA local.

    Diseño Golden Gate / Liquid Glass (CachyOS.UI):
      * ventana `CachyWindow` con marco translúcido, cabecera de 44 px con el
        divisor duro de 1 px y radio de 20 px;
      * la acción principal (dictar) es el botón redondo de la cabecera, a la
        izquierda, junto al estado y al cronómetro;
      * las acciones secundarias (idioma, puntuación con IA, historial) viven
        en la cabecera derecha;
      * la transcripción se presenta sobre una `CachyCard`;
      * toda la paleta, los radios y los espaciados salen de `CachyTheme`.
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

    // Canonical proportion of the ecosystem (1.50): the HUD keeps a stable
    // shape and the history replaces the transcript instead of stretching it.
    width: 480
    height: 320
    minimumWidth: 420
    minimumHeight: 300
    visible: Dictation ? Dictation.hudVisible : true

    showTrafficLights: true
    showTitleBarDivider: true          // divisor duro de 1 px
    showWindowFrame: true              // Liquid Glass: blur KWin + rim especular
    resizable: true
    cornerRadius: CachyTheme.radiusWindow
    contentPadded: true                // margen perimetral de 18 px

    property bool expandedHistory: false

    function formatTime(sec) {
        const m = Math.floor(sec / 60);
        const s = sec % 60;
        return (m < 10 ? "0" + m : "" + m) + ":" + (s < 10 ? "0" + s : "" + s);
    }

    // The HUD identifies itself in the leading cluster, so the centred title
    // is left empty to give the room to the actions.
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

            // Botón de grabación / parada
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

            // Estado y cronómetro
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
        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            // Selector de idioma (ES/EN/FR/DE)
            CachyToolbarButton {
                text: Dictation ? Dictation.language.toUpperCase() : "ES"
                tooltipText: CachyI18n.i18n("Cambiar el idioma de Whisper (ES/EN/FR/DE)")
                onClicked: {
                    if (!Dictation) return;
                    const langs = ["es", "en", "fr", "de"];
                    const idx = langs.indexOf(Dictation.language);
                    Dictation.language = langs[(idx + 1) % langs.length];
                }
            }

            // Puntuación inteligente con IA local
            CachyToolbarButton {
                checkable: true
                checked: Dictation ? Dictation.smartFormatting : false
                iconName: "tools-check-spelling"
                tooltipText: CachyI18n.i18n("Puntuación inteligente con IA local (Ollama)")
                onToggled: (nextState) => {
                    if (Dictation) {
                        Dictation.smartFormatting = nextState;
                    }
                }
            }

            // Inserción directa en la ventana activa
            CachyToolbarButton {
                checkable: true
                checked: Dictation ? Dictation.directInsertion : true
                iconName: "edit-paste"
                tooltipText: (Dictation && Dictation.directInsertion)
                    ? CachyI18n.i18n("Inserción directa activa (escribe o pega en la ventana activa)")
                    : CachyI18n.i18n("Solo copiar al portapapeles")
                onToggled: (nextState) => {
                    if (Dictation) {
                        Dictation.directInsertion = nextState;
                    }
                }
            }

            // Modo de invocación: Push-to-Talk vs Toggle
            CachyToolbarButton {
                checkable: true
                checked: Dictation ? Dictation.pushToTalk : false
                iconName: (Dictation && Dictation.pushToTalk) ? "microphone-sensitivity-high" : "media-playback-start"
                tooltipText: (Dictation && Dictation.pushToTalk)
                    ? CachyI18n.i18n("Modo Push-to-Talk activo (mantén pulsado para hablar)")
                    : CachyI18n.i18n("Modo Toggle activo (pulsa para alternar dictado)")
                onToggled: (nextState) => {
                    if (Dictation) {
                        Dictation.pushToTalk = nextState;
                    }
                }
            }

            // Historial de dictados
            CachyToolbarButton {
                checkable: true
                checked: root.expandedHistory
                iconName: "view-history-symbolic"
                tooltipText: CachyI18n.i18n("Mostrar el historial de dictados")
                onToggled: (nextState) => root.expandedHistory = nextState
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

            // -----------------------------------------------------------------
            // Nivel de audio en vivo
            // -----------------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: 6
                radius: 3
                color: CachyTheme.withAlpha(CachyTheme.textPrimary, 0.10)

                Rectangle {
                    width: parent.width * (Dictation ? Math.max(0.04, Dictation.audioLevel) : 0.04)
                    height: parent.height
                    radius: 3
                    color: (Dictation && Dictation.recording) ? "#FF3B30" : CachyTheme.accentColor

                    Behavior on width {
                        NumberAnimation { duration: 90 }
                    }
                }
            }

            // -----------------------------------------------------------------
            // Mensaje de estado
            // -----------------------------------------------------------------
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: Dictation ? Dictation.statusMessage : ""
                font.pixelSize: 11
                color: CachyTheme.textMuted
                elide: Text.ElideRight
            }

            // -----------------------------------------------------------------
            // Superficie del contenido: transcripción actual o historial
            // -----------------------------------------------------------------
            CachyCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0

                contentItem: Item {

                    // --- Transcripción actual ---
                    RowLayout {
                        anchors.fill: parent
                        spacing: 8
                        visible: !root.expandedHistory

                        Text {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: (Dictation && Dictation.lastTranscript.length > 0)
                                ? Dictation.lastTranscript
                                : CachyI18n.i18n("Pulsa el micrófono o Meta+Alt+D para dictar. Comandos: «coma», «punto y aparte», «nueva línea»…")
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                            maximumLineCount: 4
                            elide: Text.ElideRight
                            font.pixelSize: 13
                            color: (Dictation && Dictation.lastTranscript.length > 0)
                                ? CachyTheme.textPrimary
                                : CachyTheme.textMuted
                        }

                        CachyToolbarButton {
                            iconName: "edit-copy"
                            tooltipText: CachyI18n.i18n("Copiar la última transcripción")
                            enabled: Dictation && Dictation.lastTranscript.length > 0
                            onClicked: {
                                if (Dictation) {
                                    Dictation.copyText(Dictation.lastTranscript);
                                }
                            }
                        }
                    }

                    // --- Historial ---
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6
                        visible: root.expandedHistory

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: CachyI18n.i18n("Historial reciente")
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: CachyTheme.textMuted
                                elide: Text.ElideRight
                            }

                            CachyToolbarButton {
                                iconName: "edit-clear"
                                tooltipText: CachyI18n.i18n("Limpiar el historial")
                                onClicked: {
                                    if (Dictation) {
                                        Dictation.clearHistory();
                                    }
                                }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumWidth: 0
                            clip: true
                            spacing: 2
                            model: Dictation ? Dictation.history : []

                            // Empty state
                            Text {
                                anchors.centerIn: parent
                                width: parent.width - 20
                                horizontalAlignment: Text.AlignHCenter
                                visible: Dictation ? Dictation.history.length === 0 : true
                                text: CachyI18n.i18n("Todavía no hay dictados guardados.")
                                font.pixelSize: 11
                                color: CachyTheme.textMuted
                                wrapMode: Text.WordWrap
                            }

                            delegate: Rectangle {
                                id: historyRow
                                required property var modelData

                                width: ListView.view ? ListView.view.width : 0
                                height: historyText.implicitHeight + 12
                                radius: CachyTheme.radiusSmall
                                color: historyHover.hovered ? CachyTheme.cardBackgroundHover : "transparent"

                                Behavior on color {
                                    ColorAnimation { duration: 120 }
                                }

                                HoverHandler {
                                    id: historyHover
                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    onTapped: {
                                        if (Dictation) {
                                            Dictation.copyText(historyRow.modelData.text);
                                        }
                                    }
                                }

                                Text {
                                    id: historyText
                                    x: 6
                                    y: 6
                                    width: Math.max(0, historyRow.width - 12 - 70 - 26)
                                    text: historyRow.modelData.text || ""
                                    font.pixelSize: 12
                                    color: CachyTheme.textPrimary
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    x: Math.max(0, historyRow.width - 70 - 26)
                                    y: 6
                                    width: 70
                                    horizontalAlignment: Text.AlignRight
                                    text: historyRow.modelData.timestamp || ""
                                    font.pixelSize: 10
                                    color: CachyTheme.textMuted
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    ]
}
