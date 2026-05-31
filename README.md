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
| Processing your request | 🟡 Yellow (blinking) | Working on it |
| Asking you a question | 🔴 Red (solid) | Needs your input |
| Error or permission needed | 🔴 Red (solid) | Something's wrong |
| Waiting for you | 🟢 Green (solid) | Idle, ready |
| OpenCode disconnected | ⚫ All off (after 4s) | Plugin unreachable |

**Zero manual interaction.** The light switches automatically based on OpenCode's internal events.

---

## 🚀 Quick Start

### 1. Download

```
release/
├── OpenCodeTrafficLight.exe   ← Double-click to run
├── traffic-light.js           ← OpenCode plugin
├── light_app.py               ← Source code (run with Python)
├── opencode_light.ino         ← Arduino firmware (optional)
└── README.md
```

### 2. Run the app

**Option A — EXE (no Python required):**

Double-click **`OpenCodeTrafficLight.exe`** (~18 MB). A window with three glowing circles appears. An HTTP server starts on `127.0.0.1:9876`.

**Option B — from source (recommended for customization):**

```bash
pip install pyserial pillow
python light_app.py
```

- Starts dark by default, syncs to OpenCode's current state after 0.5 seconds
- Click the **Pin** button to keep the window always-on-top
- Automatically goes dark within 4s if OpenCode is closed
- Arduino auto-detected via USB (optional)

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

> **Note:** Arduino LED effects differ from the on-screen GUI — yellow uses pulse instead of blink, red uses blink + beep for higher visibility on physical hardware.

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
       │  question.replied               │  │  (auto-detected)    │ │
       │  permission.asked               │  └────────────────────┘ │
       │                                 │                         │
       │  State API (:9877)              │  On startup, app pings  │
       │  GET /state → {"state":"..."}   │─ :9877/state to sync   │
       └─────────────────────────────────│  every 2s as watchdog   │
                                         └─────────────────────────┘
```

---

## 📡 HTTP API

### Traffic Light App (port 9876)

| Endpoint | Effect |
|:--|:--|
| `GET /health` | 200 OK (no light change) |
| `GET /idle` | 🟢 Green solid |
| `GET /working` | 🟡 Yellow blinking |
| `GET /error` | 🔴 Red solid |
| `GET /done` | 🟢 Green solid |
| `GET /off` | ⚫ All off |

### Plugin State API (port 9877)

| Endpoint | Returns |
|:--|:--|
| `GET /state` | `{"state": "idle"\|"working"\|"error"\|"off"}` |

The desktop app queries this on startup and every 2 seconds to stay in sync. If the plugin is unreachable for 4+ seconds, all lights turn off automatically.

---

## 🔧 Features

- 🖥️ **On-screen simulation** — works without hardware
- 🔌 **Arduino support** — auto-detects via USB VID/PID
- 🪄 **Anti-aliased circles** — smooth edges rendered with Pillow
- 🟡 **Yellow blink** — working state is a steady 500ms blink
- ⚫ **Default dark** — starts with all lights off, syncs state automatically
- 🔍 **Watchdog** — polls the plugin every 2s; goes dark if OpenCode disconnects
- 📌 **Always-on-top** — pin button to keep the window visible
- 🖥️ **High DPI aware** — no blur on high-resolution displays
- ↕️ **Resizable** — drag to any size, circles scale proportionally
- ⚡ **No flicker** — persistent serial connection, single startup pulse
- 🎛️ **Queue-based** — commands execute in order, no race conditions
- 📦 **Single EXE** — PyInstaller packaged (NumPy excluded to keep it lean)
- 🔕 **Graceful fallback** — Arduino disconnected? Screen keeps working

---

## 🐛 Troubleshooting

| Symptom | Fix |
|:--|:--|
| Plugin not loading | File must be in `plugin/` (not `plugins/`). Restart OpenCode. |
| Green light stuck after starting EXE | Wait 0.5s for state sync from plugin |
| Lights don't turn off when OpenCode closes | Wait up to 4s for watchdog to detect disconnection |
| Lights flash on state change | Make sure the EXE is running (persistent serial prevents this) |
| Arduino not detected | Check USB cable; CH340 chip needs driver |
| Port 9876 or 9877 in use | Kill old `OpenCodeTrafficLight.exe` processes |
| EXE won't launch | Install from source: `pip install pyserial pillow && python light_app.py` |
| Window won't close | Right-click taskbar icon → Close |
