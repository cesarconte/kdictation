/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

ColumnLayout {
    id: root

    spacing: 6

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
