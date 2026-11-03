import tkinter as tk
from tkinter import messagebox, simpledialog

BG="#101215"; SURFACE="#191d21"; SURFACE2="#22272c"; TEXT="#e9edf0"
MUTED="#929ba3"; CYAN="#00cbd7"; GREEN="#43e638"; RED="#ed403b"; YELLOW="#f2cf38"

def install(app):
    if "users" not in app.state:
        app.state["users"]=[{"id":"USR-%02d"%i,"name":n,"role":"Musician"} for i,n in enumerate(("Henrique","Lucas","Rafael","Thiago","Bateria"),1)]
    app.state.setdefault("routing",{"1-8":0,"9-16":1,"17-24":2,"25-32":3})
    app.state.setdefault("fx",{"eq":True,"compressor":False,"delay":False,"reverb":True,"low":0.0,"mid":0.0,"high":0.0})
    app.state.setdefault("settings",{"input":"Default","output":"Default","sample_rate":48000,"buffer":256})
    app.state.setdefault("master_user","admin")
    app.state.setdefault("master_device","Este computador")

def screen(app,title,size):
    w=tk.Toplevel(app.root); w.title("dB Studio | "+title); w.geometry(size); w.configure(bg=BG)
    tk.Label(w,text=title.upper(),bg="#080a0c",fg=TEXT,font=("Segoe UI",15,"bold"),anchor="w",padx=18,pady=14).pack(fill="x")
    return w

def section(parent,title):
    f=tk.LabelFrame(parent,text=title,bg=SURFACE,fg=CYAN,font=("Segoe UI",9,"bold"),padx=12,pady=10)
    f.pack(fill="x",padx=14,pady=8); return f

def open_users(app):
    w=screen(app,"Usuários e músicos","820x600")
    tk.Label(w,text="Edite o nome exibido de cada músico/auxiliar. As alterações ficam salvas neste computador.",bg=BG,fg=MUTED).pack(anchor="w",padx=16,pady=8)
    listing=tk.Frame(w,bg=BG); listing.pack(fill="both",expand=True,padx=12)
    def redraw():
        for child in listing.winfo_children(): child.destroy()
        for u in app.state["users"]:
            row=tk.Frame(listing,bg=SURFACE,padx=8,pady=7); row.pack(fill="x",pady=3)
            tk.Label(row,text=u["id"],width=12,anchor="w",bg=SURFACE,fg=MUTED).pack(side="left")
            tk.Label(row,text=u["name"],width=28,anchor="w",bg=SURFACE,fg=TEXT,font=("Segoe UI",10,"bold")).pack(side="left")
            tk.Label(row,text=u["role"],width=14,anchor="w",bg=SURFACE,fg=CYAN).pack(side="left")
            tk.Button(row,text="EDITAR NOME",bg="#174b65",fg="white",relief="flat",command=lambda x=u:edit(x)).pack(side="right")
    def edit(u):
        n=simpledialog.askstring("Editar usuário","Nome do usuário:",initialvalue=u["name"],parent=w)
        if n and n.strip(): u["name"]=n.strip()[:32]; app.save_state(); redraw()
    def add():
        n=simpledialog.askstring("Novo usuário","Nome do músico:",parent=w)
        if n and n.strip():
            app.state["users"].append({"id":"USR-%03d"%(len(app.state["users"])+1),"name":n.strip()[:32],"role":"Musician"})
            app.save_state(); redraw()
    tk.Button(w,text="+ ADICIONAR USUÁRIO",command=add,bg=GREEN,fg="#10200e",relief="flat",padx=12,pady=8).pack(anchor="w",padx=16,pady=12)
    redraw()

def open_settings(app):
    w=screen(app,"Configurações","880x620"); s=app.state["settings"]
    f=section(w,"ÁUDIO")
    fields={}
    for r,(label,key) in enumerate((("Entrada","input"),("Saída","output"))):
        tk.Label(f,text=label,bg=SURFACE,fg=TEXT).grid(row=r,column=0,sticky="w",pady=5)
        e=tk.Entry(f,width=38); e.insert(0,s.get(key,"Default")); e.grid(row=r,column=1,padx=12); fields[key]=e
    tk.Label(f,text="Taxa de amostragem",bg=SURFACE,fg=TEXT).grid(row=2,column=0,sticky="w",pady=5)
    rate=tk.StringVar(value=str(s.get("sample_rate",48000))); tk.OptionMenu(f,rate,"44100","48000","96000").grid(row=2,column=1,sticky="w")
    tk.Label(f,text="Buffer",bg=SURFACE,fg=TEXT).grid(row=3,column=0,sticky="w",pady=5)
    buf=tk.StringVar(value=str(s.get("buffer",256))); tk.OptionMenu(f,buf,"64","128","256","512","1024").grid(row=3,column=1,sticky="w")
    g=section(w,"DISPOSITIVO MESTRE / ADMINISTRADOR")
    master=tk.Entry(g,width=38); master.insert(0,app.state.get("master_device","Este computador")); master.pack(anchor="w",pady=4)
    admin=tk.Entry(g,width=38); admin.insert(0,app.state.get("master_user","admin")); admin.pack(anchor="w",pady=4)
    tk.Label(w,text="Estas são preferências locais. A seleção real de hardware, autenticação segura e controle remoto ainda dependem da integração dos serviços de áudio/rede.",bg=BG,fg=YELLOW,wraplength=820,justify="left").pack(anchor="w",padx=16,pady=8)
    def save():
        s.update({"input":fields["input"].get(),"output":fields["output"].get(),"sample_rate":int(rate.get()),"buffer":int(buf.get())})
        app.state["master_device"]=master.get().strip() or "Este computador"; app.state["master_user"]=admin.get().strip() or "admin"
        app.save_state(False); messagebox.showinfo("dB Studio","Preferências salvas.",parent=w)
    tk.Button(w,text="SALVAR CONFIGURAÇÕES",command=save,bg=GREEN,fg="#10200e",relief="flat",padx=14,pady=8).pack(anchor="e",padx=16,pady=12)

def open_fx(app):
    w=screen(app,"FX e processamento","900x590"); fx=app.state["fx"]; f=section(w,"PROCESSADORES")
    flags={}
    for key,label in (("eq","Equalizador"),("compressor","Compressor"),("delay","Delay"),("reverb","Reverb")):
        v=tk.BooleanVar(value=fx.get(key,False)); flags[key]=v
        tk.Checkbutton(f,text=label,variable=v,bg=SURFACE,fg=TEXT,selectcolor=SURFACE2).pack(anchor="w",pady=4)
    e=section(w,"EQUALIZAÇÃO — GANHO EM dB"); gains={}
    for key,label in (("low","Low"),("mid","Mid"),("high","High")):
        tk.Label(e,text=label,bg=SURFACE,fg=TEXT).pack(side="left",padx=5)
        v=tk.DoubleVar(value=fx.get(key,0)); gains[key]=v
        tk.Scale(e,variable=v,from_=-12,to=12,resolution=.5,orient="horizontal",length=190,bg=SURFACE,fg=TEXT,troughcolor="#3b4249",highlightthickness=0).pack(side="left",padx=4)
    tk.Label(w,text="Controles e parâmetros são persistidos no projeto. A aplicação DSP em tempo real ainda requer a cadeia de processamento conectada ao stream.",bg=BG,fg=YELLOW,wraplength=850,justify="left").pack(anchor="w",padx=16,pady=8)
    def save():
        fx.update({k:v.get() for k,v in flags.items()}); fx.update({k:float(v.get()) for k,v in gains.items()})
        app.save_state(False); messagebox.showinfo("dB Studio","Parâmetros FX salvos.",parent=w)
    tk.Button(w,text="APLICAR E SALVAR",command=save,bg=GREEN,fg="#10200e",relief="flat",padx=14,pady=8).pack(anchor="e",padx=16,pady=12)

def open_routing(app):
    w=screen(app,"Roteamento","820x540")
    tk.Label(w,text="Atribuição lógica dos grupos de entradas aos mixes estéreo.",bg=BG,fg=MUTED).pack(anchor="w",padx=16,pady=8)
    f=section(w,"MATRIZ DE ROTEAMENTO"); choices={}
    for key in ("1-8","9-16","17-24","25-32"):
        row=tk.Frame(f,bg=SURFACE); row.pack(fill="x",pady=6)
        tk.Label(row,text="Canais "+key,width=20,anchor="w",bg=SURFACE,fg=TEXT).pack(side="left")
        v=tk.StringVar(value="Mix %02d"%(app.state["routing"].get(key,0)+1)); choices[key]=v
        tk.OptionMenu(row,v,*["Mix %02d"%(i+1) for i in range(16)]).pack(side="left")
    tk.Label(w,text="A matriz é salva, mas ainda não altera o roteamento físico do motor até a integração com as interfaces de áudio.",bg=BG,fg=YELLOW,wraplength=780,justify="left").pack(anchor="w",padx=16,pady=8)
    def save():
        for key,v in choices.items(): app.state["routing"][key]=int(v.get().split()[-1])-1
        app.save_state(False); messagebox.showinfo("dB Studio","Matriz salva.",parent=w)
    tk.Button(w,text="SALVAR ROTEAMENTO",command=save,bg=GREEN,fg="#10200e",relief="flat",padx=14,pady=8).pack(anchor="e",padx=16,pady=12)
