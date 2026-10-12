"""GUI only: launch with COM port argument, then operate buttons/date fields."""
import argparse
from datetime import datetime
from pathlib import Path
import queue
import sys
import time
import tkinter as tk
from tkinter import ttk, messagebox
from protocol import validate_time, ProtocolError
from transport import SerialWorker
from demo import DemoSerial

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'build/python-deps'))
DAYS = ['Понедельник', 'Вторник', 'Среда', 'Четверг', 'Пятница', 'Суббота', 'Воскресенье']
STATUS = {'RUNNING': 'Идут', 'STOPPED': 'Остановлены', 'STARTING': 'Запуск', 'ERROR': 'Ошибка'}

def serial_factory(port):
    try:
        import serial
    except ImportError as exc:
        raise OSError('Установите зависимость: python -m pip install -r lab3/client/requirements.txt') from exc
    return serial.Serial(port, 57600, timeout=.05, write_timeout=1, bytesize=8, parity='N', stopbits=1,
                         xonxoff=False, rtscts=False, dsrdtr=False)

class ClockWindow:
    def __init__(self, root, port, log_path, factory=None):
        self.root, self.port, self.log_path = root, port, log_path
        self.factory = factory or (DemoSerial if port == 'DEMO' else serial_factory)
        self.events = queue.Queue()
        self.worker = None
        self.connected = False
        self.last_state = 0
        self.closed = False
        root.title('SDK-1.1M — часы MCP79411' + (' — ДЕМОНСТРАЦИЯ' if port == 'DEMO' else ''))
        root.geometry('760x620'); root.minsize(700, 600)
        style = ttk.Style(root)
        if 'clam' in style.theme_names():
            style.theme_use('clam')
        style.configure('Title.TLabel', font=('Segoe UI', 18, 'bold'))
        style.configure('Time.TLabel', font=('Consolas', 36, 'bold'))
        frame = ttk.Frame(root, padding=24); frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Часы реального времени', style='Title.TLabel').pack(anchor='w')
        subtitle = 'Демонстрация без платы' if port == 'DEMO' else f'Порт {port} · 57600 бит/с · I²C / OLED'
        ttk.Label(frame, text=subtitle).pack(anchor='w', pady=(4, 16))
        self.connection = tk.StringVar(value='Открываем порт…')
        ttk.Label(frame, textvariable=self.connection).pack(anchor='w')
        panel = ttk.LabelFrame(frame, text='Показания стенда', padding=16); panel.pack(fill='x', pady=12)
        self.time_text = tk.StringVar(value='--:--:--')
        self.date_text = tk.StringVar(value='--/--/-- · ---')
        self.state_text = tk.StringVar(value='Нет данных')
        self.details = tk.StringVar(value='OLED: неизвестно')
        ttk.Label(panel, textvariable=self.time_text, style='Time.TLabel').pack(anchor='center')
        ttk.Label(panel, textvariable=self.date_text, font=('Segoe UI', 14)).pack(anchor='center')
        ttk.Label(panel, textvariable=self.state_text, font=('Segoe UI', 12, 'bold')).pack(pady=(10, 4))
        ttk.Label(panel, textvariable=self.details).pack()
        edit = ttk.LabelFrame(frame, text='Установить значения в MCP79411', padding=12); edit.pack(fill='x')
        now = datetime.now()
        self.date_value = tk.StringVar(value=now.strftime('%Y-%m-%d'))
        self.time_value = tk.StringVar(value=now.strftime('%H:%M:%S'))
        self.day_value = tk.StringVar(value=DAYS[now.isoweekday() - 1])
        for column, title in enumerate(['Дата (ГГГГ-ММ-ДД)', 'Время (ЧЧ:ММ:СС)', 'День недели']):
            ttk.Label(edit, text=title).grid(row=0, column=column, sticky='w', padx=4)
            edit.columnconfigure(column, weight=1)
        ttk.Entry(edit, textvariable=self.date_value, width=18).grid(row=1, column=0, sticky='ew', padx=4, pady=8)
        ttk.Entry(edit, textvariable=self.time_value, width=16).grid(row=1, column=1, sticky='ew', padx=4)
        ttk.Combobox(edit, textvariable=self.day_value, values=DAYS, state='readonly', width=18).grid(row=1, column=2, sticky='ew', padx=4)
        ttk.Label(edit, text='Годы 2000–2099. День недели задаётся независимо от даты.').grid(row=2, column=0, columnspan=3, sticky='w', padx=4)
        buttons = ttk.Frame(frame); buttons.pack(fill='x', pady=14)
        self.set_button = ttk.Button(buttons, text='Установить в часы', command=self.set_time, state='disabled')
        self.set_button.pack(side='left')
        ttk.Button(buttons, text='Подставить время ПК', command=self.pc_time).pack(side='left', padx=8)
        self.refresh_button = ttk.Button(buttons, text='Обновить', command=self.refresh, state='disabled')
        self.refresh_button.pack(side='left')
        self.connect_button = ttk.Button(buttons, text='Переподключить', command=self.connect)
        self.connect_button.pack(side='right')
        self.notice = tk.StringVar(value='После открытия порта автоматически отправляется PING.')
        ttk.Label(frame, textvariable=self.notice, wraplength=680).pack(anchor='w', pady=(0, 10))
        ttk.Label(frame, text='Журнал UART: ' + str(Path(log_path).resolve()), wraplength=680).pack(anchor='w')
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.connect()
        root.after(50, self.poll)

    def connect(self):
        if self.worker and self.worker.is_alive():
            self.notice.set('Предыдущее соединение ещё работает.'); return
        self.connected = False
        self.worker = SerialWorker(self.port, self.log_path, self.events, self.factory, self.port == 'DEMO')
        self.connection.set('Открываем порт и проверяем PING…')
        self.worker.start()

    def pc_time(self):
        now = datetime.now()
        self.date_value.set(now.strftime('%Y-%m-%d')); self.time_value.set(now.strftime('%H:%M:%S'))
        self.day_value.set(DAYS[now.isoweekday() - 1])
        self.notice.set('Время ПК подставлено в поля. Для записи нажмите «Установить в часы».')

    def refresh(self):
        if self.connected:
            self.worker.request('GET')

    def set_time(self):
        if not self.connected:
            return
        try:
            date, time_text = self.date_value.get(), self.time_value.get()
            weekday = DAYS.index(self.day_value.get()) + 1
            validate_time(date, time_text, weekday)
        except (ProtocolError, ValueError) as exc:
            messagebox.showerror('Некорректные значения', str(exc), parent=self.root); return
        self.set_button.configure(state='disabled')
        self.notice.set('Ожидаем подтверждение записи от MCP79411…')
        self.worker.request('SET', date, time_text, weekday)

    def poll(self):
        if self.closed:
            return
        while True:
            try:
                kind, value = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == 'connected':
                self.connected = True
                self.connection.set('Соединение подтверждено: PONG, протокол 1')
                self.set_button.configure(state='normal'); self.refresh_button.configure(state='normal')
            elif kind == 'state':
                self.last_state = time.monotonic()
                self.state_text.set(STATUS[value.status])
                if value.valid:
                    year, month, day = value.date.split('-')
                    self.time_text.set(value.time)
                    self.date_text.set(f'{day}/{month}/{year[2:]} · {DAYS[value.weekday - 1]}')
                else:
                    self.time_text.set('--:--:--'); self.date_text.set('--/--/-- · неизвестно')
                self.details.set(f'OLED: {"исправен" if value.oled else "ошибка"} · RTC ошибки: 0x{value.errors:02X} · '
                                 f'RX потеряно: {value.rx_overflow} · UART ошибки: {value.uart_errors}')
            elif kind == 'set_result':
                self.notice.set('Запись подтверждена; показания обновляются.' if value else 'Запись не подтверждена. Проверьте показания и сообщение об ошибке.')
                self.set_button.configure(state='normal' if self.connected else 'disabled')
            elif kind == 'warning':
                self.notice.set(value)
            elif kind in ('error', 'closed'):
                self.connected = False
                self.set_button.configure(state='disabled'); self.refresh_button.configure(state='disabled')
                if kind == 'error':
                    self.notice.set(value)
                self.connection.set('Соединение закрыто')
        if self.connected and self.last_state and time.monotonic() - self.last_state > 2:
            self.state_text.set('Данные устарели — нет свежих показаний')
        self.root.after(50, self.poll)

    def close(self):
        self.closed = True
        if self.worker:
            self.worker.stop()
            self.worker.join(timeout=2)
        self.root.destroy()

def main():
    parser = argparse.ArgumentParser(description='Графический клиент часов SDK-1.1M')
    parser.add_argument('port', help='COM-порт, например COM5; DEMO — без платы')
    parser.add_argument('--log', default=str(ROOT / 'logs' / ('uart-' + datetime.now().strftime('%Y%m%d-%H%M%S') + '.jsonl')))
    args = parser.parse_args()
    root = tk.Tk(); ClockWindow(root, args.port, args.log); root.mainloop()

if __name__ == '__main__':
    main()
