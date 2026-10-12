"""Build MCP79411/OLED clock firmware with CubeIDE's ARM GCC."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lab1_build', ROOT.parent / 'lab1/tools/build.py')
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
toolchain = base.toolchain

def compile_firmware(test=False):
    bin_dir = toolchain()
    suffix = '.exe' if os.name == 'nt' else ''
    gcc = bin_dir / ('arm-none-eabi-gcc' + suffix)
    target = 'tests' if test else 'lab3'
    out = ROOT / 'build' / target
    out.mkdir(parents=True, exist_ok=True)
    flags = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-std=c11', '-g3', '-Og',
             '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror',
             '-DSTM32F427xx', '-DUSE_HAL_DRIVER', '-ICore/Inc', '-IDrivers/CMSIS/Include',
             '-IDrivers/CMSIS/Device/ST/STM32F4xx/Include', '-IDrivers/STM32F4xx_HAL_Driver/Inc']
    sources = sorted((ROOT / 'Core/Src').glob('*.c'))
    if test:
        sources = [s for s in sources if s.name not in ('main.c', 'bus_hal.c')] + [ROOT / 'tests/test_lab3.c']
    else:
        sources += sorted((ROOT / 'Core/Startup').glob('*.s'))
    modules = ['', '_cortex', '_rcc', '_rcc_ex', '_pwr', '_pwr_ex', '_flash', '_flash_ex',
               '_gpio', '_dma', '_uart', '_tim', '_tim_ex', '_i2c']
    sources += [ROOT / f'Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal{m}.c' for m in modules]
    objects = []
    def run(args):
        subprocess.run([str(a) for a in args], cwd=ROOT, check=True)
    for source in sources:
        obj = out / (source.stem + '.o')
        vendor = ['-Wno-unused-parameter'] if 'Drivers' in source.parts else []
        run([gcc, *flags, *vendor, '-c', source, '-o', obj])
        objects.append(obj)
    elf = out / (target + '.elf')
    link = ['-nostartfiles', '-Ttests/test.ld', '-Wl,-e,test_entry'] if test else ['-TSTM32F427VGTX_FLASH.ld']
    run([gcc, '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', *objects, *link,
         '--specs=nano.specs', '--specs=nosys.specs', '-static', '-Wl,--gc-sections',
         f'-Wl,-Map={out / (target + ".map")}', '-lc', '-lm', '-o', elf])
    run([bin_dir / ('arm-none-eabi-objcopy' + suffix), '-O', 'binary', elf, out / (target + '.bin')])
    run([bin_dir / ('arm-none-eabi-size' + suffix), elf])
    symbols = subprocess.check_output([str(bin_dir / ('arm-none-eabi-nm' + suffix)), str(elf)], text=True)
    if ' HAL_Delay' in symbols:
        raise SystemExit('Unexpected HAL_Delay linked into application')
    print('Built', elf)
    return elf

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--test', action='store_true')
    args = parser.parse_args()
    compile_firmware(args.test)
