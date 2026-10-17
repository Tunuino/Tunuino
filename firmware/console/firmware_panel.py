#!/usr/bin/env python3
"""Tunuino Console: initial desktop firmware flashing panel (ST-Link/SWD)."""
import hashlib
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from flash_stm32 import flash, validate_image


class FirmwarePanel(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent, padding=16)
        self.path = tk.StringVar()
        self.digest = tk.StringVar()
        self.device = tk.StringVar(value="0x419")
        self.confirm = tk.BooleanVar(value=False)
        ttk.Label(self, text="Tunuino Console | Firmware STM32").grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(self, text="Imagem .bin").grid(row=1, column=0, sticky="w")
        ttk.Entry(self, textvariable=self.path, width=55, state="readonly").grid(row=1, column=1, sticky="ew")
        ttk.Button(self, text="Selecionar", command=self.choose).grid(row=1, column=2)
        ttk.Label(self, text="SHA-256 aprovado").grid(row=2, column=0, sticky="w")
        ttk.Entry(self, textvariable=self.digest, width=65).grid(row=2, column=1, columnspan=2, sticky="ew")
        ttk.Label(self, text="STM32 Device ID").grid(row=3, column=0, sticky="w")
        ttk.Entry(self, textvariable=self.device).grid(row=3, column=1, sticky="w")
        ttk.Checkbutton(self, text="Confirmei a placa, alimentação e cargas desligadas", variable=self.confirm).grid(row=4, column=0, columnspan=3, sticky="w")
        ttk.Button(self, text="Verificar ficheiro", command=self.inspect).grid(row=5, column=0)
        ttk.Button(self, text="Gravar firmware (ST-Link)", command=self.program).grid(row=5, column=1, sticky="w")
        self.status = tk.StringVar(value="Nenhum firmware selecionado")
        ttk.Label(self, textvariable=self.status, wraplength=550).grid(row=6, column=0, columnspan=3, sticky="w")
        self.columnconfigure(1, weight=1)

    def choose(self):
        selected = filedialog.askopenfilename(filetypes=[("STM32 binary", "*.bin")])
        if selected:
            self.path.set(selected)
            self.digest.set("")
            self.confirm.set(False)
            self.status.set("Verifique o SHA-256 aprovado antes de gravar")

    def inspect(self):
        try:
            path = Path(self.path.get())
            validate_image(path)
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
            self.status.set("SHA-256 calculado: " + actual + " | Confirme com a versão aprovada")
        except (OSError, ValueError) as exc:
            self.status.set(str(exc))

    def program(self):
        if not self.confirm.get():
            messagebox.showerror("Operação bloqueada", "Confirme a placa e desligue as cargas.")
            return
        if not messagebox.askyesno("Confirmar gravação", "Vai escrever a memória Flash da ECU por ST-Link. Continuar?"):
            return
        try:
            self.status.set("A programar e verificar...")
            self.update_idletasks()
            flash(Path(self.path.get()), "STM32_Programmer_CLI", True, False, self.digest.get().strip(), self.device.get().strip())
            self.status.set("Gravação e verificação concluídas pelo STM32CubeProgrammer")
            messagebox.showinfo("Firmware", "Gravação e verificação concluídas")
        except (ValueError, OSError, FileNotFoundError, Exception) as exc:
            self.status.set("Falha na gravação: " + str(exc))
            messagebox.showerror("Falha na gravação", str(exc))


def main():
    root = tk.Tk()
    root.title("Tunuino Console | Firmware")
    FirmwarePanel(root).pack(fill="both", expand=True)
    root.mainloop()


if __name__ == "__main__":
    main()
