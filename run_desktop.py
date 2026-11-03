import tkinter as tk
from tkinter import ttk

BG = "#111316"
PANEL = "#202126"
GREEN = "#45e638"
RED = "#f0443e"
CYAN = "#00cbd7"
TEXT = "#e7e7e7"
MUTED = "#8e9299"

class ChannelStrip(tk.Frame):
    def __init__(self, master, number, app):
        super().__init__(master, bg=BG, highlightbackground="#34383d", highlightthickness=1)
        self.number = number
        self.app = app
        self.muted = False
        self.pan = tk.DoubleVar(value=0)
        self.level = tk.DoubleVar(value=0.72)
        self.configure(width=66)
        self.grid_propagate(False)

        tk.Label(self, text="Lcl %02d" % number, bg="#17191c", fg=TEXT,
                 font=("Segoe UI", 9), height=2).pack(fill="x", padx=1, pady=(1,2))
        tk.Button(self, text="Ch %d" % number, bg=GREEN, fg="#10200e",
                  activebackground="#65f451", relief="flat", font=("Segoe UI", 9),
                  command=lambda: app.select_channel(number)).pack(fill="x", padx=1, ipady=8)

        panbox = tk.Frame(self, bg=BG, height=34)
        panbox.pack(fill="x", padx=2, pady=(3,0))
        panbox.pack_propagate(False)
        tk.Scale(panbox, variable=self.pan, from_=-1, to=1, resolution=0.1,
                 orient="horizontal", showvalue=False, sliderlength=8, length=58,
                 bg=BG, fg=TEXT, troughcolor="#34383d", highlightthickness=0,
                 bd=0, command=lambda _v: app.set_status("PAN canal %d: %.1f" % (number, self.pan.get()))).pack(fill="x")

        area = tk.Frame(self, bg="#15181b", height=138, highlightbackground="#5c5717", highlightthickness=1)
        area.pack(fill="x", padx=1, pady=(4,5))
        area.pack_propagate(False)
        tk.Label(area, text="╲", bg="#15181b", fg="#9a8500", font=("Segoe UI", 28)).place(relx=.5, rely=.45, anchor="center")

        self.mute_btn = tk.Button(self, text="MUTE", bg="#510d09", fg="#fff0ed",
                                  activebackground=RED, relief="flat", font=("Segoe UI", 9),
                                  command=self.toggle_mute)
        self.mute_btn.pack(fill="x", padx=1, pady=(0,5), ipady=6)

        meter = tk.Frame(self, bg=BG)
        meter.pack(fill="both", expand=True, padx=3)
        scale = tk.Frame(meter, bg=BG, width=20)
        scale.pack(side="left", fill="y")
        for label in ("-∞", "10", "5", "0", "-5", "-10", "-20", "-30", "-50"):
            tk.Label(scale, text=label, bg=BG, fg="#9da1a5", font=("Segoe UI", 6)).pack(anchor="w", pady=2)
        self.meter_canvas = tk.Canvas(meter, bg="#171a1d", highlightthickness=0, width=28)
        self.meter_canvas.pack(side="left", fill="y", expand=True)
        self.meter_canvas.create_rectangle(4, 16, 10, 180, fill="#202b22", outline="")
        self.meter_canvas.create_rectangle(13, 16, 19, 180, fill="#202b22", outline="")
        self.meter_green = self.meter_canvas.create_rectangle(4, 90, 10, 180, fill="#58e64a", outline="")
        self.meter_yellow = self.meter_canvas.create_rectangle(13, 55, 19, 180, fill="#e9d735", outline="")
        self.meter_red = self.meter_canvas.create_rectangle(22, 38, 26, 110, fill=RED, outline="")
        self.slider = tk.Scale(self, variable=self.level, from_=1.0, to=0.0, resolution=.01,
                               orient="vertical", showvalue=False, sliderlength=24,
                               width=18, bg=BG, fg=TEXT, troughcolor="#3b3e42",
                               activebackground="#aaa", highlightthickness=0, bd=0,
                               command=lambda v: app.set_status("Canal %d: %.0f%%" % (number, float(v)*100)))
        self.slider.pack(side="bottom", fill="y", pady=(0,3))
        tk.Label(self, text="Ch %d" % number, bg=BG, fg=TEXT, font=("Segoe UI", 8)).pack(side="bottom", fill="x")

    def toggle_mute(self):
        self.muted = not self.muted
        self.mute_btn.configure(bg=RED if self.muted else "#510d09",
                                text="MUTED" if self.muted else "MUTE")
        self.app.set_status("Canal %d %s" % (self.number, "mutado" if self.muted else "ativo"))

    def set_number(self, number):
        self.number = number
        for child in self.winfo_children():
            if isinstance(child, tk.Button) and child.cget("text").startswith("Ch "):
                child.configure(text="Ch %d" % number)
        self.app.set_status("Banco de canais atualizado")

class DBStudioApp:
    def __init__(self, root):
        self.root = root
        self.bank = 1
        self.selected = 1
        root.title("dB Studio")
        root.configure(bg=BG)
        root.geometry("1280x720")
        root.minsize(1050, 620)
        root.protocol("WM_DELETE_WINDOW", root.destroy)

        top = tk.Frame(root, bg="#050505", height=48)
        top.pack(fill="x")
        top.pack_propagate(False)
        tk.Label(top, text="Mixer", bg="#050505", fg="white",
                 font=("Segoe UI", 13)).pack(side="left", padx=8)
        tk.Label(top, text="▥     ▦       FX       ▱       ♯       ⚙       ⋯",
                 bg="#050505", fg="#eeeeee", font=("Segoe UI", 17)).pack(side="right", padx=18)
        sub = tk.Frame(root, bg="#050505", height=25)
        sub.pack(fill="x")
        tk.Label(sub, text="LR Mix", bg="#050505", fg="#d8d8d8",
                 font=("Segoe UI", 9)).pack(side="left", padx=8)

        body = tk.Frame(root, bg=BG)
        body.pack(fill="both", expand=True)
        self.strips_frame = tk.Frame(body, bg=BG)
        self.strips_frame.pack(side="left", fill="both", expand=True)
        self.strips = [ChannelStrip(self.strips_frame, i, self) for i in range(1,17)]
        for i, strip in enumerate(self.strips):
            strip.grid(row=0, column=i, sticky="nsew")
            self.strips_frame.grid_columnconfigure(i, weight=1, uniform="channel")
        self.strips_frame.grid_rowconfigure(0, weight=1)

        side = tk.Frame(body, bg=BG, width=192)
        side.pack(side="right", fill="y", padx=(2,0))
        side.pack_propagate(False)
        self.side_button(side, "Fine", "#25272c", self.fine)
        self.side_button(side, "Mute Enable", RED, self.mute_enable)
        self.side_button(side, "Snd->Fader", "#30313a", lambda: self.set_status("Snd->Fader selecionado"))
        self.side_button(side, "Mix 1", "#30313a", lambda: self.set_status("Mix estéreo 1 selecionado"), accent=CYAN)
        self.side_button(side, "Ch 1-16", "#30313a", lambda: self.change_bank(1), accent=GREEN)
        self.side_button(side, "Ch 17-32", "#30313a", lambda: self.change_bank(2), accent=GREEN)
        self.side_button(side, "Ch 33-48", "#30313a", lambda: self.set_status("Banco futuro: canais 33-48"), accent=GREEN)
        self.side_button(side, "FxRet", "#30313a", lambda: self.set_status("Retornos de efeitos"))
        self.side_button(side, "Mix", "#30313a", lambda: self.set_status("Seleção de mixes"), accent=CYAN)
        self.side_button(side, "Grp/FxSnd", "#30313a", lambda: self.set_status("Grupos / envios FX"), accent="#2476ff")
        self.side_button(side, "Mtx/Main", "#30313a", lambda: self.set_status("Matriz / Main"), accent="#d400d8")
        self.side_button(side, "DCA", "#30313a", lambda: self.set_status("Controles DCA"), accent=RED)

        status = tk.Frame(root, bg="#090a0b", height=24)
        status.pack(fill="x", side="bottom")
        status.pack_propagate(False)
        self.status_label = tk.Label(status, text="SIMULAÇÃO  |  32 INPUTS  |  16 MIXES  |  48 kHz",
                                     bg="#090a0b", fg="#b8bec4", font=("Segoe UI", 8))
        self.status_label.pack(side="left", padx=8)

    def side_button(self, parent, label, color, command, accent=None):
        wrap = tk.Frame(parent, bg=accent or BG, padx=3, pady=2)
        wrap.pack(fill="x", pady=1)
        btn = tk.Button(wrap, text=label, command=command, bg=color, fg=TEXT,
                        activebackground="#454952", activeforeground="white",
                        relief="flat", font=("Segoe UI", 9), anchor="center")
        btn.pack(fill="both", expand=True, ipady=9)

    def set_status(self, message):
        self.status_label.configure(text="SIMULAÇÃO  |  " + message)

    def select_channel(self, number):
        self.selected = number
        self.set_status("Canal %d selecionado" % number)

    def change_bank(self, bank):
        self.bank = bank
        start = 1 if bank == 1 else 17
        for i, strip in enumerate(self.strips):
            strip.set_number(start+i)
        self.set_status("Banco %d–%d selecionado" % (start, start+15))

    def fine(self):
        self.set_status("Ajuste fino disponível para os controles")

    def mute_enable(self):
        self.set_status("Modo de habilitação de MUTE")

def main():
    root = tk.Tk()
    DBStudioApp(root)
    root.mainloop()

if __name__ == "__main__":
    main()
