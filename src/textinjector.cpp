/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "textinjector.h"
#include "wayland-fake-input-client-protocol.h"

#include <linux/input-event-codes.h>
#include <wayland-client.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <cstring>

static void handleGlobal(void *data,
                          wl_registry *registry,
                          uint32_t id,
                          const char *interface,
                          uint32_t version)
{
    auto *self = static_cast<TextInjector *>(data);
    if (!self) {
        return;
    }

    if (std::strcmp(interface, "org_kde_kwin_fake_input") == 0) {
        const uint32_t bindVersion = version < 6 ? version : 6;
        auto *bound = static_cast<org_kde_kwin_fake_input *>(
            wl_registry_bind(registry, id, &org_kde_kwin_fake_input_interface, bindVersion));
        self->setFakeInput(bound);
    }
}

static void handleGlobalRemove(void *, wl_registry *, uint32_t)
{
}

static const struct wl_registry_listener s_registryListener = {
    handleGlobal,
    handleGlobalRemove
};

TextInjector::TextInjector(QObject *parent)
    : QObject(parent)
{
    ensureConnected();
}

TextInjector::~TextInjector()
{
    disconnectWayland();
}

void TextInjector::setFakeInput(org_kde_kwin_fake_input *fakeInput)
{
    m_fakeInput = fakeInput;
}

bool TextInjector::isAvailable() const
{
    QMutexLocker locker(&m_mutex);
    return m_fakeInput != nullptr && m_authenticated.load();
}

bool TextInjector::ensureConnected()
{
    QMutexLocker locker(&m_mutex);
    if (m_fakeInput && m_authenticated.load()) {
        return true;
    }

    if (!m_display) {
        m_display = wl_display_connect(nullptr);
        if (!m_display) {
            return false;
        }

        m_registry = wl_display_get_registry(m_display);
        if (!m_registry) {
            wl_display_disconnect(m_display);
            m_display = nullptr;
            return false;
        }

        wl_registry_add_listener(m_registry, &s_registryListener, this);
        wl_display_roundtrip(m_display);
    }

    if (m_fakeInput && !m_authenticated.load()) {
        org_kde_kwin_fake_input_authenticate(
            m_fakeInput,
            "KDictation",
            "Universal AI Voice Dictation Direct Text Insertion");
        wl_display_roundtrip(m_display);
        m_authenticated.store(true);
    }

    return m_fakeInput != nullptr && m_authenticated.load();
}

void TextInjector::disconnectWayland()
{
    QMutexLocker locker(&m_mutex);
    if (m_fakeInput) {
        org_kde_kwin_fake_input_destroy(m_fakeInput);
        m_fakeInput = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_flush(m_display);
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
    m_authenticated.store(false);
}

void TextInjector::copyToClipboard(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (auto *clip = QGuiApplication::clipboard()) {
        clip->setText(text, QClipboard::Clipboard);
        clip->setText(text, QClipboard::Selection);
    }

    // Sincronizar de forma no bloqueante con el portapapeles nativo de Wayland
    static const bool hasWlCopy = !QStandardPaths::findExecutable(QStringLiteral("wl-copy")).isEmpty();
    if (hasWlCopy && qEnvironmentVariableIsEmpty("QT_TEST_RUNNING")) {
        QProcess proc;
        proc.setProgram(QStringLiteral("wl-copy"));
        proc.setArguments({text});
        proc.setStandardInputFile(QProcess::nullDevice());
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        proc.startDetached();
    }
}

bool TextInjector::sendPasteKeystrokes()
{
    QMutexLocker locker(&m_mutex);
    if (!m_fakeInput || !m_display) {
        return false;
    }

    // Emulate Ctrl+V con pausas de baja latencia adaptadas al compositor KWin (10 ms)
    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_LEFTCTRL, 1);
    wl_display_flush(m_display);
    QThread::msleep(10);

    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_V, 1);
    wl_display_flush(m_display);
    QThread::msleep(15);

    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_V, 0);
    wl_display_flush(m_display);
    QThread::msleep(10);

    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_LEFTCTRL, 0);
    wl_display_flush(m_display);
    wl_display_roundtrip(m_display);

    return true;
}

bool TextInjector::sendTerminalPasteKeystrokes()
{
    QMutexLocker locker(&m_mutex);
    if (!m_fakeInput || !m_display) {
        return false;
    }

    // Emulate Shift+Insert
    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_LEFTSHIFT, 1);
    wl_display_flush(m_display);
    QThread::msleep(10);

    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_INSERT, 1);
    wl_display_flush(m_display);
    QThread::msleep(15);

    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_INSERT, 0);
    wl_display_flush(m_display);
    QThread::msleep(10);

    org_kde_kwin_fake_input_keyboard_key(m_fakeInput, KEY_LEFTSHIFT, 0);
    wl_display_flush(m_display);
    wl_display_roundtrip(m_display);

    return true;
}

bool TextInjector::sendKeysymSequence(const QString &text)
{
    QMutexLocker locker(&m_mutex);
    if (!m_fakeInput || !m_display) {
        return false;
    }

    for (const QChar ch : text) {
        const uint32_t ucs4 = ch.unicode();
        // X11 / Wayland keysym for Unicode code points: 0x01000000 | codepoint
        uint32_t keysym = ucs4;
        if (ucs4 == '\n') {
            keysym = 0xff0d; // Return
        } else if (ucs4 == '\t') {
            keysym = 0xff09; // Tab
        } else if (ucs4 > 0x7e) {
            keysym = 0x01000000 | ucs4;
        }

        org_kde_kwin_fake_input_keyboard_keysym(m_fakeInput, keysym, 1);
        wl_display_flush(m_display);
        QThread::msleep(10);
        org_kde_kwin_fake_input_keyboard_keysym(m_fakeInput, keysym, 0);
        wl_display_flush(m_display);
        QThread::msleep(10);
    }
    wl_display_roundtrip(m_display);
    return true;
}

bool TextInjector::injectText(const QString &text, InsertionMode mode)
{
    if (text.isEmpty()) {
        return false;
    }

    copyToClipboard(text);

    if (mode == InsertionMode::ClipboardOnly) {
        Q_EMIT insertionCompleted(text);
        return true;
    }

    // Small delay so that the active window regains full focus after HUD interaction
    QThread::msleep(35);

    bool ok = false;
    if (ensureConnected() && m_fakeInput) {
        if (mode == InsertionMode::AutoPaste) {
            ok = sendPasteKeystrokes();
        } else if (mode == InsertionMode::Typewriter) {
            ok = sendKeysymSequence(text);
        }
    }

    if (!ok) {
        // Fallback to wtype or ydotool if installed
        if (!QStandardPaths::findExecutable(QStringLiteral("wtype")).isEmpty()) {
            if (mode == InsertionMode::AutoPaste) {
                ok = (QProcess::execute(
                          QStringLiteral("wtype"),
                          {QStringLiteral("-M"), QStringLiteral("ctrl"), QStringLiteral("-k"), QStringLiteral("v"), QStringLiteral("-m"), QStringLiteral("ctrl")})
                      == 0);
            } else {
                ok = (QProcess::execute(QStringLiteral("wtype"), {text}) == 0);
            }
        } else if (!QStandardPaths::findExecutable(QStringLiteral("ydotool")).isEmpty()) {
            if (mode == InsertionMode::AutoPaste) {
                ok = (QProcess::execute(
                          QStringLiteral("ydotool"),
                          {QStringLiteral("key"), QStringLiteral("29:1"), QStringLiteral("47:1"), QStringLiteral("47:0"), QStringLiteral("29:0")})
                      == 0);
            } else {
                ok = (QProcess::execute(QStringLiteral("ydotool"), {QStringLiteral("type"), text}) == 0);
            }
        }
    }

    Q_EMIT insertionCompleted(text);
    return ok;
}
