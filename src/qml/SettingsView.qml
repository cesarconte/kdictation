/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import CachyOS.UI

ColumnLayout {
    id: root

    signal closeRequested()

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 6

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: CachyI18n.i18n("Ajustes de KDictation")
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: CachyTheme.textMuted
            elide: Text.ElideRight
        }

        CachyToolbarButton {
            iconName: "window-close"
            tooltipText: CachyI18n.i18n("Cerrar ajustes")
            onClicked: root.closeRequested()
        }
    }

    Flickable {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: 0
        contentWidth: width
        contentHeight: settingsCol.implicitHeight
        clip: true

        ColumnLayout {
            id: settingsCol
            width: parent.width
            spacing: 8

            // 1. Motor Whisper
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: CachyI18n.i18n("Modelo Whisper local")
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: CachyTheme.textPrimary
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 6

                    QQC2.ComboBox {
                        id: whisperCombo
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: Dictation ? Dictation.availableWhisperModels : []
                        currentIndex: {
                            if (!Dictation) return -1;
                            return model.indexOf(Dictation.whisperModel);
                        }
                        onActivated: (index) => {
                            if (Dictation) {
                                Dictation.whisperModel = model[index];
                            }
                        }
                    }

                    Text {
                        text: CachyI18n.i18n("En RAM")
                        font.pixelSize: 10
                        color: "#34C759"
                    }
                }
            }

            // 2. Refinamiento IA (Ollama)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: CachyI18n.i18n("Modelo IA local (Ollama)")
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: CachyTheme.textPrimary
                    elide: Text.ElideRight
                }

                QQC2.ComboBox {
                    id: aiCombo
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    model: Dictation ? Dictation.availableAiModels : []
                    currentIndex: {
                        if (!Dictation) return -1;
                        return model.indexOf(Dictation.aiModel);
                    }
                    onActivated: (index) => {
                        if (Dictation) {
                            Dictation.aiModel = model[index];
                        }
                    }
                }
            }

            // 3. Opciones de inserción, segundo plano y modo
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                QQC2.CheckBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: CachyI18n.i18n("Inserción directa en ventana activa")
                    checked: Dictation ? Dictation.directInsertion : true
                    onToggled: {
                        if (Dictation) Dictation.directInsertion = checked;
                    }
                }

                QQC2.CheckBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: CachyI18n.i18n("Modo Push-to-Talk (mantener pulsado)")
                    checked: Dictation ? Dictation.pushToTalk : false
                    onToggled: {
                        if (Dictation) Dictation.pushToTalk = checked;
                    }
                }

                QQC2.CheckBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: CachyI18n.i18n("Iniciar con el sistema en segundo plano (Modo Demonio)")
                    checked: Dictation ? Dictation.autostart : false
                    onToggled: {
                        if (Dictation) Dictation.autostart = checked;
                    }
                }

                QQC2.CheckBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: CachyI18n.i18n("Permanecer en segundo plano al cerrar la ventana")
                    checked: Dictation ? Dictation.closeToBackground : true
                    onToggled: {
                        if (Dictation) Dictation.closeToBackground = checked;
                    }
                }
            }

            // 4. Recordatorio de atajo y botón de salida
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 6

                Text {
                    text: CachyI18n.i18n("Atajo:")
                    font.pixelSize: 10
                    color: CachyTheme.textMuted
                }

                Rectangle {
                    radius: 4
                    color: CachyTheme.withAlpha(CachyTheme.textPrimary, 0.08)
                    implicitWidth: shortcutLabel.implicitWidth + 12
                    implicitHeight: 20

                    Text {
                        id: shortcutLabel
                        anchors.centerIn: parent
                        text: "Meta + Alt + D"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: CachyTheme.accentColor
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                CachyButton {
                    destructive: true
                    text: CachyI18n.i18n("Salir")
                    onClicked: {
                        if (Dictation) {
                            Dictation.quitApplication();
                        } else {
                            Qt.quit();
                        }
                    }
                }
            }
        }
    }
}
