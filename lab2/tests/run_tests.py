"""Run real ARM C code for both UART backends in Unicorn, without a board.

Minimal MMIO model: USART SR/DR side effects and NVIC enable/disable.
Not a physical UART timing/electrical model, and not the part-2 submission.
"""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / '.lab1-tools'))
try:
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
    from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR
except ImportError:
    raise SystemExit('Install test dependency: python -m pip install unicorn')
spec = importlib.util.spec_from_file_location('lab2_build', ROOT / 'tools/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

def run_backend(backend):
    tools = build.toolchain()
    suffix = '.exe' if os.name == 'nt' else ''
    out = ROOT / 'build' / ('test-' + backend)
    out.mkdir(parents=True, exist_ok=True)
    common = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-std=c11', '-Og', '-g',
              '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror',
              '-DSTM32F427xx', '-DUSE_HAL_DRIVER', f'-DUART_BACKEND_REG={int(backend == "register")}',
              '-DUART_RX_CAPACITY=8', '-DUART_TX_CAPACITY=8', '-ICore/Inc',
              '-IDrivers/CMSIS/Include', '-IDrivers/CMSIS/Device/ST/STM32F4xx/Include',
              '-IDrivers/STM32F4xx_HAL_Driver/Inc']
    sources = [ROOT / 'Core/Src' / (s + '.c') for s in
               ['button', 'queue', 'traffic', 'console', 'uart_driver', 'uart_hal', 'uart_register', 'system_stm32f4xx']]
    sources += [ROOT / 'tests/test_lab2.c']
    modules = ['', '_rcc', '_cortex']
    if backend == 'hal':
        modules += ['_uart', '_dma', '_gpio']
    sources += [ROOT / f'Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal{m}.c' for m in modules]
    def run(args):
        subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
    objects = []
    for source in sources:
        obj = out / (source.stem + '.o')
        flags = ['-Wno-unused-parameter'] if 'Drivers' in source.parts else []
        run([tools / ('arm-none-eabi-gcc' + suffix), *common, *flags, '-c', source, '-o', obj])
        objects.append(obj)
    elf = out / 'tests.elf'
    run([tools / ('arm-none-eabi-gcc' + suffix), '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft',
         *objects, '-nostartfiles', '-Ttests/test.ld', '--specs=nano.specs', '--specs=nosys.specs',
         '-Wl,--gc-sections', '-Wl,-e,test_entry', '-lc', '-o', elf])
    run([tools / ('arm-none-eabi-objcopy' + suffix), '-O', 'binary', elf, out / 'tests.bin'])
    nm = subprocess.check_output([str(tools / ('arm-none-eabi-nm' + suffix)), '-n', str(elf)], text=True)
    symbols = {f[2]: int(f[0], 16) for line in nm.splitlines() if len(f := line.split()) == 3}
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 1024 * 1024)
    uc.mem_map(0x20000000, 192 * 1024)
    uc.mem_map(0x40000000, 0x30000)
    uc.mem_map(0xE000E000, 0x2000)
    uc.mem_write(0x08000000, (out / 'tests.bin').read_bytes())
    # Initialize .data from ELF load image, as startup normally does.
    objdump = subprocess.check_output([str(tools / ('arm-none-eabi-objdump' + suffix)), '-h', str(elf)], text=True)
    for line in objdump.splitlines():
        f = line.split()
        if len(f) >= 6 and f[1] == '.data':
            size, vma, lma = (int(f[i], 16) for i in (2, 3, 4))
            if size:
                uc.mem_write(vma, bytes(uc.mem_read(lma, size)))
    uc.reg_write(UC_ARM_REG_SP, 0x2002FFF0)
    uc.reg_write(UC_ARM_REG_LR, 0x080FFFF1)
    def read32(address):
        return struct.unpack('<I', uc.mem_read(address, 4))[0]
    def write32(address, value):
        uc.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))
    sr, dr = 0x40011400, 0x40011404
    sent = []
    completed = False
    def on_read(emu, access, address, size, value, user):
        if address == dr:
            write32(sr, read32(sr) & ~(0x20 | 0x0F))
    def on_write(emu, access, address, size, value, user):
        nonlocal completed
        if address == symbols['test_done'] and value:
            completed = True; emu.emu_stop()
        # Test injection writes DR followed by SR; TX write is captured too.
        if address == dr:
            sent.append(value & 255)
            write32(sr, (read32(sr) & ~0x20) | 0xC0)
        if 0xE000E180 <= address < 0xE000E1A0:  # ICER write-one-to-disable
            write32(address - 0x80, read32(address - 0x80) & ~value)
    uc.hook_add(UC_HOOK_MEM_READ, on_read)
    uc.hook_add(UC_HOOK_MEM_WRITE, on_write)
    uc.emu_start(symbols['test_entry'] | 1, 0x080FFFF0, count=10000000)
    failure, checks = read32(symbols['test_failure']), read32(symbols['test_checks'])
    if failure or not completed:
        raise SystemExit(f'FAIL {backend}: test_lab2.c:{failure}, {checks} checks, completed={completed}')
    # Ordered physical DR writes prove queued transmission was preserved.
    stream = bytes(sent)
    if b'AB' not in stream or b'XYZ' not in stream or b'ffffffff' not in stream:
        raise SystemExit(f'FAIL {backend}: queued TX byte order')
    report = f'PASS {backend}: {checks} ARM C assertions; UART TX order checked.\n'
    (out / 'results.txt').write_text(report, encoding='utf-8')
    print(report.strip())
    return report

if __name__ == '__main__':
    reports = [run_backend(backend) for backend in ['hal', 'register']]
    (ROOT / 'build/test-results.txt').write_text(''.join(reports), encoding='utf-8')
