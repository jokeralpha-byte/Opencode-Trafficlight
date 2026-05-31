"""
OpenCode Traffic Light App
==========================
GUI + HTTP server + optional Arduino serial.
Works standalone with on-screen simulation even without Arduino hardware.
"""
import ctypes, sys, os
if sys.platform == "win32":
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except Exception:
            pass

import tkinter as tk
from http.server import HTTPServer, BaseHTTPRequestHandler
from PIL import Image, ImageDraw, ImageTk
import serial, serial.tools.list_ports
import threading, time, queue, json, http.client

# ===================== GUI =====================
class TrafficLightGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("OpenCode Traffic Light")
        self.root.geometry("440x840")
        self.root.minsize(360, 680)
        self.root.resizable(True, True)
        self.root.configure(bg="#1a1a2e")
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        # State
        self._state = "idle"
        self._arduino = False
        self._yellow_on = True

        # Title
        tk.Label(self.root, text="OpenCode", font=("Consolas", 14, "bold"),
                 fg="#e0e0e0", bg="#1a1a2e").pack(pady=(15, 0))
        tk.Label(self.root, text="Traffic Light", font=("Consolas", 10),
                 fg="#888888", bg="#1a1a2e").pack(pady=(0, 10))

        # Traffic light frame
        self.light_frame = tk.Frame(self.root, bg="#0f0f23")
        self.light_frame.pack(expand=True, fill="both", padx=10, pady=5)

        # Three lights
        self.canvas = tk.Canvas(self.light_frame, bg="#0f0f23", highlightthickness=0)
        self.canvas.pack(expand=True, fill="both")
        self.canvas.bind("<Configure>", self._on_resize)

        # Colors
        self._dim = {"red": "#330000", "yellow": "#333300", "green": "#003300"}
        self._bright = {"red": "#FF1400", "yellow": "#F7B500", "green": "#00FF92"}

        # Status bar
        self.status_label = tk.Label(self.root, text="IDLE", font=("Consolas", 11, "bold"),
                                      fg="#00cc66", bg="#1a1a2e")
        self.status_label.pack(pady=(8, 0))

        # Arduino indicator
        self.arduino_label = tk.Label(self.root, text="No Arduino", font=("Consolas", 8),
                                       fg="#666666", bg="#1a1a2e")
        self.arduino_label.pack(pady=(2, 0))

        # Port info
        self.port_label = tk.Label(self.root, text="HTTP :9876", font=("Consolas", 8),
                                    fg="#555555", bg="#1a1a2e")
        self.port_label.pack(pady=(2, 10))

        # 置顶按钮
        self._topmost = False
        self.top_btn = tk.Button(self.root, text="置顶", font=("SimHei", 9),
                                  fg="#888888", bg="#2a2a4e", relief="flat",
                                  activebackground="#3a3a6e", activeforeground="#e0e0e0",
                                  command=self._toggle_topmost)
        self.top_btn.pack(pady=(5, 10))

    def _toggle_topmost(self):
        self._topmost = not self._topmost
        self.root.attributes('-topmost', self._topmost)
        self.top_btn.config(text="已置顶" if self._topmost else "置顶",
                            fg="#ffcc00" if self._topmost else "#888888")

    def _on_resize(self, event):
        """Redraw anti-aliased circles proportionally when canvas resizes."""
        w, h = event.width, event.height
        if w < 10 or h < 10:
            return
        pad_y = h * 0.04
        gap = h * 0.04
        dia = int(min(w * 0.56, (h - 2 * pad_y - 2 * gap) / 3))
        if dia < 4:
            return
        self.canvas.delete("all")
        self._circles = {"red": {}, "yellow": {}, "green": {}}
        positions = {}
        for i, name in enumerate(("red", "yellow", "green")):
            y = int(pad_y + i * (dia + gap))
            x = int((w - dia) / 2)
            dim_img = self._make_circle(dia, self._dim[name])
            bright_img = self._make_circle(dia, self._bright[name])
            self._circles[name]["dim"] = dim_img
            self._circles[name]["bright"] = bright_img
            img_id = self.canvas.create_image(x, y, anchor="nw", image=dim_img)
            positions[name] = img_id
        self._circle_ids = positions
        self._apply_state()

    def _make_circle(self, dia, fill_color):
        """Render an anti-aliased circle with Pillow."""
        img = Image.new("RGBA", (dia, dia), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        draw.ellipse((0, 0, dia - 1, dia - 1), fill=fill_color)
        return ImageTk.PhotoImage(img)

    def _apply_state(self):
        """Re-apply current light state (used after resize or state change)."""
        c = {"idle": "green", "working": "yellow", "error": "red", "done": "green", "off": None}.get(self._state)
        for name in ("red", "yellow", "green"):
            img_id = getattr(self, "_circle_ids", {}).get(name)
            if img_id is None:
                continue
            if name == "yellow" and c == "yellow":
                img = self._circles[name]["bright"] if self._yellow_on else self._circles[name]["dim"]
            else:
                img = self._circles[name]["bright"] if name == c else self._circles[name]["dim"]
            self.canvas.itemconfig(img_id, image=img)

    def set_state(self, state):
        """Thread-safe state update."""
        self.root.after(0, self._set_state, state)

    def _set_state(self, state):
        self._state = state

        colors = {"idle": "green", "working": "yellow", "error": "red", "done": "green", "off": None}
        c = colors.get(state)

        if c == "yellow":
            self._start_blink()
        else:
            self._stop_blink()
            self._yellow_on = False

        self._apply_state()

        labels = {"idle": ("IDLE", "#00cc66"), "working": ("WORKING", "#F7B500"),
                   "error": ("ERROR", "#FF1400"), "done": ("DONE", "#00cc66"),
                   "off": ("OFF", "#666666")}
        text, color = labels.get(state, (state.upper(), "#888888"))
        self.status_label.config(text=text, fg=color)

    def _start_blink(self):
        if getattr(self, "_blinking", False):
            return
        self._blinking = True
        self._blink_tick()

    def _stop_blink(self):
        self._blinking = False

    def _blink_tick(self):
        if not self._blinking or self._state != "working":
            self._blinking = False
            return
        self._yellow_on = not self._yellow_on
        self._apply_state()
        self.root.after(500, self._blink_tick)

    def set_arduino(self, connected, port=""):
        self.root.after(0, self._set_arduino, connected, port)

    def _set_arduino(self, connected, port):
        self._arduino = connected
        if connected:
            self.arduino_label.config(text=f"Arduino: {port}", fg="#00cc66")
        else:
            self.arduino_label.config(text="No Arduino (simulated)", fg="#888888")

    def on_close(self):
        self.root.destroy()

    def run(self):
        self.root.mainloop()


# ===================== Serial =====================
class SerialWorker:
    def __init__(self, gui, cmd_queue):
        self.gui = gui
        self.queue = cmd_queue
        self._ser = None
        self._running = True

    def find_port(self):
        for p in serial.tools.list_ports.comports():
            if p.vid in (0x2341, 0x2A03, 0x1A86):
                return p.device
        return None

    def connect(self):
        port = self.find_port()
        if not port:
            return None
        try:
            s = serial.Serial(port, 115200, timeout=1)
            time.sleep(1.5)
            s.reset_input_buffer()
            dl = time.time() + 2
            while time.time() < dl:
                if s.in_waiting:
                    if "RDY" in s.readline().decode(errors="replace"):
                        return s
            return s
        except:
            return None

    def run(self):
        # Try connect once at start
        s = self.connect()
        if s:
            self._ser = s
            port = s.port
            self.gui.set_arduino(True, port)
        else:
            self.gui.set_arduino(False)

        cmd_map = {
            "idle":    [b"O\n", b"G\n"],
            "working": [b"O\n", b"YP\n"],
            "error":   [b"O\n", b"RB\n", b"B:3\n"],
            "done":    [b"O\n", b"G\n", b"B:2\n"],
            "off":     [b"O\n"],
        }

        while self._running:
            try:
                cmd = self.queue.get(timeout=0.5)
            except:
                continue

            data = cmd_map.get(cmd, [])
            self.gui.set_state(cmd)
            for d in data:
                if self._ser and self._ser.is_open:
                    try:
                        self._ser.write(d)
                        self._ser.flush()
                        time.sleep(0.05)
                    except:
                        try: self._ser.close()
                        except: pass
                        self._ser = None
                        self.gui.set_arduino(False)
                        break


# ===================== HTTP Server =====================
class HTTPHandler(BaseHTTPRequestHandler):
    cmd_queue = None  # set by factory

    def do_GET(self):
        cmd = self.path.strip("/")
        if cmd == "health":
            self._respond(200)
        elif cmd in {"working", "done", "error", "idle", "off"}:
            self._respond(200)
            self.cmd_queue.put(cmd)
        else:
            self._respond(404)

    def _respond(self, code):
        self.send_response(code)
        self.end_headers()
        self.wfile.write(b"ok\n")
        self.wfile.flush()

    def log_message(self, *a): pass


def make_handler(cmd_queue):
    class Handler(HTTPHandler):
        pass
    Handler.cmd_queue = cmd_queue
    return Handler


# ===================== Main =====================
def main():
    # Command queue (HTTP -> Serial/GUI)
    cmd_queue = queue.Queue()

    # GUI
    gui = TrafficLightGUI()

    # Serial worker (background)
    serial_worker = SerialWorker(gui, cmd_queue)
    threading.Thread(target=serial_worker.run, daemon=True).start()

    # HTTP server (background)
    handler = make_handler(cmd_queue)
    httpd = HTTPServer(("127.0.0.1", 9876), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()

    # Initial state: dark until plugin confirms
    cmd_queue.put("off")

    # Watchdog: sync state from plugin, turn off when disconnected
    def _watchdog():
        fail_count = 0
        need_sync = True
        while True:
            time.sleep(0.5 if need_sync else 2)
            try:
                conn = http.client.HTTPConnection("127.0.0.1", 9877, timeout=1)
                conn.request("GET", "/state")
                resp = conn.getresponse()
                if resp.status == 200:
                    data = json.loads(resp.read().decode())
                    state = data.get("state", "idle")
                    if need_sync or fail_count >= 2:
                        cmd_queue.put(state)
                        need_sync = False
                    fail_count = 0
                else:
                    fail_count += 1
            except Exception:
                fail_count += 1
            if fail_count == 2:
                cmd_queue.put("off")
    threading.Thread(target=_watchdog, daemon=True).start()

    print("OpenCode Traffic Light running on http://127.0.0.1:9876")
    gui.run()

    # Cleanup
    httpd.shutdown()
    serial_worker._running = False


if __name__ == "__main__":
    main()
