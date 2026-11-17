/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QMutex>
#include <QObject>
#include <QString>

#include <atomic>

struct wl_display;
struct wl_registry;
struct org_kde_kwin_fake_input;

class TextInjector : public QObject
{
    Q_OBJECT

public:
    enum class InsertionMode {
        AutoPaste,     // Recommended: copies to clipboard and sends Ctrl+V directly to active window
        Typewriter,    // Direct typing character by character via keysyms
        ClipboardOnly  // Only copy to clipboard without key emulation
    };
    Q_ENUM(InsertionMode)

    explicit TextInjector(QObject *parent = nullptr);
    ~TextInjector() override;

    bool isAvailable() const;
    void setFakeInput(org_kde_kwin_fake_input *fakeInput);
    bool injectText(const QString &text, InsertionMode mode = InsertionMode::AutoPaste);

    bool sendPasteKeystrokes();
    bool sendTerminalPasteKeystrokes();
    bool sendKeysymSequence(const QString &text);

    static void copyToClipboard(const QString &text);

Q_SIGNALS:
    void insertionCompleted(const QString &text);
    void insertionFailed(const QString &reason);

private:
    bool ensureConnected();
    void disconnectWayland();

    mutable QMutex m_mutex;
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    org_kde_kwin_fake_input *m_fakeInput = nullptr;
    std::atomic<bool> m_authenticated{false};
};
