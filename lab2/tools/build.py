"""Build both UART backends with CubeIDE's ARM GCC; no network required."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lab1_build', ROOT.parent / 'lab1/tools/build.py')
lab1_build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab1_build)
toolchain = lab1_build.toolchain

def build(backend, rx_size, tx_size, polling):
    bin_dir = toolchain()
    suffix = '.exe' if os.name == 'nt' else ''
    gcc = bin_dir / ('arm-none-eabi-gcc' + suffix)
    out = ROOT / 'build' / backend
    out.mkdir(parents=True, exist_ok=True)
    common = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-std=c11', '-g3', '-Og',
              '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror',
              '-DSTM32F427xx', '-DUSE_HAL_DRIVER', f'-DUART_BACKEND_REG={int(backend == "register")}',
              f'-DUART_RX_CAPACITY={rx_size}', f'-DUART_TX_CAPACITY={tx_size}',
              f'-DUART_START_INTERRUPTS={int(not polling)}', '-ICore/Inc',
              '-IDrivers/CMSIS/Include', '-IDrivers/CMSIS/Device/ST/STM32F4xx/Include',
              '-IDrivers/STM32F4xx_HAL_Driver/Inc']
    sources = sorted((ROOT / 'Core/Src').glob('*.c')) + sorted((ROOT / 'Core/Startup').glob('*.s'))
    modules = ['', '_cortex', '_rcc', '_rcc_ex', '_pwr', '_pwr_ex', '_flash', '_flash_ex']
    if backend == 'hal':
        modules += ['_uart', '_dma', '_gpio']
    sources += [ROOT / f'Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal{m}.c' for m in modules]
    objects = []
    def run(args):
        subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
    for source in sources:
        obj = out / (source.stem + '.o')
        vendor = ['-Wno-unused-parameter'] if 'Drivers' in source.parts else []
        run([gcc, *common, *vendor, '-c', source, '-o', obj])
        objects.append(obj)
    elf = out / 'lab2.elf'
    run([gcc, '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', *objects,
         '-TSTM32F427VGTX_FLASH.ld', '--specs=nano.specs', '--specs=nosys.specs', '-static',
         '-Wl,--gc-sections', f'-Wl,-Map={out / "lab2.map"}', '-lc', '-lm', '-o', elf])
    run([bin_dir / ('arm-none-eabi-objcopy' + suffix), '-O', 'binary', elf, out / 'lab2.bin'])
    run([bin_dir / ('arm-none-eabi-size' + suffix), elf])
    if backend == 'register':
        symbols = subprocess.check_output([str(bin_dir / ('arm-none-eabi-nm' + suffix)), str(elf)], text=True)
        if any('HAL_UART_' in line or 'LL_USART_' in line for line in symbols.splitlines()):
            raise SystemExit('Register firmware contains forbidden UART library symbols')
    print('Built', elf)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--backend', choices=['hal', 'register', 'all'], default='all')
    parser.add_argument('--rx-size', type=int, default=256)
    parser.add_argument('--tx-size', type=int, default=256)
    parser.add_argument('--polling', action='store_true')
    args = parser.parse_args()
    if args.rx_size < 1 or args.tx_size < 1:
        parser.error('queue sizes must be positive')
    for backend in (['hal', 'register'] if args.backend == 'all' else [args.backend]):
        build(backend, args.rx_size, args.tx_size, args.polling)

if __name__ == '__main__':
    main()
