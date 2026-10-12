"""Explicit demo transport for GUI testing without a board; not hardware validation."""
from datetime import datetime, timedelta
import time
from protocol import Decoder, encode, validate_time

class DemoSerial:
    def __init__(self, port='DEMO'):
        self.origin = datetime.now().replace(microsecond=0)
        self.weekday = self.origin.isoweekday()
        self.since = time.monotonic()
        self.next_sample = self.since
        self.decoder = Decoder()
        self.buffer = bytearray()
        self.closed = False

    def state(self, seq):
        current = self.origin + timedelta(seconds=int(time.monotonic() - self.since))
        days = (current.date() - self.origin.date()).days
        return encode((seq, 'STATE', current.strftime('%Y-%m-%d'), current.strftime('%H:%M:%S'),
                       (self.weekday - 1 + days) % 7 + 1, 'RUNNING', 1, 0, 1, 0, 0))

    def write(self, data):
        for raw, fields, error in self.decoder.feed(data):
            if error:
                self.buffer.extend(encode((0, 'ERR', 'CRC'))); continue
            seq = int(fields[0])
            if fields[1:] == ['PING']:
                self.buffer.extend(encode((seq, 'PONG', 1)))
            elif fields[1:] == ['GET']:
                self.buffer.extend(self.state(seq))
            elif len(fields) == 5 and fields[1] == 'SET':
                try:
                    self.origin, self.weekday = validate_time(*fields[2:])
                    self.since = time.monotonic()
                    self.buffer.extend(encode((seq, 'OK', 'SET')) + self.state(0))
                except ValueError:
                    self.buffer.extend(encode((seq, 'ERR', 'VALUE')))
            else:
                self.buffer.extend(encode((seq, 'ERR', 'COMMAND')))
        return len(data)

    def read(self, size):
        if self.closed:
            raise OSError('Demo closed')
        now = time.monotonic()
        if now >= self.next_sample:
            self.buffer.extend(self.state(0)); self.next_sample = now + .25
        if not self.buffer:
            time.sleep(.02)
        data = bytes(self.buffer[:size]); del self.buffer[:size]; return data

    def close(self):
        self.closed = True
