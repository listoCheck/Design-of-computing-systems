"""Serial thread and lossless JSONL UART journal; Tk is touched only by GUI thread."""
from datetime import datetime, timezone
from pathlib import Path
import json
import queue
import threading
import time
from protocol import Decoder, Snapshot, ProtocolError, encode

class Journal:
    def __init__(self, path, demo=False):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.file = self.path.open('a', encoding='utf-8')
        self.demo = demo

    def record(self, direction, kind, data):
        entry = {'timestamp': datetime.now(timezone.utc).isoformat(timespec='milliseconds'),
                 'direction': direction, 'kind': kind, 'demo': self.demo,
                 'hex': data.hex(), 'text': data.decode('ascii', errors='backslashreplace')}
        self.file.write(json.dumps(entry, ensure_ascii=False) + '\n')
        self.file.flush()

    def close(self):
        self.file.close()

class SerialWorker(threading.Thread):
    def __init__(self, port, log_path, events, factory, demo=False):
        super().__init__(daemon=True)
        self.port, self.log_path, self.events, self.factory, self.demo = port, log_path, events, factory, demo
        self.commands = queue.Queue()
        self.stop_event = threading.Event()
        self.next_id = 1
        self.pending = {}
        self.connected = False

    def request(self, *fields):
        self.commands.put(fields)

    def stop(self):
        self.stop_event.set()

    def run(self):
        channel = journal = None
        try:
            journal = Journal(self.log_path, self.demo)
            channel = self.factory(self.port)
            self.events.put(('opened', self.port))
            def send(command):
                seq = self.next_id
                self.next_id = self.next_id % 65535 + 1
                frame = encode((seq, *command))
                sent = 0
                while sent < len(frame):
                    count = channel.write(frame[sent:])
                    if not count:
                        raise OSError('COM-порт не принял данные')
                    journal.record('TX', 'BYTES', frame[sent:sent + count])
                    sent += count
                journal.record('TX', 'FRAME', frame)
                self.pending[seq] = (command[0], time.monotonic())
            send(('PING',))  # FIRST write immediately after opening the COM port.
            decoder = Decoder()
            next_get = time.monotonic() + 1
            while not self.stop_event.is_set():
                try:
                    command = self.commands.get_nowait()
                except queue.Empty:
                    command = None
                if command:
                    if self.connected:
                        send(command)
                    else:
                        self.events.put(('warning', 'Соединение ещё не подтверждено'))
                data = channel.read(256)
                if data:
                    journal.record('RX', 'BYTES', data)
                    for raw, fields, error in decoder.feed(data):
                        journal.record('RX', 'FRAME' if fields else 'BAD_FRAME', raw)
                        if error:
                            self.events.put(('warning', 'Ошибка протокола: ' + error)); continue
                        try:
                            seq, name = int(fields[0]), fields[1]
                            pending = self.pending.get(seq)
                            if name == 'PONG' and fields[2:] == ['1'] and pending and pending[0] == 'PING':
                                self.pending.pop(seq)
                                self.connected = True
                                self.events.put(('connected', self.port)); send(('GET',))
                            elif name == 'STATE':
                                snapshot = Snapshot.parse(fields)
                                self.events.put(('state', snapshot))
                                if pending and pending[0] == 'GET':
                                    self.pending.pop(seq)
                            elif name == 'OK' and fields[2:] == ['SET'] and pending and pending[0] == 'SET':
                                self.pending.pop(seq)
                                self.events.put(('set_result', True)); send(('GET',))
                            elif name == 'ERR' and len(fields) == 3:
                                if pending:
                                    self.pending.pop(seq)
                                    if pending[0] == 'SET':
                                        self.events.put(('set_result', False))
                                self.events.put(('warning', 'Стенд: ' + fields[2]))
                            else:
                                raise ProtocolError('Неожиданный ответ: ' + name)
                        except (ProtocolError, ValueError) as exc:
                            self.events.put(('warning', str(exc)))
                now = time.monotonic()
                for seq, (command, since) in list(self.pending.items()):
                    if now - since <= (5 if command == 'SET' else 3):
                        continue
                    self.pending.pop(seq)
                    if command == 'PING':
                        raise OSError('Нет ответа PONG: проверьте COM-порт и прошивку')
                    if command == 'SET':
                        self.events.put(('set_result', False))
                    self.events.put(('warning', f'Тайм-аут {command}; результат команды не подтверждён'))
                if self.connected and now >= next_get:
                    if not any(command == 'GET' for command, _ in self.pending.values()):
                        send(('GET',))
                    next_get = now + 1
        except Exception as exc:
            self.events.put(('error', str(exc)))
        finally:
            if channel:
                channel.close()
            if journal:
                journal.close()
            self.events.put(('closed', None))
