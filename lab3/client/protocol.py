"""Version 1: ASCII $body*CRC16-CCITT-FALSE\n; no user command-line input."""
from dataclasses import dataclass
from datetime import datetime
import binascii
import re

MAX_FRAME = 192
STATES = {'STOPPED', 'RUNNING', 'STARTING', 'ERROR'}

class ProtocolError(ValueError):
    pass

def encode(fields):
    body = '|'.join(str(x) for x in fields).encode('ascii')
    if len(body) + 8 > MAX_FRAME or any(c < 32 or c > 126 or c in b'$*' for c in body):
        raise ProtocolError('Invalid frame body')
    return b'$' + body + f'*{binascii.crc_hqx(body, 0xFFFF):04X}\n'.encode('ascii')

def decode(frame):
    raw = frame[:-1] if frame.endswith(b'\n') else frame
    if len(raw) < 8 or len(raw) >= MAX_FRAME or raw[:1] != b'$' or raw[-5:-4] != b'*':
        raise ProtocolError('Frame format')
    body, checksum = raw[1:-5], raw[-4:]
    if not re.fullmatch(b'[0-9A-F]{4}', checksum) or any(c < 32 or c > 126 or c in b'$*' for c in body):
        raise ProtocolError('Frame characters')
    if binascii.crc_hqx(body, 0xFFFF) != int(checksum, 16):
        raise ProtocolError('CRC mismatch')
    fields = body.decode('ascii').split('|')
    if len(fields) < 2 or not fields[0].isdigit() or not 0 <= int(fields[0]) <= 65535:
        raise ProtocolError('Sequence number')
    return fields

class Decoder:
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        results = []
        for byte in data:
            if byte == ord('$'):
                self.buffer.clear()
            if not self.buffer and byte != ord('$'):
                continue
            self.buffer.append(byte)
            if byte == 10:
                raw = bytes(self.buffer)
                self.buffer.clear()
                try:
                    results.append((raw, decode(raw), None))
                except ProtocolError as exc:
                    results.append((raw, None, str(exc)))
            elif len(self.buffer) >= MAX_FRAME:
                results.append((bytes(self.buffer), None, 'Frame too long'))
                self.buffer.clear()
        return results

def validate_time(date, time, weekday):
    if not re.fullmatch(r'20\d\d-\d\d-\d\d', date) or not re.fullmatch(r'\d\d:\d\d:\d\d', time):
        raise ProtocolError('Год 2000–2099; дата ГГГГ-ММ-ДД; время ЧЧ:ММ:СС')
    try:
        moment = datetime.strptime(date + ' ' + time, '%Y-%m-%d %H:%M:%S')
        day = int(weekday)
    except ValueError as exc:
        raise ProtocolError('Некорректная дата, время или день недели') from exc
    if not 1 <= day <= 7:
        raise ProtocolError('День недели: 1–7')
    return moment, day

@dataclass(frozen=True)
class Snapshot:
    date: str
    time: str
    weekday: int
    status: str
    valid: bool
    errors: int
    oled: bool
    rx_overflow: int
    uart_errors: int

    @classmethod
    def parse(cls, fields):
        if len(fields) != 11 or fields[1] != 'STATE' or fields[5] not in STATES:
            raise ProtocolError('STATE format')
        try:
            numbers = [int(fields[i]) for i in [4, 6, 7, 8, 9, 10]]
        except ValueError as exc:
            raise ProtocolError('STATE numbers') from exc
        weekday, valid, errors, oled, overflow, uart_errors = numbers
        if valid not in (0, 1) or oled not in (0, 1) or any(n < 0 or n > 0xFFFFFFFF for n in [errors, overflow, uart_errors]):
            raise ProtocolError('STATE range')
        if valid:
            validate_time(fields[2], fields[3], weekday)
        elif fields[2] != '0000-00-00' or fields[3] != '00:00:00' or weekday != 0:
            raise ProtocolError('Invalid sample must have empty date/time')
        return cls(fields[2], fields[3], weekday, fields[5], bool(valid), errors, bool(oled), overflow, uart_errors)
