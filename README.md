

<h1 align="center">🚦 OpenCode Traffic Light</h1>
<p align="center"><i>A physical & virtual traffic light that shows what your AI coding agent is doing — in real time.</i></p>

<p align="center">
  <img src="https://img.shields.io/badge/🟢_Idle-Green-brightgreen?style=for-the-badge">
  <img src="https://img.shields.io/badge/🟡_Working-Yellow-yellow?style=for-the-badge">
  <img src="https://img.shields.io/badge/🔴_Need_You-Red-red?style=for-the-badge">
</p>

---

## ✨ What It Does

| Your Agent | The Light | Meaning |
|:--|:--|:--|
| Waiting for you | 🟢 Green (solid) | Idle, ready |
| Processing your request | 🟡 Yellow (breathing) | Working on it |
| Asking you a question | 🔴 Red (blinking) + beep | Needs your input |
| Error or permission needed | 🔴 Red (blinking) + beep | Something's wrong |
| Task complete | 🟢 Green (solid) + beep | Done |

**Zero manual interaction.** The light switches automatically based on OpenCode's internal events.

---

## 🚀 Quick Start

### 1. Download

```
release/
├── OpenCodeTrafficLight.exe   ← Double-click (no Python needed)
├── traffic-light.js           ← OpenCode plugin
└── opencode_light.ino         ← Arduino firmware (optional)
```

### 2. Run the app

Double-click **`OpenCodeTrafficLight.exe`**. A window pops up showing three glowing circles. An HTTP server starts silently on `127.0.0.1:9876`.

> **No Arduino?** The on-screen simulation works perfectly on its own.

### 3. Install the plugin

```bash
# Global (works for all projects):
cp traffic-light.js ~/.config/opencode/plugin/traffic-light.js

# Or project-level:
cp traffic-light.js .opencode/plugin/traffic-light.js
```

Restart OpenCode. Done. The light now tracks your agent automatically.

### 4. (Optional) Wire up Arduino

| Arduino Pin | Component |
|:--|:--|
| **D9** (PWM) | Red LED → 220Ω → GND |
| **D10** (PWM) | Yellow LED → 220Ω → GND |
| **D11** (PWM) | Green LED → 220Ω → GND |
| **D8** | Passive buzzer → GND |

Upload **`opencode_light.ino`** to your Arduino Uno. The app auto-detects it via USB.

---

## 🏗 Architecture

```
┌──────────────┐     HTTP / fetch()      ┌─────────────────────────┐
│  OpenCode    │ ──────────────────────► │  Traffic Light App      │
│  Plugin (.js)│     localhost:9876       │                         │
└──────────────┘                         │  ┌─ On-Screen GUI ────┐ │
       │                                 │  │  ●  ●  ●           │ │
       │ Listens to:                     │  │  Red Yellow Green  │ │
       │  message.part.updated           │  └────────────────────┘ │
       │  tool.execute.before            │                         │
       │  session.idle                   │  ┌─ USB Serial ───────┐ │
       │  session.error                  │  │  Arduino Uno        │ │
       │  permission.asked               │  │  (auto-detected)    │ │
       └─────────────────────────────────│  └────────────────────┘ │
                                         └─────────────────────────┘
```

---

## 📡 HTTP API

The desktop app serves a simple REST API:

| Endpoint | Effect |
|:--|:--|
| `GET /health` | 200 OK (no light change) |
| `GET /idle` | 🟢 Green solid |
| `GET /working` | 🟡 Yellow breathing |
| `GET /error` | 🔴 Red blinking + beep |
| `GET /done` | 🟢 Green solid + beep |
| `GET /off` | ⚫ All off |

---

## 🔧 Features

- 🖥️ **On-screen simulation** — works without hardware
- 🔌 **Arduino support** — auto-detects via USB VID/PID
- ⚡ **No flicker** — persistent serial connection, single startup flash
- 🎛️ **Queue-based** — commands execute in order, no race conditions
- 📦 **Single EXE** — PyInstaller packaged, zero dependencies
- 🔕 **Graceful fallback** — Arduino disconnected? Screen keeps working

---

## 🐛 Troubleshooting

| Symptom | Fix |
|:--|:--|
| Plugin not loading | File must be in `plugin/` (not `plugins/`). Restart OpenCode. |
| Lights flash on change | Make sure the EXE is running (persistent serial prevents this) |
| Arduino not detected | Check USB cable; CH340 chip needs driver |
| Port 9876 in use | Kill old `OpenCodeTrafficLight.exe` processes |
| Window won't close | Right-click taskbar icon → Close |



</p>
