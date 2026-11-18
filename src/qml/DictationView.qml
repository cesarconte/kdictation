/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

RowLayout {
    id: root

    spacing: 8

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
