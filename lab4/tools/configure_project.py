"""Update copied CubeIDE metadata for this project; run once after copying lab3."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
for path in (root / '.project', root / '.cproject'):
    text = path.read_text(encoding='utf-8').replace('lab3', 'lab4')
    if path.name == '.cproject':
        text = text.replace('<listOptionValue builtIn="false" value="../Core/Inc"/>',
            '<listOptionValue builtIn="false" value="../Core/Inc"/>'
            '<listOptionValue builtIn="false" value="../FreeRTOS/Source/include"/>'
            '<listOptionValue builtIn="false" value="../FreeRTOS/Source/portable/GCC/ARM_CM4F"/>')
        text = text.replace('<entry flags="VALUE_WORKSPACE_PATH|RESOLVED" kind="sourcePath" name="Drivers"/>',
            '<entry flags="VALUE_WORKSPACE_PATH|RESOLVED" kind="sourcePath" name="Drivers"/>'
            '<entry flags="VALUE_WORKSPACE_PATH|RESOLVED" kind="sourcePath" name="FreeRTOS"/>')
    path.write_text(text, encoding='utf-8')
