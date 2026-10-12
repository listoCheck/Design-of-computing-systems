"""Desktop protocol/client checks.  They use DEMO transport, never a real COM port."""
import json
from pathlib import Path
import queue
import sys
import tempfile
import time
import unittest

CLIENT = Path(__file__).resolve().parents[1] / 'client'
sys.path.insert(0, str(CLIENT))
from protocol import Decoder, ProtocolError, Snapshot, decode, encode, validate_time
from demo import DemoSerial
from transport import SerialWorker


class ProtocolTests(unittest.TestCase):
    def test_crc_fragmentation_and_validation(self):
        line = encode((7, 'SET', '2026-10-12', '12:34:56', 1))
        self.assertEqual(decode(line)[1], 'SET')
        decoder = Decoder()
        self.assertEqual(decoder.feed(line[:5]), [])
        result = decoder.feed(line[5:])
        self.assertEqual(result[0][1][0], '7')
        broken = bytearray(line); broken[3] ^= 1
        self.assertIsNotNone(Decoder().feed(broken)[0][2])
        self.assertEqual(validate_time('2024-02-29', '23:59:59', '7')[1], 7)
        with self.assertRaises(ProtocolError): validate_time('2025-02-29', '12:00:00', '1')

    def test_state_shape(self):
        state = Snapshot.parse(['0', 'STATE', '2026-10-12', '12:34:56', '1', 'RUNNING', '1', '0', '1', '0', '0'])
        self.assertTrue(state.valid)
        with self.assertRaises(ProtocolError):
            Snapshot.parse(['0', 'STATE', '0000-00-00', '00:00:00', '1', 'ERROR', '0', '0', '0', '0', '0'])


class WorkerTests(unittest.TestCase):
    def test_ping_is_first_and_set_is_journaled(self):
        events = queue.Queue()
        with tempfile.TemporaryDirectory() as directory:
            journal = Path(directory) / 'uart.jsonl'
            worker = SerialWorker('DEMO', journal, events, lambda port: DemoSerial(port), demo=True)
            worker.start(); connected = state = False
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline and not (connected and state):
                try: event = events.get(timeout=.1)
                except queue.Empty: continue
                connected |= event[0] == 'connected'; state |= event[0] == 'state'
            self.assertTrue(connected); self.assertTrue(state)
            worker.request('SET', '2026-10-12', '12:34:56', '1')
            deadline = time.monotonic() + 3; set_ok = False
            while time.monotonic() < deadline and not set_ok:
                try: set_ok = events.get(timeout=.1) == ('set_result', True)
                except queue.Empty: pass
            worker.stop(); worker.join(2)
            self.assertTrue(set_ok); self.assertFalse(worker.is_alive())
            entries = [json.loads(row) for row in journal.read_text(encoding='utf-8').splitlines()]
            frames = [entry for entry in entries if entry['direction'] == 'TX' and entry['kind'] == 'FRAME']
            self.assertIn('PING', frames[0]['text'])
            self.assertTrue(all(entry['demo'] for entry in entries))

if __name__ == '__main__': unittest.main()
