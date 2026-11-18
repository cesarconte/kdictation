/*
 * SPDX-FileCopyrightText: 2026 Cesar
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

Rectangle {
    id: root

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.preferredHeight: 6
    radius: 3
    color: CachyTheme.withAlpha(CachyTheme.textPrimary, 0.10)
    clip: true

    Rectangle {
        id: levelIndicator
        height: parent.height
        radius: 3
        visible: Dictation ? (Dictation.recording || Dictation.transcribing) : false
        width: {
            if (!Dictation) return 0;
            if (Dictation.transcribing) return parent.width;
            if (Dictation.recording) return Math.max(6, parent.width * Math.min(1.0, Dictation.audioLevel));
            return 0;
        }
        color: (Dictation && Dictation.recording) ? "#FF3B30" : CachyTheme.accentColor

        Behavior on width {
            NumberAnimation { duration: 60 }
        }

        SequentialAnimation on opacity {
            running: Dictation ? Dictation.transcribing : false
            loops: Animation.Infinite
            NumberAnimation { from: 0.35; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 1.0; to: 0.35; duration: 400; easing.type: Easing.InOutQuad }
        }
    }
}
