import tkinter as tk
from tkinter import ttk
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import asyncio
import numpy as np
import struct

async def sin_pkg_generator(freq:float, samp_freq: float, noisy:bool = False):
    dt = 1 / samp_freq
    init = 0

    while True:

        x = init + np.arange(start=0, stop=128) * dt
        y = np.sin(2 * np.pi * freq * x)

        if noisy:
            y += np.sqrt(0.05) * np.random.randn(*x.shape)

        init = x[-1] + dt

        await asyncio.sleep(128 / samp_freq)
        yield y

async def squared_pkg_generator(freq:float, samp_freq: float, noisy:bool = False):
    dt = 1 / samp_freq
    init = 0

    while True:

        x = init + np.arange(start=0, stop=128) * dt
        y = np.where(np.sin(2 * np.pi * freq * x) >= 0, 1.0, 0.0).astype(np.float32)

        init = x[-1] + dt
        
        if noisy:
            y += np.sqrt(0.05) * np.random.randn(*x.shape)
        
        await asyncio.sleep(128 / samp_freq)
        yield y

async def triang_pkg_generator(freq:float, samp_freq: float, noisy:bool = False):
    dt = 1 / samp_freq
    init = 0
    T = 1 / freq

    while True:

        x = init + np.arange(start=0, stop=128) * dt
        z = x  - T * np.floor(x / T)

        y1 = np.where(
            (0 <= z) & (z <= T/2), 1 - (4/T) * z, 0
        ).astype(np.float32)
        
        y2 = np.where(
            (T/2 < z) & (z <= T),
            -3 + (4/T) * z, 0
        ).astype(np.float32)
        

        y = y1 + y2 

        init = x[-1] + dt

        if noisy:
            y += np.sqrt(0.05) * np.random.randn(*x.shape)

        await asyncio.sleep(128 / samp_freq)
        yield y

async def ptrain_pkg_generator(freq:float, samp_freq: float, noisy:bool = False):
    dt = 1 / samp_freq
    init = 0
    T = 1 / freq

    while True:

        x = init + np.arange(start=0, stop=128) * dt
        y = np.where(np.sin(2 * np.pi * freq * x) >= 1 - dt / 2, 1.0, 0.0)

        init = x[-1] + dt

        if noisy:
            y += np.sqrt(0.05) * np.random.randn(*x.shape)

        await asyncio.sleep(128 / samp_freq)
        yield y

class ScrollContainer(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        style = ttk.Style()
        bg = style.lookup("TFrame", "background") or parent.cget("background")

        canvas = tk.Canvas(self, highlightthickness=0, bd=0, background=bg)
        vsb = ttk.Scrollbar(self, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=vsb.set)

        self.body = ttk.Frame(canvas)
        win = canvas.create_window((0, 0), window=self.body, anchor="nw")

        canvas.grid(row=0, column=0, sticky="nsew")
        vsb.grid(row=0, column=1, sticky="ns")
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        self.body.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.bind("<Configure>", lambda e: canvas.itemconfigure(win, width=e.width))

class SignalGeneratorComponent(ttk.Frame):
    def __init__(self, parent, configs):
        super().__init__(parent)
    
        self._freq = configs["freq"]
        self._wave = configs["wave"]
        self._port = configs["port"]
        self._noisy = configs["noisy"]
        self._samp_freq = configs["samp_freq"]
        self._client_addr = configs["client_addr"]
        self._server_port = configs["server_port"]
        self._PGK_FMT = "<QH128f"

        self._config_command = None
        self.on_destroy = None

        self._build()
        self._build_plot()
        

    def _destroy(self):
        self.destroy()
        if self.on_destroy:
            self.on_destroy()

    def _build(self):
        
        self._plot_area = ttk.Frame(self, padding = (7, 7))
        self._plot_area.pack(side="top", fill="x")

        self._controls = ttk.Frame(self, padding=(3, 3))
        self._controls.pack(side="top", fill="x")

        self._enable = tk.BooleanVar(value=False)
        self._switch = ttk.Checkbutton(self._controls, text="SENDING", variable=self._enable)
        self._switch.pack(side="left", anchor="w", ipadx=25)

        self.label_sample = ttk.Label(self._controls, text="SAMPLES: ", style="Title.TLabel")
        self.label_sample.pack(side="left", anchor="e")

        self._nsamples = tk.IntVar(value=128)
        self._sample_input = ttk.Spinbox(self._controls, 
            from_=128, to=1280, increment=128, wrap=True,
            textvariable=self._nsamples, width = 5
        )
        self._sample_input.pack(side="left", anchor="w")

        self._btn_del = ttk.Button(
            self._controls, text="REMOVE", command = self._destroy,
            style="Menu.TButton"
        )
        self._btn_del.pack(side="right", anchor="e", padx=3)

        self._btn_config = ttk.Button(self._controls, text="CONFIGURE",
            command = lambda: self._config_command() if self._config_command else None,
            style="Menu.TButton"
        )
        self._btn_config.pack(side="right", anchor="e", padx=3)


    def _build_plot(self):
        self._figure = Figure(figsize=(5, 1.5), dpi=200)
        self._ax = self._figure.add_subplot(111)

        self._ax.set_ylim(-1.2, 1.2)
        self._ax.set_xlim(0, 128)

        self._ax.set_title(f"{self._wave} to Port {self._port}", fontsize=6, fontweight="bold")

        self._ax.tick_params(axis="both", labelsize=4)
        self._ax.set_xlabel("n", fontsize=5)
        self._ax.set_ylabel("y[n]", fontsize=5)

        self._mpl_canvas = FigureCanvasTkAgg(self._figure, master=self._plot_area)
        self._mpl_widget = self._mpl_canvas.get_tk_widget()
        self._mpl_widget.pack(side="top", fill="x", expand=True)

        self._line, = self._ax.plot([], []) 

        self._figure.tight_layout()

        self._figure.subplots_adjust(left=0.12, right=0.97, top=0.88, bottom=0.18)

    def _update_plot(self, y):
        x = np.arange(start=0, stop=y.shape[0])
        self._line.set_data(x, y)

        self._ax.set_xlim(0, x.shape[0] - 1)
        self._mpl_canvas.draw_idle()

    def start(self, loop):
        self._task = loop.create_task(self._run())        

    async def _run(self):

        self._transport, _ = await asyncio.get_running_loop().create_datagram_endpoint(
            asyncio.DatagramProtocol,
            local_addr=("0.0.0.0", 0),
            remote_addr = (self._client_addr, self._server_port)
        ) 

        _generator = {
            "Sin Wave": sin_pkg_generator,
            "Square Wave": squared_pkg_generator,
            "Triangular Wave": triang_pkg_generator,
            "Pulse Train": ptrain_pkg_generator,
        }

        buff = []
        i = 0
        async for y in _generator[self._wave](self._freq, self._samp_freq, self._noisy):
            buff.extend(y.tolist())
            
            if self._enable.get():
                pkg = struct.pack(self._PGK_FMT, i, self._port, *(y.astype(np.float32).tolist()))
                self._transport.sendto(pkg)

            if len(buff) >= self._nsamples.get():
                self.after(0, self._update_plot, np.array(buff))
                buff.clear()

            i += 1
        

    def stop(self):
        if getattr(self, "_task", None) and not self._task.done():
            self._task.cancel()

        if getattr(self, "_transport", None):
            self._transport.close()

    def update_data(self, configs):
        self._freq = configs["freq"]
        self._wave = configs["wave"]
        self._port = configs["port"]
        self._noisy = configs["noisy"]
        self._samp_freq = configs["samp_freq"]
        self._server_port = configs["server_port"]
        self._client_addr = configs["client_addr"]

        self._ax.set_title(f"{self._wave} to Port {self._port}", fontsize=6, fontweight="bold")

        self.stop()
        self.start(asyncio.get_event_loop())

class App(tk.Tk):
    def __init__(self):
        super().__init__()

        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)

        self.title("Signal Generator")
        self.geometry("900x600")

        self._generators = []

        self._style()
        self._build()

        self._tick()
        
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _on_close(self):
        for comp in getattr(self, "_generators", []):
            comp.stop()

        try:
            self.loop.call_soon(self.loop.stop)
            self.loop.run_forever()
        except Exception:
            pass

        self.destroy() 
        

    def _style(self):
        self.style = ttk.Style(self)
        
        try:
            self.style.theme_use("clam")

            self.style.configure("Title.TLabel",
                font=("TkDefaultFont", 10, "bold"),
                padding = (0, 6)                     
            )

            self.style.configure("Input.TEntry",
                font=("TkDefaultFont", 8, "normal"),
                padding = (2, 2)
            )

            self.style.configure("Menu.TButton",
                font=("TkDefaultFont", 10, "normal"),
                padding = (0, 6)
            )

            self.style.configure("Border.TFrame",
                borderwidth=1,
                relief="solid",
                padding = (5, 5)                     
            )


        except tk.TclError:
            pass

    def _build(self):

        root = ttk.Frame(self, padding=6)
        root.grid(row=0, column=0, sticky="nsew")
        self.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)

        root.rowconfigure(0, weight=1)
        root.columnconfigure(0, weight=0)
        root.columnconfigure(1, weight=1)

        self.samp_freq_var = tk.DoubleVar(value=2000.0)
        self.server_port_var = tk.IntVar(value=55555)
        self.client_addr_var = tk.StringVar(value="127.0.0.1")

        left_menu = ttk.Frame(root, padding=(5, 5))
        left_menu.grid(row=0, column=0, sticky="nw")

        server_config_box = ttk.Frame(left_menu)
        server_config_box.grid(row=0, column=0, sticky="ew")

        label_server_port = ttk.Label(server_config_box, text="Server Port: ", style="Title.TLabel")
        label_server_port.pack(side="top", anchor="w")

        textinput_server_port = ttk.Entry(server_config_box, textvariable=self.server_port_var, style="Input.TEntry")
        textinput_server_port.pack(side="top", fill="x")

        label_client_addr = ttk.Label(server_config_box, text="Client IPV4 Addr:", style="Title.TLabel")
        label_client_addr.pack(side="top", anchor="w")

        textinput_client_addr = ttk.Entry(server_config_box, textvariable=self.client_addr_var, style="Input.TEntry")
        textinput_client_addr.pack(side="top", fill="x")

        label_samp_freq = ttk.Label(server_config_box, text="Sampling Frequency (Hz):", style="Title.TLabel")
        label_samp_freq.pack(side="top", anchor="e")

        numberinput_samp_freq = ttk.Entry(server_config_box, textvariable=self.samp_freq_var, style="Input.TEntry")
        numberinput_samp_freq.pack(side="top", fill="x", anchor="e")

        button_add_signal = ttk.Button(server_config_box, text="ADD SIGNAL",
            command= lambda: self._signal_dialog(),
            style="Menu.TButton")
        button_add_signal.pack(side="top", fill="x", anchor="e", pady=10)

        body = ttk.Frame(root, style="Border.TFrame")
        body.grid(row=0, column=1, sticky="nsew")
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)

        self._generator_list = ScrollContainer(body)
        self._generator_list.grid(row=0, column=0, sticky="nsew")
        self._generator_list.rowconfigure(0, weight=1)
        self._generator_list.columnconfigure(0, weight=1)

    def _add_new_generator(self, dialog, configs):
        comp = SignalGeneratorComponent(self._generator_list.body, configs)
        self._generators.append(comp)
        comp._config_command = lambda: self._signal_dialog(comp)
        comp.pack(side="top", fill="x", expand=True)
        comp.start(self.loop)
        comp.on_destroy = lambda: comp.stop()
        dialog.destroy()
    
    def _update_generator(self, dialog, cell, configs):
        cell.update_data(configs)
        dialog.destroy()


    def _signal_dialog(self, cell: SignalGeneratorComponent = None):
        signal_dialog = tk.Toplevel(self)
        signal_dialog.title(
            "New Signal Configuration" if not cell else "Signal Configuration"
        )
        signal_dialog.transient(self)
        signal_dialog.grab_set()

        dialog_body = ttk.Frame(signal_dialog, padding = (8, 8))
        dialog_body.pack(side="top", fill="both", anchor="center")

        signal_choice_var = tk.StringVar(value="Sin Wave" if not cell else cell._wave)
        noisy_var = tk.BooleanVar(value = False if not cell else cell._noisy)
        signal_freq_var = tk.DoubleVar(value=100 if not cell else cell._freq)
        output_port_var = tk.IntVar(value=12345 if not cell else cell._port)

        label_signal = ttk.Label(dialog_body, text="Signal Type:", style="Title.TLabel")
        label_signal.pack(side="top", anchor="w", fill="x")

        combobox_sinals = tk.OptionMenu(dialog_body, signal_choice_var, "Sin Wave", "Square Wave", "Triangular Wave", "Pulse Train")
        combobox_sinals.pack(side="top", fill="x")

        label_port = ttk.Label(dialog_body, text="Output Port:", style="Title.TLabel")
        label_port.pack(side="top", fill="x")

        numberimput_port = ttk.Entry(dialog_body, textvariable=output_port_var, style="Input.TEntry")
        numberimput_port.pack(side="top", fill="x")

        frame_noise = ttk.Frame(dialog_body)
        frame_noise.pack(side="top", fill="x")

        label_noise = ttk.Label(frame_noise, text="Noise?", style="Title.TLabel")
        label_noise.pack(side="left", anchor="w", fill="x")

        checkbox_noisy = ttk.Checkbutton(frame_noise, variable=noisy_var)
        checkbox_noisy.pack(side="right", anchor="e")

        label_frequency = ttk.Label(dialog_body, text="Frequency (Hz): ", style="Title.TLabel")
        label_frequency.pack(side="top", anchor="w")

        numberinput_signal_freq = ttk.Entry(dialog_body, textvariable=signal_freq_var, style="Input.TEntry")
        numberinput_signal_freq.pack(side="top", fill="x")

        frame_btn = ttk.Frame(dialog_body)
        frame_btn.pack(side="top", fill="x", pady=8)

        get_generator_configs = lambda : {
            "freq": signal_freq_var.get(),
            "samp_freq": self.samp_freq_var.get(),
            "wave": signal_choice_var.get(),
            "noisy": noisy_var.get(),
            "port": output_port_var.get(),
            "client_addr": self.client_addr_var.get(),
            "server_port": self.server_port_var.get()
        }

        add_cmd = lambda: self._add_new_generator(
            signal_dialog, 
            get_generator_configs()
        )

        update_cmd = lambda: self._update_generator(
            signal_dialog,
            cell,
            get_generator_configs()
        )

        btn_add = ttk.Button(frame_btn, 
            text="ADD" if not cell else "UPDATE", 
            command = add_cmd if not cell else update_cmd,
            style="Menu.TButton"
        )
        btn_add.pack(side="left", anchor="w", padx=3)

        btn_cancel = ttk.Button(
            frame_btn, text="CANCEL", command=lambda: signal_dialog.destroy(),
            style="Menu.TButton"
        )
        btn_cancel.pack(side="right", anchor="e", padx=3)

        signal_dialog.wait_window()

    def _tick(self):
        self.loop.call_soon(self.loop.stop)
        self.loop.run_forever()
        self.after(10, self._tick)

if __name__ == "__main__":
    App().mainloop()
