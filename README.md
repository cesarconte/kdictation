# KDictation — Universal AI Voice Dictation for KDE Plasma 6.8 + Wayland

**KDictation** brings voice dictation to **KDE Plasma 6.8 (Wayland)** with native Breeze design powered by **PipeWire (`pw-record`)**, **Whisper (`whisper-cli`)**, and optional local AI punctuation polishing via **Ollama (`qwen2.5:7b`)**.

## Features
- **Native KWin Breeze Window (`Kirigami.ApplicationWindow`)** with system titlebar buttons and live audio RMS meter.
- **Global Shortcut (`Meta+Alt+D`)** via `KGlobalAccel` and D-Bus control (`org.cesar.kdictation` `/Dictation` `Toggle()`).
- **Spoken Punctuation Commands**: `"coma"`, `"punto y coma"`, `"dos puntos"`, `"nueva línea"`, `"nuevo párrafo"`, `"signo de interrogación"`, `"signo de exclamación"`, `"punto final"`.
- **Automatic Wayland Clipboard Sync (`wl-copy`)** and recent dictation history.
