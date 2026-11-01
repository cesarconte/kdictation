# KDictation — Universal AI Voice Dictation for KDE Plasma 6.8 + Wayland

Inspired by **macOS Voice Dictation**, **KDictation** brings a floating **Liquid Glass** voice dictation HUD to **KDE Plasma 6.8 (Wayland)** powered by **PipeWire (`pw-record`)**, **Whisper (`whisper-cli`)**, and optional local AI punctuation polishing via **Ollama (`qwen2.5:7b`)**.

## Features
- **Wayland Layer-Shell Floating Pill (`LayerShellQt`)** with multi-layer **Liquid Glass** aesthetic and live audio RMS meter.
- **Global Shortcut (`Meta+Alt+D`)** via `KGlobalAccel` and D-Bus control (`org.cesar.kdictation` `/Dictation` `Toggle()`).
- **Spoken Punctuation Commands**: `"coma"`, `"punto y coma"`, `"dos puntos"`, `"nueva línea"`, `"nuevo párrafo"`, `"signo de interrogación"`, `"signo de exclamación"`, `"punto final"`.
- **Automatic Wayland Clipboard Sync (`wl-copy`)** and recent dictation history.
