"""Execute the real ARM unit tests in Unicorn; no target board is required."""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / '.lab1-tools'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR
spec = importlib.util.spec_from_file_location('build', ROOT / 'tools/build.py')
build = importlib.util.module_from_spec(spec); spec.loader.exec_module(build)

def main():
    elf = build.compile_firmware(True); tools = build.toolchain(); suffix = '.exe' if os.name == 'nt' else ''
    nm = subprocess.check_output([str(tools / ('arm-none-eabi-nm' + suffix)), '-n', str(elf)], text=True)
    symbols = {x[2]: int(x[0], 16) for row in nm.splitlines() if len(x := row.split()) == 3}
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 1024 * 1024); uc.mem_map(0x20000000, 192 * 1024)
    uc.mem_map(0x40000000, 0x30000); uc.mem_map(0xE000E000, 0x2000)
    binary = elf.with_suffix('.bin').read_bytes(); uc.mem_write(0x08000000, binary)
    objdump = subprocess.check_output([str(tools / ('arm-none-eabi-objdump' + suffix)), '-h', str(elf)], text=True)
    for row in objdump.splitlines():
        x = row.split()
        if len(x) >= 6 and x[1] == '.data' and int(x[2], 16):
            uc.mem_write(int(x[3], 16), bytes(uc.mem_read(int(x[4], 16), int(x[2], 16))))
    uc.reg_write(UC_ARM_REG_SP, 0x2002FFF0); uc.reg_write(UC_ARM_REG_LR, 0x080FFFF1)
    done = False
    def wrote(emu, access, address, size, value, unused):
        nonlocal done
        if address == symbols['test_done'] and value: done = True; emu.emu_stop()
    uc.hook_add(UC_HOOK_MEM_WRITE, wrote)
    uc.emu_start(symbols['test_entry'] | 1, 0x080FFFF0, count=2000000)
    read = lambda name: struct.unpack('<I', uc.mem_read(symbols[name], 4))[0]
    if not done or read('test_failure'): raise SystemExit(f"FAIL assertion line {read('test_failure')}; checks={read('test_checks')}; crc={read('test_crc_value'):04X}")
    report = f"PASS: {read('test_checks')} ARM assertions (RTC, protocol, OLED, application).\n"
    (ROOT / 'build' / 'test-results.txt').write_text(report, encoding='utf-8'); print(report, end='')
if __name__ == '__main__': main()
