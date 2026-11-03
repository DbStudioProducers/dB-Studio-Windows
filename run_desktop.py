# dB Studio 0.4 - Desktop Mixer
# Interface prototype: controls, mix state, presets and visual signal generator.
# This release does not yet route physical audio devices or stream to musicians.
import json
import math
import os
import random
import time
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog

BG = "#101215"
SURFACE = "#191d21"
SURFACE2 = "#22272c"
BORDER = "#343b42"
GREEN = "#43e638"
GREEN_DARK = "#1c5b20"
RED = "#ed403b"
CYAN = "#00cbd7"
TEXT = "#e9edf0"
MUTED = "#929ba3"
YELLOW = "#f2cf38"
FONT = ("Segoe UI", 9)
APP_DIR = os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), "dB Studio")
STATE_FILE = os.path.join(APP_DIR, "mixer_state.json")


def default_state():
    return {
        "mixes": [
            {"name": "Mix %02d" % (i + 1), "master": 0.75,
             "channels": {
                 str(ch): {"name": "Ch %d" % ch, "volume": 0.72, "pan": 0.0,
                           "mute": False, "solo": False}
                 for ch in range(1, 33)
             }} for i in range(16)
        ]
    }


class ChannelStrip(tk.Frame):
    def __init__(self, master, app, channel):
        super().__init__(master, bg=BG, highlightbackground=BORDER,
                         highlightthickness=1, width=65)
        self.app = app
        self.channel = channel
        self.pack_propagate(False)
        self.columnconfigure(0, weight=1)

        self.local_label = tk.Label(self, text="Lcl %02d" % channel,
                                    bg="#171a1e", fg=TEXT, font=("Segoe UI", 8),
                                    pady=5)
        self.local_label.pack(fill="x", padx=1, pady=(1, 2))
        self.name_button = tk.Button(self, text="Ch %d" % channel,
                                     command=self.rename, bg=GREEN, fg="#10200e",
                                     activebackground="#65f451", relief="flat",
                                     font=("Segoe UI", 9, "bold"), cursor="hand2")
        self.name_button.pack(fill="x", padx=1, ipady=7)

        pan_frame = tk.Frame(self, bg=BG)
        pan_frame.pack(fill="x", padx=3, pady=(6, 1))
        tk.Label(pan_frame, text="PAN", bg=BG, fg=MUTED,
                 font=("Segoe UI", 7)).pack()
        self.pan_var = tk.DoubleVar(value=0)
        self.pan_scale = tk.Scale(pan_frame, variable=self.pan_var, from_=-1,
                                  to=1, resolution=.01, orient="horizontal",
                                  showvalue=False, sliderlength=10,
                                  bg=BG, fg=TEXT, troughcolor="#41484f",
                                  highlightthickness=0, bd=0,
                                  command=self.on_pan)
        self.pan_scale.pack(fill="x")

        self.mute_button = tk.Button(self, text="MUTE", command=self.toggle_mute,
                                     bg="#561512", fg="#ffe9e7",
                                     activebackground=RED, relief="flat",
                                     font=("Segoe UI", 8, "bold"), cursor="hand2")
        self.mute_button.pack(fill="x", padx=2, pady=(5, 2), ipady=5)
        self.diagonal = tk.Canvas(self, bg=BG, height=120, highlightthickness=0)
        self.diagonal.pack(fill="both", expand=True, padx=1, pady=(0, 1))
        self.diagonal.bind("<Configure>", self.draw_diagonal)
        self.meter_fader = tk.Frame(self, bg=BG, height=255)
        self.meter_fader.pack(fill="x", side="bottom", padx=2, pady=(2, 0))
        self.meter_fader.pack_propagate(False)
        self.meter = tk.Canvas(self.meter_fader, bg="#0a0c0e", width=25, highlightthickness=0)
        self.meter.pack(side="right", fill="y", padx=(0, 1))
        self.meter.create_rectangle(2, 2, 7, 250, fill="#202b22", outline="")
        self.meter.create_rectangle(10, 2, 15, 250, fill="#202b22", outline="")
        self.meter_left = self.meter.create_rectangle(2, 250, 7, 250, fill=GREEN, outline="")
        self.meter_right = self.meter.create_rectangle(10, 250, 15, 250, fill=GREEN, outline="")
        self.meter_ticks()
        self.level_var = tk.DoubleVar(value=.72)
        self.level_scale = tk.Scale(self.meter_fader, variable=self.level_var, from_=1.0,
                                    to=0.0, resolution=.01, orient="vertical",
                                    showvalue=False, sliderlength=24, width=19,
                                    bg=BG, fg=TEXT, troughcolor="#3b4249",
                                    activebackground="#c4c9ce", highlightthickness=0,
                                    bd=0, command=self.on_level)
        self.level_scale.pack(side="left", fill="y", expand=True)
        self.value_label = tk.Label(self, text="-2.9 dB", bg=BG, fg=MUTED, font=("Segoe UI", 7))
        self.value_label.pack(fill="x", pady=(0, 1))

    def meter_ticks(self):
        for y in (42, 84, 126, 168, 210):
            self.meter.create_line(0, y, 17, y, fill="#394149")

    def draw_diagonal(self, event):
        self.diagonal.delete("all")
        self.diagonal.create_line(0, event.height-4, event.width-2, 18, fill="#806b00", width=1)
        self.diagonal.create_line(0, event.height-4, event.width-2, event.height-4, fill="#30363b", width=1)

    def data(self):
        return self.app.channel_data(self.channel)

    def refresh(self):
        d = self.data()
        self.pan_var.set(d["pan"])
        self.level_var.set(d["volume"])
        self.name_button.configure(text=d["name"])
        self.mute_button.configure(bg=RED if d["mute"] else "#561512",
                                   text="MUTED" if d["mute"] else "MUTE")
        self.value_label.configure(text=self.db_text(d["volume"]))
        self.draw_meter(0 if d["mute"] else self.app.meter_value(self.channel))

    @staticmethod
    def db_text(value):
        if value <= 0.0001:
            return "-∞ dB"
        return "%.1f dB" % (20 * math.log10(value))

    def draw_meter(self, value):
        height = int(max(0, min(1, value)) * 246)
        top = 250 - height
        color = RED if value > .88 else YELLOW if value > .70 else GREEN
        self.meter.coords(self.meter_left, 2, top, 7, 250)
        self.meter.coords(self.meter_right, 10, top, 15, 250)
        self.meter.itemconfigure(self.meter_left, fill=color)
        self.meter.itemconfigure(self.meter_right, fill=color)

    def on_level(self, value):
        d = self.data()
        d["volume"] = float(value)
        self.value_label.configure(text=self.db_text(float(value)))
        self.app.changed()

    def on_pan(self, value):
        self.data()["pan"] = float(value)
        self.app.changed()

    def toggle_mute(self):
        d = self.data()
        d["mute"] = not d["mute"]
        self.refresh()
        self.app.changed()

    def toggle_solo(self):
        d = self.data()
        d["solo"] = not d["solo"]
        self.refresh()
        self.app.changed()

    def rename(self):
        current = self.data()["name"]
        name = simpledialog.askstring("Identificação do canal",
                                      "Nome do canal %d:" % self.channel,
                                      initialvalue=current, parent=self.app.root)
        if name and name.strip():
            self.data()["name"] = name.strip()[:18]
            self.refresh()
            self.app.changed()

    def set_channel(self, channel):
        self.channel = channel
        self.local_label.configure(text="Lcl %02d" % channel)
        self.refresh()


class MixerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("dB Studio | Digital Monitor Mixer")
        self.root.configure(bg=BG)
        self.root.geometry("1280x720")
        self.root.minsize(1024, 600)
        self.mix_index = 0
        self.bank = 1
        self.demo_signal = True
        self.demo_phase = 0
        self.state = self.load_state()
        self.strips = []
        self.build_ui()
        self.refresh_all()
        self.root.after(100, self.animate)

    def load_state(self):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                loaded = json.load(f)
            if len(loaded.get("mixes", [])) == 16:
                return loaded
        except (IOError, ValueError, TypeError):
            pass
        return default_state()

    def save_state(self, quiet=True):
        try:
            os.makedirs(APP_DIR, exist_ok=True)
            with open(STATE_FILE, "w", encoding="utf-8") as f:
                json.dump(self.state, f, indent=2, ensure_ascii=False)
            self.set_status("Configuração salva neste computador")
            return True
        except Exception as exc:
            if not quiet:
                messagebox.showerror("dB Studio", "Não foi possível salvar:\n%s" % exc)
            return False

    def build_ui(self):
        top = tk.Frame(self.root, bg="#050607", height=48)
        top.pack(fill="x")
        top.pack_propagate(False)
        tk.Label(top, text="dB Studio", bg="#050607", fg="white",
                 font=("Segoe UI", 15, "bold")).pack(side="left", padx=(13, 20))
        tk.Label(top, text="MIXER", bg="#050607", fg=CYAN,
                 font=("Segoe UI", 9, "bold")).pack(side="left")
        nav = tk.Frame(top, bg="#050607")
        nav.pack(side="right", padx=12)
        for label in ("▥  Mixer", "▦  Devices", "FX", "▱  Routing", "⚙  Settings"):
            tk.Button(nav, text=label, bg="#050607", fg=TEXT,
                      activebackground="#252a30", activeforeground=CYAN,
                      relief="flat", font=("Segoe UI", 9),
                      command=lambda x=label: self.nav_action(x)).pack(side="left", padx=5)

        sub = tk.Frame(self.root, bg="#090b0d", height=34)
        sub.pack(fill="x")
        sub.pack_propagate(False)
        self.mix_title = tk.Label(sub, text="LR Mix", bg="#090b0d", fg=TEXT,
                                  font=("Segoe UI", 10, "bold"))
        self.mix_title.pack(side="left", padx=12)
        self.bank_title = tk.Label(sub, text="CHANNELS 01–16", bg="#090b0d",
                                   fg=MUTED, font=("Segoe UI", 8))
        self.bank_title.pack(side="left", padx=18)
        tk.Label(sub, text="48 kHz  |  FLOAT32 ENGINE PREVIEW",
                 bg="#090b0d", fg=MUTED, font=("Segoe UI", 8)).pack(side="right", padx=12)

        content = tk.Frame(self.root, bg=BG)
        content.pack(fill="both", expand=True)
        self.channel_area = tk.Frame(content, bg=BG)
        self.channel_area.pack(side="left", fill="both", expand=True, padx=(2, 0), pady=3)
        for i in range(16):
            self.channel_area.grid_columnconfigure(i, weight=1, uniform="strips")
            strip = ChannelStrip(self.channel_area, self, i + 1)
            strip.grid(row=0, column=i, sticky="nsew", padx=1)
            self.strips.append(strip)
        self.channel_area.grid_rowconfigure(0, weight=1)

        self.side = tk.Frame(content, bg="#15181c", width=190)
        self.side.pack(side="right", fill="y", padx=(4, 2), pady=3)
        self.side.pack_propagate(False)
        self.side_button("Fine", self.fine, "#292d33", GREEN)
        self.side_button("Mute Enable", self.mute_all, RED)
        self.side_button("Snd->Fader", self.send_to_fader, "#292d33")
        tk.Label(self.side, text="CHANNEL BANK", bg="#15181c", fg=MUTED,
                 font=("Segoe UI", 7, "bold")).pack(fill="x", pady=(5, 1))
        self.side_button("Ch 1–16", lambda: self.change_bank(1), "#292d33", GREEN)
        self.side_button("Ch 17–32", lambda: self.change_bank(2), "#292d33", GREEN)
        self.side_button("Ch 33–48", lambda: self.change_bank(2), "#292d33", GREEN)
        tk.Label(self.side, text="MONITOR MIXES", bg="#15181c", fg=MUTED,
                 font=("Segoe UI", 7, "bold")).pack(fill="x", pady=(5, 1))
        self.mix_buttons = []
        for i in range(16):
            b = tk.Button(self.side, text="Mix %02d" % (i + 1),
                          command=lambda n=i: self.select_mix(n),
                          bg="#292e34", fg=TEXT, activebackground="#3d454d",
                          relief="flat", font=("Segoe UI", 8), anchor="w")
            b.pack(fill="x", padx=4, pady=1, ipady=3)
            self.mix_buttons.append(b)
        tk.Label(self.side, text="MASTER", bg="#15181c", fg=MUTED,
                 font=("Segoe UI", 7, "bold")).pack(fill="x", pady=(10, 1))
        self.master_var = tk.DoubleVar(value=.75)
        self.master_scale = tk.Scale(self.side, variable=self.master_var,
                                     from_=1.5, to=0, resolution=.01,
                                     orient="vertical", showvalue=False,
                                     bg="#15181c", fg=TEXT, troughcolor="#3b4249",
                                     highlightthickness=0, command=self.master_changed)
        self.master_scale.pack(fill="y", expand=True)
        self.master_value = tk.Label(self.side, text="75%", bg="#15181c",
                                     fg=TEXT, font=("Segoe UI", 8))
        self.master_value.pack(pady=4)

        bottom = tk.Frame(self.root, bg="#080a0c", height=31)
        bottom.pack(fill="x", side="bottom")
        bottom.pack_propagate(False)
        self.status = tk.Label(bottom, text="READY", bg="#080a0c", fg="#b8c1c8",
                               font=("Segoe UI", 8))
        self.status.pack(side="left", padx=10)
        tk.Button(bottom, text="TEST SIGNAL  ON", command=self.toggle_demo,
                  bg="#173b1c", fg="#a7f39e", activebackground=GREEN,
                  relief="flat", font=("Segoe UI", 8, "bold")).pack(side="right", padx=6, pady=3)
        tk.Button(bottom, text="EXPORT PRESET", command=self.export_preset,
                  bg="#252a30", fg=TEXT, relief="flat",
                  font=("Segoe UI", 8)).pack(side="right", padx=3, pady=3)
        tk.Button(bottom, text="IMPORT PRESET", command=self.import_preset,
                  bg="#252a30", fg=TEXT, relief="flat",
                  font=("Segoe UI", 8)).pack(side="right", padx=3, pady=3)
        tk.Button(bottom, text="SAVE", command=lambda: self.save_state(False),
                  bg="#174b50", fg=TEXT, relief="flat",
                  font=("Segoe UI", 8, "bold")).pack(side="right", padx=3, pady=3)

    def side_button(self, text, command, color, accent=None):
        wrap = tk.Frame(self.side, bg=accent or "#15181c", padx=3, pady=2)
        wrap.pack(fill="x", pady=1)
        tk.Button(wrap, text=text, command=command, bg=color, fg=TEXT,
                  activebackground="#4a5159", relief="flat",
                  font=("Segoe UI", 8), cursor="hand2").pack(fill="both", ipady=6)

    def current_mix(self):
        return self.state["mixes"][self.mix_index]

    def channel_data(self, channel):
        return self.current_mix()["channels"][str(channel)]

    def refresh_all(self):
        for i, strip in enumerate(self.strips):
            strip.set_channel((self.bank - 1) * 16 + i + 1)
        mix = self.current_mix()
        self.mix_title.configure(text=mix["name"])
        self.master_var.set(mix["master"])
        self.master_value.configure(text="%d%%" % int(mix["master"] * 100 / 1.5))
        self.bank_title.configure(text="CHANNELS %02d–%02d" %
                                  ((self.bank - 1) * 16 + 1, self.bank * 16))
        for i, b in enumerate(self.mix_buttons):
            b.configure(bg=CYAN if i == self.mix_index else "#292e34",
                        fg="#061719" if i == self.mix_index else TEXT)
        for strip in self.strips:
            strip.refresh()

    def changed(self):
        self.save_state()
    def set_status(self, msg):
        self.status.configure(text=msg.upper()[:100])
    def select_mix(self, index):
        self.mix_index = index
        self.refresh_all()
        self.set_status("Mix %02d selecionado" % (index + 1))
    def change_bank(self, bank):
        self.bank = bank
        self.refresh_all()
        self.set_status("Banco %d–%d selecionado" % ((bank-1)*16+1, bank*16))
    def master_changed(self, value):
        self.current_mix()["master"] = float(value)
        self.master_value.configure(text="%d%%" % (float(value) * 100 / 1.5))
        self.changed()
    def mute_all(self):
        channels = self.current_mix()["channels"]
        new_value = not all(channels[str(i)]["mute"] for i in range(1, 33))
        for i in range(1, 33):
            channels[str(i)]["mute"] = new_value
        self.refresh_all()
        self.set_status("Todos os canais " + ("mutados" if new_value else "liberados"))
        self.changed()
    def fine(self):
        self.set_status("Ajuste fino: use as setas do teclado nos faders")
    def send_to_fader(self):
        self.set_status("Snd->Fader: modo de envio será integrado ao roteamento")
    def nav_action(self, label):
        if "Mixer" in label:
            return
        if "Devices" in label:
            self.open_devices()
            return
        messagebox.showinfo("dB Studio", "%s será habilitado nas próximas etapas.\nO mixer principal permanece disponível." % label)

    def demo_devices(self):
        if not hasattr(self, "_demo_devices"):
            now = time.time()
            self._demo_devices = []
            for n, sig, bat, age, status in [
                (1, -32, 92, 754, "Online"), (2, -36, 85, 917, "Online"),
                (3, -40, 76, 1041, "Online"), (4, -52, 43, 1211, "Warning"),
                (5, None, None, None, "Offline")]:
                self._demo_devices.append({
                    "id": "DEV-%03d" % n, "name": "Monitor %02d - In Ear" % n,
                    "signal": sig, "battery": bat, "started": now-age if age else None,
                    "status": status, "aux": {"master": .75, "channels": {
                        str(i): {"volume": .72, "pan": 0.0, "mute": False}
                        for i in range(1, 33)}}})
        return self._demo_devices

    def open_devices(self):
        if hasattr(self, "devices_window") and self.devices_window.winfo_exists():
            self.devices_window.deiconify()
            self.devices_window.lift()
            return
        self.devices_window = tk.Toplevel(self.root)
        win = self.devices_window
        win.title("dB Studio | Devices")
        win.geometry("1050x620")
        win.configure(bg=BG)
        tk.Label(win, text="CONNECTED DEVICES", bg="#080a0c", fg=TEXT,
                 font=("Segoe UI", 16, "bold"), pady=15).pack(fill="x")
        tk.Label(win, text="DEMONSTRAÇÃO — dispositivos fictícios; descoberta real ainda não conectada",
                 bg=BG, fg=YELLOW, font=("Segoe UI", 8, "bold")).pack(anchor="w", padx=14, pady=6)
        head = tk.Frame(win, bg=SURFACE2)
        head.pack(fill="x", padx=12, pady=(8, 2))
        for title, width in (("DEVICE NAME", 31), ("SIGNAL", 12), ("BATTERY", 12), ("CONNECTED", 14), ("STATUS", 14), ("ACTION", 12)):
            tk.Label(head, text=title, width=width, anchor="w", bg=SURFACE2,
                     fg=MUTED, font=("Segoe UI", 8, "bold"), padx=4, pady=9).pack(side="left")
        self.device_rows = tk.Frame(win, bg=BG)
        self.device_rows.pack(fill="x", padx=12)
        self.device_info = tk.Label(win, text="Selecione um dispositivo para ver detalhes.",
                                    bg=SURFACE, fg=TEXT, font=("Segoe UI", 10),
                                    anchor="w", padx=15, pady=18)
        self.device_info.pack(fill="x", padx=12, pady=12)
        self.selected_device_id = None
        self.refresh_devices()

    def refresh_devices(self):
        if not hasattr(self, "device_rows") or not self.device_rows.winfo_exists():
            return
        for w in self.device_rows.winfo_children():
            w.destroy()
        for dev in self.demo_devices():
            active = dev["status"] != "Offline"
            elapsed = self.duration_text(time.time()-dev["started"]) if active else "--:--:--"
            vals = (dev["name"], (str(dev["signal"])+" dB") if dev["signal"] is not None else "N/A",
                    (str(dev["battery"])+"%") if dev["battery"] is not None else "N/A",
                    elapsed, dev["status"])
            row = tk.Frame(self.device_rows, bg=SURFACE, highlightbackground=BORDER, highlightthickness=1)
            row.pack(fill="x", pady=2)
            for i, (value, width) in enumerate(zip(vals, (31,12,12,14,14))):
                color = GREEN if value == "Online" else YELLOW if value == "Warning" else MUTED if value == "Offline" else TEXT
                tk.Label(row, text=value, width=width, anchor="w", bg=SURFACE,
                         fg=color, font=FONT, padx=4, pady=10).pack(side="left")
            tk.Button(row, text="OPEN", command=lambda d=dev: self.select_device(d),
                      bg="#174b65", fg="white", relief="flat",
                      font=("Segoe UI", 8, "bold"), padx=10).pack(side="left", padx=5, pady=4)
        self.root.after(1000, self.refresh_devices)

    @staticmethod
    def duration_text(seconds):
        if seconds is None:
            return "--:--:--"
        seconds = max(0, int(seconds))
        return "%02d:%02d:%02d" % (seconds//3600, (seconds%3600)//60, seconds%60)

    def select_device(self, dev):
        self.selected_device_id = dev["id"]
        signal = str(dev["signal"]) + " dB" if dev["signal"] is not None else "N/A"
        battery = str(dev["battery"]) + "%" if dev["battery"] is not None else "N/A (not reported)"
        elapsed = self.duration_text(time.time()-dev["started"]) if dev["started"] else "Not connected"
        self.device_info.configure(text="%s  |  %s  |  Signal: %s  |  Battery: %s  |  Connected: %s" %
                                   (dev["name"], dev["status"], signal, battery, elapsed) +
                                   "     [OPEN AUX abaixo]")
        tk.Button(self.device_info.master, text="OPEN INDIVIDUAL AUX",
                  command=lambda d=dev: self.open_device_aux(d), bg=CYAN,
                  fg="#061719", relief="flat", font=("Segoe UI", 9, "bold"),
                  padx=12, pady=7).pack(anchor="w", padx=15, pady=(0, 12))

    def open_device_aux(self, dev):
        win = tk.Toplevel(self.devices_window)
        win.title("dB Studio | Aux — " + dev["name"])
        win.geometry("850x570")
        win.configure(bg=BG)
        tk.Label(win, text=dev["name"] + "  |  INDIVIDUAL AUX",
                 bg="#080a0c", fg=TEXT, font=("Segoe UI", 14, "bold"),
                 pady=14).pack(fill="x")
        tk.Label(win, text="CONTROLES DEMONSTRATIVOS — alterações locais, sem transmissão ao dispositivo",
                 bg=BG, fg=YELLOW, font=("Segoe UI", 8, "bold")).pack(anchor="w", padx=12, pady=6)
        master = tk.Frame(win, bg=SURFACE, padx=10, pady=8)
        master.pack(fill="x", padx=10, pady=6)
        tk.Label(master, text="AUX MASTER", bg=SURFACE, fg=TEXT).pack(side="left")
        mv = tk.DoubleVar(value=dev["aux"]["master"])
        tk.Scale(master, variable=mv, from_=1, to=0, orient="horizontal",
                 length=260, bg=SURFACE, fg=TEXT, troughcolor="#3b4249",
                 highlightthickness=0, command=lambda v: dev["aux"].update({"master":float(v)})).pack(side="left", padx=12)
        strips = tk.Frame(win, bg=BG)
        strips.pack(fill="both", expand=True, padx=8, pady=8)
        for ch in range(1, 9):
            data = dev["aux"]["channels"][str(ch)]
            box = tk.Frame(strips, bg=SURFACE, highlightbackground=BORDER, highlightthickness=1, padx=4)
            box.pack(side="left", fill="both", expand=True, padx=2)
            tk.Label(box, text="CH %02d" % ch, bg=SURFACE, fg=GREEN,
                     font=("Segoe UI", 9, "bold")).pack(pady=6)
            mute = tk.BooleanVar(value=data["mute"])
            tk.Checkbutton(box, text="MUTE", variable=mute, bg=SURFACE, fg=RED,
                           selectcolor=SURFACE2, activebackground=SURFACE,
                           command=lambda d=data, v=mute: d.update({"mute":v.get()})).pack()
            tk.Label(box, text="PAN", bg=SURFACE, fg=MUTED).pack(pady=(8,0))
            pan = tk.DoubleVar(value=data["pan"])
            tk.Scale(box, variable=pan, from_=-1, to=1, resolution=.01,
                     orient="horizontal", showvalue=False, length=75, bg=SURFACE,
                     fg=TEXT, troughcolor="#3b4249", highlightthickness=0,
                     command=lambda v,d=data: d.update({"pan":float(v)})).pack()
            tk.Label(box, text="LEVEL", bg=SURFACE, fg=MUTED).pack(pady=(8,0))
            level = tk.DoubleVar(value=data["volume"])
            tk.Scale(box, variable=level, from_=1, to=0, orient="vertical",
                     showvalue=False, length=220, bg=SURFACE, fg=TEXT,
                     troughcolor="#3b4249", highlightthickness=0,
                     command=lambda v,d=data: d.update({"volume":float(v)})).pack(fill="y", expand=True)
    def toggle_demo(self):
        self.demo_signal = not self.demo_signal
        self.set_status("Sinal de teste " + ("ativado" if self.demo_signal else "desativado"))
    def meter_value(self, channel):
        if not self.demo_signal:
            return 0
        base = .18 + .25 * (0.5 + 0.5 * math.sin(self.demo_phase + channel * .61))
        if self.channel_data(channel)["solo"]:
            base *= .95
        return base
    def animate(self):
        self.demo_phase += .13
        for strip in self.strips:
            d = strip.data()
            val = self.meter_value(strip.channel)
            if d["mute"] or (any(self.channel_data(i)["solo"] for i in range(1, 33))
                             and not d["solo"]):
                val = 0
            strip.draw_meter(val)
        self.root.after(90, self.animate)
    def export_preset(self):
        path = filedialog.asksaveasfilename(parent=self.root, title="Exportar preset",
            defaultextension=".json", filetypes=[("Preset dB Studio", "*.json")])
        if path:
            try:
                with open(path, "w", encoding="utf-8") as f:
                    json.dump(self.state, f, indent=2, ensure_ascii=False)
                self.set_status("Preset exportado")
            except Exception as exc:
                messagebox.showerror("Exportar preset", str(exc))
    def import_preset(self):
        path = filedialog.askopenfilename(parent=self.root, title="Importar preset",
            filetypes=[("Preset dB Studio", "*.json")])
        if path:
            try:
                with open(path, "r", encoding="utf-8") as f:
                    loaded = json.load(f)
                if len(loaded.get("mixes", [])) != 16:
                    raise ValueError("O preset precisa conter exatamente 16 mixes.")
                self.state = loaded
                self.refresh_all()
                self.save_state()
                self.set_status("Preset importado")
            except Exception as exc:
                messagebox.showerror("Importar preset", str(exc))


def main():
    root = tk.Tk()
    MixerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
