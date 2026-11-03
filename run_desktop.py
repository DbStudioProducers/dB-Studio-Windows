# dB Studio 0.4 - Desktop Mixer
# Interface prototype: controls, mix state, presets and visual signal generator.
# This release does not yet route physical audio devices or stream to musicians.
import json
import math
import os
import random
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
                                  showvalue=False, sliderlength=10, height=18,
                                  bg=BG, fg=TEXT, troughcolor="#41484f",
                                  highlightthickness=0, bd=0,
                                  command=self.on_pan)
        self.pan_scale.pack(fill="x")

        self.mute_button = tk.Button(self, text="MUTE", command=self.toggle_mute,
                                     bg="#561512", fg="#ffe9e7",
                                     activebackground=RED, relief="flat",
                                     font=("Segoe UI", 8, "bold"), cursor="hand2")
        self.mute_button.pack(fill="x", padx=2, pady=(5, 2), ipady=5)
        self.solo_button = tk.Button(self, text="SOLO", command=self.toggle_solo,
                                     bg="#343941", fg=TEXT,
                                     activebackground="#59616b", relief="flat",
                                     font=("Segoe UI", 8, "bold"), cursor="hand2")
        self.solo_button.pack(fill="x", padx=2, pady=2, ipady=4)

        self.meter = tk.Canvas(self, bg="#0a0c0e", width=19, height=160,
                               highlightthickness=1, highlightbackground="#30363b")
        self.meter.pack(fill="both", expand=True, padx=7, pady=(8, 3))
        self.meter.create_rectangle(3, 3, 8, 155, fill="#202b22", outline="")
        self.meter.create_rectangle(11, 3, 16, 155, fill="#202b22", outline="")
        self.meter_left = self.meter.create_rectangle(3, 155, 8, 155, fill=GREEN, outline="")
        self.meter_right = self.meter.create_rectangle(11, 155, 16, 155, fill=GREEN, outline="")
        self.meter_ticks()

        self.level_var = tk.DoubleVar(value=.72)
        self.level_scale = tk.Scale(self, variable=self.level_var, from_=1.0,
                                    to=0.0, resolution=.01, orient="vertical",
                                    showvalue=False, sliderlength=22, width=17,
                                    bg=BG, fg=TEXT, troughcolor="#3b4249",
                                    activebackground="#c4c9ce", highlightthickness=0,
                                    bd=0, command=self.on_level)
        self.level_scale.pack(fill="y", expand=True, pady=(0, 1))
        self.value_label = tk.Label(self, text="-2.9 dB", bg=BG, fg=MUTED,
                                    font=("Segoe UI", 7))
        self.value_label.pack(fill="x", pady=(0, 1))

    def meter_ticks(self):
        for y in (25, 52, 79, 106, 133):
            self.meter.create_line(0, y, 19, y, fill="#394149")

    def data(self):
        return self.app.channel_data(self.channel)

    def refresh(self):
        d = self.data()
        self.pan_var.set(d["pan"])
        self.level_var.set(d["volume"])
        self.name_button.configure(text=d["name"])
        self.mute_button.configure(bg=RED if d["mute"] else "#561512",
                                   text="MUTED" if d["mute"] else "MUTE")
        self.solo_button.configure(bg=YELLOW if d["solo"] else "#343941",
                                   fg="#171717" if d["solo"] else TEXT,
                                   text="SOLO ON" if d["solo"] else "SOLO")
        self.value_label.configure(text=self.db_text(d["volume"]))
        self.draw_meter(0 if d["mute"] else self.app.meter_value(self.channel))

    @staticmethod
    def db_text(value):
        if value <= 0.0001:
            return "-∞ dB"
        return "%.1f dB" % (20 * math.log10(value))

    def draw_meter(self, value):
        height = int(max(0, min(1, value)) * 152)
        top = 155 - height
        color = RED if value > .88 else YELLOW if value > .70 else GREEN
        self.meter.coords(self.meter_left, 3, top, 8, 155)
        self.meter.coords(self.meter_right, 11, top, 16, 155)
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
        self.root.geometry("1440x850")
        self.root.minsize(1120, 680)
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
        for label in ("▥  Mixer", "▦  Inputs", "FX", "▱  Routing", "⚙  Settings"):
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

        self.side = tk.Frame(content, bg="#15181c", width=154)
        self.side.pack(side="right", fill="y", padx=(4, 2), pady=3)
        self.side.pack_propagate(False)
        self.side_button("Fine", self.fine, "#30343a")
        self.side_button("Mute Enable", self.mute_all, RED)
        self.side_button("Snd->Fader", self.send_to_fader, "#30343a")
        tk.Label(self.side, text="CHANNEL BANK", bg="#15181c", fg=MUTED,
                 font=("Segoe UI", 7, "bold")).pack(fill="x", pady=(9, 2))
        self.side_button("Ch 1–16", lambda: self.change_bank(1), "#30343a", GREEN)
        self.side_button("Ch 17–32", lambda: self.change_bank(2), "#30343a", GREEN)
        tk.Label(self.side, text="MONITOR MIXES", bg="#15181c", fg=MUTED,
                 font=("Segoe UI", 7, "bold")).pack(fill="x", pady=(10, 2))
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
        if "Mixer" not in label:
            messagebox.showinfo("dB Studio", "%s será habilitado nas próximas etapas.\nO mixer principal permanece disponível." % label)
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
