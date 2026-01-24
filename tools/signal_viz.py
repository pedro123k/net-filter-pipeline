import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import asyncio
import numpy as np
import struct

N_FLOATS = 128
FLOAT_SIZE = 4
BYTES_EXPECTED = N_FLOATS * FLOAT_SIZE

class RawUDPRX(asyncio.DatagramProtocol):
    def __init__(self, queue):
        self._queue = queue
    def datagram_received(self, data, addr):

        if (len(data) != BYTES_EXPECTED):
            return

        arr = np.frombuffer(data, dtype=f"<f4", count=N_FLOATS).copy()

        if self._queue.full():
            try:
                self._queue.get_nowait()
            except asyncio.QueueEmpty:
                pass
        self._queue.put_nowait(arr)

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

class SignalVizualizatorComponent(ttk.Frame):

    def __init__(self, parent, configs):
        super().__init__(parent)
        
        self._port = configs["port"]

        self._edit_cmd = None
        self.on_destroy = None

        self._build()
        self._build_plots()

    def update_data(self, configs):
        self._port = configs["port"]
        self._figure.suptitle(f"Signal from port {self._port}", fontsize=6, y=0.90, fontweight="bold")

        self._update_graphs_limits()

        loop = asyncio.get_event_loop()
        asyncio.run_coroutine_threadsafe(self._restart(loop), loop)

    def _update_graphs_limits(self):
        self._ax1.set_ylim(self._ymin.get(), self._ymax.get())
        self._ax2.set_ylim(self._Ymin.get(), self._Ymax.get())
        self._mpl_canvas.draw_idle()

    def _build(self):
        self._plot_area = ttk.Frame(self, padding = (5, 5))
        self._plot_area.pack(side="top", fill="x")

        self._controls = ttk.Frame(self)
        self._controls.pack(side="top", fill="x")

        self._sample_limiter = ttk.Frame(self._controls)
        self._sample_limiter.pack(side="left", anchor="nw", padx=10)

        self._nsamples_var = tk.IntVar(value=128)

        self._label_nsamples = ttk.Label(self._sample_limiter, text="SAMPLES: ", style="Title.TLabel")
        self._label_nsamples.pack(side="top", anchor="nw")

        self._samples_input = ttk.Spinbox(
            self._sample_limiter,
            from_=  128, to = 1280, increment = 128,
            textvariable = self._nsamples_var, width = 5 
        )

        self._samples_input.pack(side="top", anchor="center")

        self._ymin = tk.DoubleVar(value= -1.0)
        self._ymax = tk.DoubleVar(value= 1.0)

        self._Ymin = tk.DoubleVar(value= -30.0)
        self._Ymax = tk.DoubleVar(value= 10.0)

        force_update = lambda a, b, c: self._update_graphs_limits()

        self._ymin.trace_add("write", force_update)
        self._ymax.trace_add("write", force_update)
        self._Ymin.trace_add("write", force_update)
        self._Ymax.trace_add("write", force_update)

        self._ylimiter = ttk.Frame(self._controls)
        self._ylimiter.pack(side="left", anchor="w")

        self._label_ylimits = ttk.Label(self._ylimiter, text="y[n] LIMITS:", style="Title.TLabel")
        self._label_ylimits.pack(side="top", fill="x")
        
        self._numberimput_ymax = ttk.Spinbox(
            self._ylimiter,
            from_= 0.0,
            to = 10.0,
            increment=0.2,
            textvariable=self._ymax, width = 5
        )
        self._numberimput_ymax.pack(side="top", anchor="center")

        self._numberimput_ymin = ttk.Spinbox(
            self._ylimiter,
            from_= -10.0,
            to = 0.0,
            increment = -0.2,
            textvariable=self._ymin, width = 5
        )
        self._numberimput_ymin.pack(side="top", anchor="center", pady = 4)

        self._Ylimiter = ttk.Frame(self._controls)
        self._Ylimiter.pack(side="left", anchor="w", padx=10)

        self._label_Ylimits = ttk.Label(self._Ylimiter, text="Y(e^jω) LIMITS:", style="Title.TLabel")
        self._label_Ylimits.pack(side="top", fill="x", padx=10)

        self._numberimput_Ymax = ttk.Spinbox(
            self._Ylimiter,
            from_= 0.0,
            to = 100,
            increment = 10.0,
            textvariable=self._Ymax, width = 5
        )
        self._numberimput_Ymax.pack(side="top", anchor="center")

        self._numberimput_Ymin = ttk.Spinbox(
            self._Ylimiter,
            from_= -100.0,
            to = 0.0,
            increment = -10.0,
            textvariable=self._Ymin, width = 5
        )
        self._numberimput_Ymin.pack(side="top", anchor="center", pady = 4)

        self._btn_remove = ttk.Button(
            self._controls, text="REMOVE", command= self._destroy,
            style="Menu.TButton"
        )
        self._btn_remove.pack(side="right", anchor="se", padx=5)

        self._btn_edit = ttk.Button(self._controls, text="EDIT",
            command = lambda: self._edit_cmd () if self._edit_cmd else None,
            style="Menu.TButton"                        
        )
        self._btn_edit.pack(side="right", anchor="sw")


    def _build_plots(self):
        self._figure = Figure(figsize=(3, 2), dpi=200)
        
        self._ax1 = self._figure.add_subplot(221, )
        self._ax1.set_xlim(0, 128)
        self._ax1.tick_params(axis="both", labelsize=3)
        self._ax1.set_xlabel("n", fontsize=4)
        self._ax1.set_ylabel("y[n]", fontsize=4)

        self._ax2 = self._figure.add_subplot(222)
        self._ax2.set_ylim(-30, 10)
        self._ax2.set_xlim(0,1)
        self._ax2.tick_params(axis="both", labelsize=3)
        self._ax2.set_xlabel("ω (rad/amostra)", fontsize=4)
        self._ax2.set_ylabel("|Y(ω)", fontsize=4)
        
        self._ax3 = self._figure.add_subplot(224)
        self._ax3.set_xlim(0,1)
        self._ax3.set_ylim(-np.pi, np.pi)
        self._ax3.tick_params(axis="both", labelsize=3)
        self._ax3.set_xlabel("ω (rad/amostra)", fontsize=4)
        self._ax3.set_ylabel("∠Y(ω)", fontsize=4)

        self._mpl_canvas = FigureCanvasTkAgg(self._figure, master=self._plot_area)
        self._mpl_widget = self._mpl_canvas.get_tk_widget()

        self._mpl_widget.pack(side="top", fill="x", expand=True)

        self._figure.suptitle(f"Signal from port {self._port}", fontsize=6, y=0.90, fontweight="bold")
        self._figure.tight_layout()

        self._line1 = self._ax1.plot([], [])[0]
        self._line2 = self._ax2.plot([], [])[0]
        self._line3 = self._ax3.plot([], [])[0]

    def _update_plots(self, y):
        x = np.arange(start = 0, stop= y.shape[0])
        self._line1.set_data(x, y)
        self._ax1.set_xlim(0, x.shape[0] - 1)

        Y = np.fft.rfft(y)
        N = Y.size

        k = np.arange(N)

        w = 2 * np.pi * k / N 

        mag = np.abs(Y)
        ang = np.angle(Y)

        self._line2.set_data(w, mag)
        self._ax2.set_xlim(0, np.pi)

        self._line3.set_data(w, ang)
        self._ax3.set_xlim(0, np.pi)

        self._mpl_canvas.draw_idle()

    def start(self, loop):
        self._task = loop.create_task(self._run())

    async def _restart(self, loop):
        self.stop()
        task = getattr(self, "_task", None)
        if task:
            try:
                await task
            except Exception:
                pass
        self.start(loop)

    async def _run(self):
        self.queue = asyncio.Queue(10)

        self._transport, _ = await asyncio.get_event_loop().create_datagram_endpoint(
            lambda: RawUDPRX(self.queue),
            local_addr=("0.0.0.0", self._port)
        )

        try:
            input = []
            while True:

                expected_pkgs = max(1, self._nsamples_var.get() // 128)

                input.append(await self.queue.get())
                if len(input) >= expected_pkgs:
                    self._update_plots(np.concatenate(input))
                    input.clear()
        finally:
            self._transport.close()

    def stop(self):

        if getattr(self, "_transport", None) is not None:
            try: 
                self._transport.close()
            except Exception:
                pass
            finally:
                self._transport = None 

        if getattr(self, "_task", None) and not self._task.done():
            self._task.cancel()            

    def _destroy(self):
        self.destroy()
        if self.on_destroy:
            self.on_destroy()

class TransferFuncComponent(ttk.Frame):
    def __init__(self, parent, coeffs_path):
        super().__init__(parent)

        self._build_transfer_func(coeffs_path)
        self._build()
        self._update_graph()

    def _build_transfer_func(self, coeffs_path):

        self._func = []

        coeffs = []

        with open(coeffs_path) as f:
            for l in f.readlines():
                coeffs.append(float(l.rstrip()))

        qt_funcs = len(coeffs) // 6

        c_exp = lambda e: lambda w: np.exp(1j*e*w)

        eps = 1e-8
        
        for i in range(qt_funcs):
            b0i = coeffs[6 * i + 3]
            b1i = coeffs[6 * i + 4]
            b2i = coeffs[6 * i + 5]
            a0i = coeffs[6 * i + 0]
            a1i = coeffs[6 * i + 1]
            a2i = coeffs[6 * i + 2]

            func_nom = lambda w : b0i + b1i * c_exp(-1)(w) + b2i * c_exp(-2)(w) + eps
            func_den = lambda w : a0i + a1i * c_exp(-1)(w) + a2i * c_exp(-2)(w) + eps

            self._func.append(lambda w: func_nom(w) / func_den(w))

    def eval_func(self, w):
        output = w

        for f in self._func:
            output = f(output)

        return output 


    def _build(self):
        
        self._plot_area = ttk.Frame(self, padding = (5, 5))
        self._plot_area.pack(side="top", fill="x")

        self._controls = ttk.Frame(self, padding = (5, 5))
        self._controls.pack(side="top", fill="x")

        self._label_limits = ttk.Label(self._controls, text="LIMITS: ", style="Title.TLabel")
        self._label_limits.pack(side="left", anchor="w")

        self._mag_max_var = tk.DoubleVar(value=10)
        self._mag_min_var = tk.DoubleVar(value=-30)

        self._mag_max_var.trace_add("write", lambda a, b, c: self._update_graph())
        self._mag_min_var.trace_add("write", lambda a, b, c: self._update_graph())

        self._input_mag_max = ttk.Spinbox(self._controls,
            from_= 0.0,
            to = 100.0,
            increment = 10.0,                                  
            textvariable=self._mag_max_var, width=4
        )
        self._input_mag_max.pack(side="left", anchor="e", padx=3)

        self._input_mag_min = ttk.Spinbox(self._controls,
            from_= -100.0,
            to = 0.0,
            increment = -10.0,                                  
            textvariable=self._mag_min_var, width = 4                                      
        )
        self._input_mag_min.pack(side="left", anchor="e", padx=3)

        self._btn_remove = ttk.Button(
            self._controls, text="REMOVE", command = self.destroy,
            style="Menu.TButton" 
        )
        self._btn_remove.pack(side="right", anchor="e")

        self._figure = Figure(figsize=(3,2), dpi=200)

        self._ax1 = self._figure.add_subplot(121)
        self._ax1.set_xlim(0, 1)
        self._ax1.tick_params(axis="both", labelsize=3)
        self._ax1.set_xlabel("ω / ω0 ", fontsize=4)
        self._ax1.set_ylabel("|H(jω)|", fontsize=4)

        self._ax2 = self._figure.add_subplot(122)
        self._ax2.set_xlim(0, 1)
        self._ax2.set_ylim(-np.pi, np.pi)
        self._ax2.tick_params(axis="both", labelsize=3)
        self._ax2.set_xlabel("ω / ω0 ", fontsize=4)
        self._ax2.set_ylabel("∠H(jω)", fontsize=4)

        w = np.linspace(start=0, stop=np.pi, num=100)
        H = self.eval_func(w)

        mag = 20 * np.log10(np.abs(H))
        deg = np.angle(H)

        self._mlp_canvas = FigureCanvasTkAgg(self._figure, master=self._plot_area)
        self._mlp_widget = self._mlp_canvas.get_tk_widget()
        self._mlp_widget.pack(side="top", fill="x")

        self._figure.suptitle(f"Frequency Response", fontsize=7, y=0.90, fontweight="bold")
        

        self._line1 = self._ax1.plot(w / np.pi, mag)
        self._line2 = self._ax2.plot(w / np.pi, deg)

    def _update_graph(self):
        self._ax1.set_ylim(
            self._mag_min_var.get(),
            self._mag_max_var.get()
        )

        self._figure.tight_layout()
        self._mlp_canvas.draw_idle()


class App(tk.Tk):
    def __init__(self):
        super().__init__()

        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)

        self._vizualizators = []
        
        self.title("Signal Vizualizator")
        self.geometry("720x540")

        self._style()
        self._build()
        self._tick()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _style(self):
        self.style = ttk.Style()

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
        self.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)

        root = ttk.Frame(self, padding = 6)
        root.grid(row=0, column=0, sticky="nsew")
        root.rowconfigure(0, weight=1)
        root.rowconfigure(1, weight=0)
        root.columnconfigure(0, weight=1)

        self._vizualizator_list = ScrollContainer(root)
        self._vizualizator_list.pack(side="top", fill="both", expand=True)

        btn_container = ttk.Frame(root)
        btn_container.pack(side="top", fill="x")

        button_add_signal = ttk.Button(btn_container, text="ADD SIGNAL", command=lambda : self._signal_dialog())
        button_add_signal.pack(side="left", anchor="w", padx=5)

        button_add_func = ttk.Button(btn_container, text="LOAD COEFFS", command=lambda : self._coeff_dialog())
        button_add_func.pack(side="left", anchor="w")

    def _tick(self):
        self.loop.call_soon(self.loop.stop)
        self.loop.run_forever()
        self.after(10, self._tick)

    def _add_signal_visualization(self, dialog, configs):
        viz = SignalVizualizatorComponent(self._vizualizator_list.body, configs)
        viz._edit_cmd = lambda : self._signal_dialog(viz)
        viz.pack(side="top", fill="x", expand=True)

        viz.start(self.loop)
        viz.on_destroy = lambda : viz.stop()

        self._vizualizators.append(viz)

        dialog.destroy()

    def _coeff_dialog(self):
        coeffs_path = filedialog.askopenfilename(
            title="Load Coeffs file",
            filetypes=[("Text File", "*.txt")]
        )

        func_vizualizator = TransferFuncComponent(self._vizualizator_list.body, coeffs_path)
        func_vizualizator.pack(side="top", fill="x")

    def _update_vizualizator(self, dialog, cell, configs):
        cell.update_data(configs)
        dialog.destroy()

    def _signal_dialog(self, cell: SignalVizualizatorComponent = None):
        dialog = tk.Toplevel(self)
        dialog.title(
            "New Signal Visualization" if not cell else "Edit Signal Visualization"
        )
        dialog.transient(self)
        dialog.grab_set()

        input_port_var = tk.IntVar(
            value=12345 if not cell else cell._port
        )

        dialog_body = ttk.Frame(dialog, padding = (5, 5))
        dialog_body.pack(side="top", fill="both", anchor="center")

        label_port = ttk.Label(dialog_body, text="Port:", style="Title.TLabel")
        label_port.pack(side="top", fill="x")

        inputnumber = ttk.Entry(dialog_body, textvariable=input_port_var, style="Input.TEntry")
        inputnumber.pack(side="top", fill="x")

        hframe = ttk.Frame(dialog_body)
        hframe.pack(side="bottom", fill="both", expand=True , anchor="s", ipady = 5)

        add_cmd = lambda : self._add_signal_visualization(dialog, {"port": input_port_var.get()})
        edit_cmd = lambda : self._update_vizualizator(dialog, cell, {"port": input_port_var.get()})

        btn_add = ttk.Button(hframe, 
            text="ADD" if not cell else "UPDATE",
            command= add_cmd if not cell else edit_cmd,
            style="Menu.TButton"
        )
        btn_add.pack(side="left", anchor="sw", padx = 3)

        btn_cancel = ttk.Button(
            hframe, text="CANCEL", command=lambda: dialog.destroy(),
            style="Menu.TButton"
        )
        btn_cancel.pack(side="right", anchor="se", padx = 3)

        dialog.wait_window()


    def _on_close(self):

        for viz in getattr(self, "_vizualizators", []):
            viz.stop()

        try:
            self.loop.call_soon(self.loop.stop)
            self.loop.run_forever()
        except Exception:
            pass
        
        self.destroy()

if __name__ == "__main__":
    App().mainloop()