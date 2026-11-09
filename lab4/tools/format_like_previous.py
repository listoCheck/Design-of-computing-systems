"""Make the two deliverables using the retained previous report as template."""
from copy import deepcopy
from hashlib import sha256
from pathlib import Path
from shutil import copyfile
from io import BytesIO

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_TAB_ALIGNMENT
from docx.shared import Cm, Inches, Pt, RGBColor
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / 'lab3/docs/Отчет_ЛР3_3406_2026.docx'
EXPECTED = '2e8f945071ee37352265cd6547a30a845b347fd889bb373ecc8ac05968029296'
OUT3 = ROOT / 'lab3/docs/Отчет_ЛР3_вариант1_новый.docx'
OUT4 = ROOT / 'lab4/docs/Отчет_ЛР4_вариант1.docx'
WORK = ROOT / 'lab4/build/reference'

def diagram(name, title, boxes):
    img = Image.new('RGB', (1400, 1000), 'white')
    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 28)
    bold = ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf', 30)
    draw.text((700, 25), title, font=bold, fill='black', anchor='ma')
    top, height, gap = 105, 125, 30
    for i, lines in enumerate(boxes):
        y = top + i * (height + gap)
        draw.rounded_rectangle((135, y, 1265, y + height), radius=15, outline='black', width=3)
        draw.multiline_text((700, y + height/2), lines, font=font, fill='black', anchor='mm', align='center', spacing=8)
        if i + 1 < len(boxes):
            draw.line((700, y + height, 700, y + height + gap - 8), fill='black', width=3)
            draw.polygon(((692, y + height + gap - 10), (708, y + height + gap - 10), (700, y + height + gap)), fill='black')
    out = WORK / name
    img.save(out)
    return out

def para(d, text, *, heading=False, center=False, code=False):
    p = d.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER if center else WD_ALIGN_PARAGRAPH.LEFT if heading or code else WD_ALIGN_PARAGRAPH.JUSTIFY
    p.paragraph_format.space_after = Pt(6 if not code else 0)
    if not heading and not center and not code:
        p.paragraph_format.first_line_indent = Cm(1.25)
    p.paragraph_format.line_spacing = 1.05 if code else 1.15
    run = p.add_run(text)
    run.font.name = 'Consolas' if code else 'Times New Roman'
    run.font.size = Pt(9 if code else 12)
    run.bold = heading
    run.font.color.rgb = RGBColor(0, 0, 0)
    return p

def page(d, heading):
    d.add_page_break()
    para(d, heading, heading=True)

def figure(d, path):
    p = d.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run().add_picture(str(path), width=Inches(5.8))

def main():
    if sha256(REF.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit('The reference report changed: inspect it again before editing.')
    WORK.mkdir(parents=True, exist_ok=True)
    copyfile(REF, OUT3)
    d = Document(REF)
    shapes = d.inline_shapes
    pin_rid = shapes[0]._inline.graphic.graphicData.pic.blipFill.blip.embed
    pins = WORK / 'previous-pins.png'
    pins.write_bytes(d.part.related_parts[pin_rid].blob)
    # Preserve the complete cover, including its section break, and the final
    # section definition. Remove the old table of contents and report body.
    body = d.element.body
    cover_end = next(p._p for p in d.paragraphs if p.text == 'Санкт-Петербург')
    past_cover = False
    for child in list(body):
        if child is cover_end:
            past_cover = True
        elif past_cover and child.tag.rsplit('}', 1)[-1] != 'sectPr':
            body.remove(child)
    title = next(p for p in d.paragraphs if p.text.strip() == 'Лабораторная работа 3')
    for run in title.runs:
        if '3' in run.text: run.text = run.text.replace('3', '4')
    assert title.text.strip() == 'Лабораторная работа 4'

    para(d, 'Содержание', heading=True, center=True)
    for label, num in [('Задание', 3), ('Используемые контакты', 4), ('Блок схемы', 5),
                       ('Обработка событий', 6), ('Описание работы алгоритма', 7),
                       ('Код программы', 8), ('Проверка и руководство пользователя', 9), ('Вывод', 10)]:
        p = para(d, f'{label}\t{num}', heading=True)
        p.paragraph_format.tab_stops.add_tab_stop(Inches(6.1), WD_TAB_ALIGNMENT.RIGHT)

    page(d, 'Задание')
    para(d, 'Операционные системы реального времени', center=True)
    para(d, 'Разработать на базе FreeRTOS программу часов для стенда SDK-1.1M, сохраняющую функции лабораторной работы 3. Использовать внешние часы MCP79411 и OLED-дисплей, а также совместимую программу для компьютера.')
    para(d, 'Реализовать не менее двух задач, переключаемых планировщиком, и один обработчик прерывания. Обновлять дисплей с частотой не менее 20 кадров в секунду и выводить циклический счётчик от 0 до 99.')
    para(d, 'Сравнить реализацию без операционной системы с реализацией на FreeRTOS, описать преимущества, недостатки и проверку программы.')

    page(d, 'Используемые контакты')
    figure(d, pins)
    para(d, 'Описание контактов', heading=True)
    for line in ['PB8 — I²C1_SCL, AF4, open-drain.', 'PB9 — I²C1_SDA, AF4, open-drain.',
                 'PC6 / PC7 — USART6_TX / USART6_RX, AF8.', 'MCP79411 — семибитовый адрес 0x6F.',
                 'SSD1306 128×64 — семибитовый адрес 0x3C.', 'Шина I²C — 400 кГц; UART — 57600 бит/с, 8N1.']:
        para(d, line)

    page(d, 'Блок схемы')
    para(d, 'Схема организации задач', center=True)
    fig = diagram('lab4-tasks.png', 'Запуск и работа FreeRTOS', [
        'HAL_Init, PLL, 120 МГц\nНастройка UART и I²C',
        'Создание мьютекса I²C и очереди\nСоздание двух задач',
        'Запуск планировщика FreeRTOS',
        'Задача display: изображение и счётчик\nЗадача clock-uart: RTC и команды ПК',
        'USART6 IRQ: приём и передача байтов\nSysTick: время и переключение задач'])
    figure(d, fig)

    page(d, 'Обработка событий')
    para(d, 'Передача показаний часов на дисплей', center=True)
    fig = diagram('lab4-events.png', 'Обновление отображения', [
        'clock-uart: чтение MCP79411\nПолучение состояния часов',
        'Копирование последнего снимка в очередь',
        'display: формирование изображения\nи значения счётчика 00–99',
        'Передача восьми страниц OLED\nКаждая транзакция под мьютексом',
        'После успешного кадра: увеличить счётчик\nи обновить статистику времени'])
    figure(d, fig)

    page(d, 'Описание работы алгоритма')
    for line in [
        'Ядро STM32F427 работает на 120 МГц. FreeRTOS Kernel V10.0.1 использует вытесняющее планирование с периодом системного тика 1 мс. Для дисплея создана задача display с приоритетом 3, для часов и UART — задача clock-uart с приоритетом 2.',
        'Задача часов обслуживает протокол PING, GET и SET, а также раз в 250 мс считывает данные MCP79411. Последний снимок передаётся в очередь длиной один элемент, поэтому дисплей не читает изменяемые данные другой задачи.',
        'Обе задачи используют I²C1. Для каждой транзакции используется мьютекс с наследованием приоритета; это исключает одновременный доступ к контроллеру. Между страницами OLED задача дисплея освобождает процессор на один тик.',
        'Плановый период кадра составляет 40 мс, или 25 кадров/с. Передача 1024 байт только с учётом битов данных и подтверждения занимает около 23 мс при 400 кГц. Реальное время включает дополнительные расходы и должно быть измерено на стенде.',
        'После успешной передачи полного изображения счётчик изменяется по кругу от 00 до 99. Структура display_stats хранит число кадров, частоту, максимальный интервал и ошибки передачи.']:
        para(d, line)

    page(d, 'Код программы')
    para(d, 'Фрагмент lab4/Core/Src/main.c. Полные исходники находятся в репозитории.')
    code = '''if (xTaskCreate(display_task, "display", 768,
                NULL, 3, NULL) != pdPASS) fatal(6);
if (xTaskCreate(application_task, "clock-uart", 1024,
                NULL, 2, NULL) != pdPASS) fatal(7);
vTaskStartScheduler();

/* В задаче дисплея: */
(void)xQueuePeek(snapshot_queue, &snapshot, 0);
oled_render(&display, &snapshot, stats.counter);
bool ok = true;
while (display.page < 8U) {
    if (!oled_service(&display)) { ok = false; break; }
    vTaskDelay(pdMS_TO_TICKS(1));
}
oled_ok = ok;
uint32_t finished = HAL_GetTick();
if (ok)
    frame_stats_complete(&stats, finished,
                         finished - start);
else
    frame_stats_fail(&stats);

void USART6_IRQHandler(void) { uart_irq(); }'''
    para(d, code, code=True)

    page(d, 'Проверка и руководство пользователя')
    for line in [
        'Проект lab4 собран в STM32CubeIDE без ошибок и предупреждений. В эмуляторе выполнено 46 проверок ARM-кода, в том числе RTC, протокола, графического буфера, счётчика и статистики. Три теста клиента предыдущей работы также прошли успешно.',
        'В STM32CubeIDE выбрать File → Import → Existing Projects into Workspace и указать папку lab4. Выполнить сборку, подключить стенд через DBG USB и загрузить ELF через встроенный FTDI/JTAG программатор.',
        'Запустить клиент: python lab3/client/app.py COMx. После PONG проверить данные на дисплее и в окне программы, затем установить дату и время. Через Expressions можно наблюдать display_stats и lab4_fault.',
        'При последней проверке стенд не был обнаружен. Реальная работа OLED, RTC и UART, а также соблюдение 20 кадров/с под нагрузкой не подтверждены; эти пункты необходимо проверить на оборудовании.']:
        para(d, line)

    page(d, 'Вывод')
    for line in [
        'Разработана многозадачная версия часов на FreeRTOS. Обновление дисплея и работа с RTC/UART вынесены в отдельные задачи; доступ к общей шине защищён мьютексом.',
        'В варианте без ОС один цикл последовательно обслуживает процессы, что проще и требует меньше памяти, но длительная операция задерживает остальные. FreeRTOS разделяет процессы и позволяет задавать приоритеты; при этом возрастает расход памяти и требуется правильная синхронизация.',
        'Проект собран и проверен программными тестами. Проверка на физическом стенде и измерение частоты обновления дисплея остаются необходимыми.']:
        para(d, line)
    d.save(OUT4)
    assert sha256(REF.read_bytes()).hexdigest() == EXPECTED
    print(OUT3); print(OUT4)

if __name__ == '__main__': main()
