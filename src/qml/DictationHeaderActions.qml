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

    required property string currentView
    signal viewChangeRequested(string viewName)

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

    // Puntuación y refinamiento inteligente con IA local (Ollama)
    CachyToolbarButton {
        checkable: true
        checked: Dictation ? (Dictation.aiMode > 0) : false
        iconName: "tools-check-spelling"
        tooltipText: {
            if (!Dictation || Dictation.aiMode === 0) {
                return CachyI18n.i18n("Refinamiento IA desactivado (modo rápido local). Clic para activar");
            }
            if (Dictation.aiMode === 1) {
                return CachyI18n.i18n("Modo Puliendo activo (") + Dictation.aiModel + CachyI18n.i18n(": elimina muletillas y ortografía). Clic para modo Estructurado");
            }
            return CachyI18n.i18n("Modo Estructurado activo (") + Dictation.aiModel + CachyI18n.i18n(": redacción profesional). Clic para desactivar");
        }
        onClicked: {
            if (Dictation) {
                Dictation.aiMode = (Dictation.aiMode + 1) % 3;
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
        checked: root.currentView === "history"
        iconName: "view-history-symbolic"
        tooltipText: CachyI18n.i18n("Mostrar el historial de dictados")
        onToggled: (nextState) => root.viewChangeRequested(nextState ? "history" : "dictation")
    }

    // Ajustes y Configuración
    CachyToolbarButton {
        checkable: true
        checked: root.currentView === "settings"
        iconName: "settings-configure"
        tooltipText: CachyI18n.i18n("Ajustes y configuración")
        onToggled: (nextState) => root.viewChangeRequested(nextState ? "settings" : "dictation")
    }
}
