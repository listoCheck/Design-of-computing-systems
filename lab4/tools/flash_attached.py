"""Program lab4 using an already running SDK OpenOCD telnet server."""
import socket
from pathlib import Path

root = Path(__file__).resolve().parents[1]
elf = (root / 'Debug/lab4.elf').resolve()
if not elf.is_file():
    raise SystemExit(f'Missing firmware: {elf}')
path = elf.as_posix()
if any(c in path for c in '{}\r\n'):
    raise SystemExit('Invalid ELF path')
with socket.create_connection(('127.0.0.1', 4444), timeout=5) as s:
    s.settimeout(30)
    def read_prompt():
        data = b''
        while not data.endswith(b'> '):
            part = s.recv(8192)
            if not part:
                raise ConnectionError('OpenOCD closed the connection')
            data += part
        return data
    read_prompt()
    s.sendall(('program {' + path + '} verify reset\n').encode('ascii'))
    result = read_prompt()
    print(result.decode('utf-8', errors='replace'))
    if b'Verified OK' not in result:
        raise SystemExit('OpenOCD did not confirm successful verification')
