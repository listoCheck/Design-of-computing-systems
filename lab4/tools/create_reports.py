"""Create the lab 3 and 4 reports from the documented implementation."""
from pathlib import Path
import sys
from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = Path(__file__).resolve().parents[2]

def base(number, title):
    d = Document()
    sec = d.sections[0]
    sec.top_margin, sec.bottom_margin = Cm(2), Cm(2)
    sec.left_margin, sec.right_margin = Cm(2.5), Cm(2)
    normal = d.styles['Normal']
    normal.font.name, normal.font.size = 'Times New Roman', Pt(12)
    normal.paragraph_format.line_spacing = 1.15
    normal.paragraph_format.space_after = Pt(5)
    for name in ('Title', 'Heading 1', 'Heading 2'):
        st = d.styles[name]
        st.font.name, st.font.color.rgb = 'Times New Roman', RGBColor(0, 0, 0)
        st.font.size = Pt(16 if name == 'Title' else 14 if name == 'Heading 1' else 12)
        st.font.bold = True
        fonts = st.element.get_or_add_rPr().rFonts
        for attr in ('asciiTheme', 'hAnsiTheme', 'eastAsiaTheme', 'cstheme'):
            fonts.attrib.pop(qn('w:' + attr), None)
        fonts.set(qn('w:ascii'), 'Times New Roman')
        fonts.set(qn('w:hAnsi'), 'Times New Roman')
        st.paragraph_format.space_before = Pt(10)
        st.paragraph_format.space_after = Pt(5)
    borders = OxmlElement('w:pBdr')
    bottom = OxmlElement('w:bottom'); bottom.set(qn('w:val'), 'nil'); borders.append(bottom)
    d.styles['Title'].element.get_or_add_pPr().append(borders)
    for text in ('Федеральное государственное автономное образовательное учреждение высшего образования',
                 'Национальный исследовательский университет ИТМО'):
        p = d.add_paragraph(text); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    for _ in range(5): d.add_paragraph('')
    p = d.add_paragraph('Отчёт по лабораторной работе ' + str(number), style='Title')
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p = d.add_paragraph(title); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p = d.add_paragraph('Дисциплина: Проектирование вычислительных систем\nВариант 1 — Часы')
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    for _ in range(4): d.add_paragraph('')
    p = d.add_paragraph('Выполнил(и): __________________________\nГруппа: ______________________________\nПреподаватель: ________________________')
    p.paragraph_format.left_indent = Cm(8)
    for _ in range(4): d.add_paragraph('')
    p = d.add_paragraph('Санкт-Петербург, 2026'); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    d.add_page_break()
    return d

def h(d, title): d.add_heading(title, level=1)
def p(d, text): d.add_paragraph(text)
def bullets(d, items):
    for item in items: d.add_paragraph(item, style='List Bullet')
def table(d, headers, rows, widths=None):
    t = d.add_table(rows=1, cols=len(headers))
    t.style = 'Table Grid'; t.alignment = WD_TABLE_ALIGNMENT.CENTER
    t.autofit = False
    for i, text in enumerate(headers):
        c = t.rows[0].cells[i]; c.text = text
        if widths: c.width = Cm(widths[i])
        shade = OxmlElement('w:shd'); shade.set(qn('w:fill'), 'E5E7EB'); c._tc.get_or_add_tcPr().append(shade)
        for run in c.paragraphs[0].runs: run.bold = True
    for row in rows:
        cells = t.add_row().cells
        for i, text in enumerate(row):
            cells[i].text = text
            if widths: cells[i].width = Cm(widths[i])
    for row in t.rows:
        for c in row.cells:
            c.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for para in c.paragraphs:
                para.paragraph_format.space_after = Pt(3)
                for run in para.runs: run.font.size = Pt(10.5)
    d.add_paragraph('')

def sources(d):
    h(d, 'Использованные источники')
    for s in [
        'STMicroelectronics. RM0090: STM32F405/407/427/429 reference manual. https://www.st.com/resource/en/reference_manual/dm00031020.pdf',
        'Microchip. MCP79410/11/12 Battery-Backed I2C RTCC, DS20002266. https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/MCP79410-MCP79411-MCP79412-Battery-Backed-I2C-RTCC-DS20002266.pdf',
        'Solomon Systech. SSD1306 OLED/PLED Segment/Common Driver datasheet. https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf',
        'SDK-1.1M. Схема и примеры производителя. https://github.com/lmtspbru/SDK-1.1M',
    ]: p(d, s)

def lab3():
    d = base(3, 'OLED дисплей и устройства I2C')
    h(d, 'Цель работы')
    p(d, 'Изучить интерфейс I²C, программирование дисплейного модуля, таймеров и обмена по UART. Разработать часы на базе стенда SDK-1.1MC.427 и клиентскую программу для компьютера.')
    h(d, 'Задание')
    p(d, 'Вариант 1 «Часы»: использовать микросхему MCP79411, отображать время, дату, день недели и состояние часов на OLED и в программе ПК. Предусмотреть установку даты и времени с компьютера, команду проверки связи и журнал обмена UART с временными метками.')
    h(d, 'Аппаратная конфигурация')
    table(d, ('Устройство', 'Подключение и параметры'), [
        ('Микроконтроллер', 'STM32F427; системная частота 120 МГц, HSE 25 МГц, PLL.'),
        ('RTC MCP79411', 'I²C1: PB8 — SCL, PB9 — SDA; адрес 0x6F, частота 400 кГц.'),
        ('OLED SSD1306', 'Общая шина I²C1; адрес 0x3C, разрешение 128 × 64.'),
        ('UART', 'USART6: PC6 — TX, PC7 — RX; 57600 бит/с, 8N1.'),
        ('Таймер', 'TIM6: запрос обновления каждые 250 мс.')], [4, 11.5])
    h(d, 'Программная реализация')
    p(d, 'Прошивка организована как кооперативный главный цикл. Драйвер RTC считывает регистры MCP79411, преобразует BCD и контролирует состояние генератора и ошибки питания. Согласованность чтения проверяется повторным чтением секунд. Дисплей показывает время в формате ЧЧ:ММ:СС, дату ДД/ММ/ГГ, день недели и состояние часов.')
    p(d, 'Для установки времени обработчик команды останавливает генератор, ожидает прекращения работы, записывает семь регистров и сверяет прочитанные значения. После успешной проверки генератор запускается; ошибка записи передаётся клиенту. Начальные значения часов при старте программы не перезаписываются.')
    p(d, 'Оба устройства используют общий контроллер I²C в режиме опроса. Обращения выполняются последовательно из основного цикла; обработчик таймера только устанавливает флаг. Буфер OLED отправляется страницами, по 128 байт на страницу.')
    h(d, 'Протокол и клиентская программа')
    p(d, 'Клиент на Python с графическим интерфейсом Tkinter получает имя COM-порта из аргумента командной строки. После открытия порта первым отправляется PING; ответ PONG подтверждает связь. Команды GET и SET предназначены для чтения и установки показаний. Кадры защищены CRC; входящие и исходящие данные записываются в журнал JSONL с меткой времени.')
    h(d, 'Руководство пользователя')
    for line in ['1. Собрать проект lab3 в STM32CubeIDE и загрузить прошивку через встроенный FTDI/JTAG программатор стенда.',
                 '2. Подключить интерфейс UART, определить номер COM-порта и запустить: python lab3/client/app.py COMx.',
                 '3. Дождаться подтверждения PONG. Проверить отображение часов и состояния на экране и в окне программы.',
                 '4. Указать дату, время и день недели, нажать «Установить в часы». Путь к журналу отображается в окне.']:
        p(d, line)
    h(d, 'Результаты проверки')
    p(d, 'Прошивка собрана ARM-компилятором. В эмуляторе Unicorn выполнены 37 проверок ARM-кода: обработка даты и BCD, протокол, RTC, буфер OLED и команды приложения. Три теста клиента проверяют протокол, PING и журнал в демонстрационном режиме.')
    p(d, 'Работа на физическом стенде, реальные показания дисплея и установка времени в MCP79411 не подтверждены: стенд не был обнаружен при последней проверке. Перед защитой необходимо выполнить эти проверки на оборудовании. Бонусная поддержка матричной клавиатуры не реализована.')
    h(d, 'Вывод')
    p(d, 'Разработаны прошивка часов и клиентская программа с обменом по UART. Программные тесты подтверждают проверенные алгоритмы; для подтверждения работоспособности всей системы требуется тест на стенде.')
    sources(d)
    return d

def lab4():
    d = base(4, 'Операционные системы реального времени')
    h(d, 'Цель работы')
    p(d, 'Освоить применение FreeRTOS для программирования микроконтроллера и перенести функции часов из лабораторной работы № 3 в многозадачную программу.')
    h(d, 'Задание')
    p(d, 'Повторить функции часов и клиента предыдущей работы, используя не менее двух задач FreeRTOS и один обработчик прерывания. Обновлять OLED без зависаний с частотой не менее 20 кадров/с; вывести циклический счётчик 00–99. Сравнить реализацию без ОС и вариант с ОС РВ.')
    h(d, 'Аппаратная конфигурация')
    p(d, 'Стенд SDK-1.1MC.427, микроконтроллер STM32F427 на 120 МГц. MCP79411 и OLED SSD1306 подключены к I²C1 PB8/PB9, 400 кГц. Обмен с клиентом выполняется по USART6 PC6/PC7, 57600 бит/с, 8N1. Используется тот же клиент и формат сообщений, что в лабораторной № 3.')
    h(d, 'Архитектура FreeRTOS')
    p(d, 'В проекте используется FreeRTOS Kernel V10.0.1 с вытесняющим планированием и системным тиком 1 мс. Созданы две задачи. Обработчик USART6 обслуживает обмен байтами с ПК; SysTick обеспечивает работу планировщика и обновление счётчика времени HAL.')
    table(d, ('Задача', 'Приоритет', 'Назначение'), [
        ('display', '3', 'Берёт последнее состояние часов, формирует изображение и отправляет восемь страниц OLED. Плановый период — 40 мс.'),
        ('clock-uart', '2', 'Принимает команды, обслуживает UART, опрашивает MCP79411 раз в 250 мс и передаёт снимок состояния на дисплей.')], [3, 2.5, 10])
    p(d, 'Снимок данных передаётся через очередь длиной один элемент. Обе задачи используют одну шину I²C, поэтому каждая транзакция защищена мьютексом с наследованием приоритета. Между отправками страниц задача дисплея освобождает процессор на один тик.')
    p(d, 'На экране отображаются время, дата, день недели, состояние RTC и счётчик. Счётчик увеличивается после передачи полного кадра и после 99 возвращается к 00. При ошибке связи с дисплеем выполняется повторная инициализация.')
    h(d, 'Оценка времени обновления')
    p(d, 'Плановый период кадра 40 мс соответствует 25 кадрам/с. Передача 1024 байт при 400 кГц требует только для данных и битов подтверждения примерно 1024 × 9 / 400000 = 0,023 с. Дополнительное время требуется на адресацию, команды, ожидание доступа и работу процессора; фактическую частоту нужно измерять на работающем стенде.')
    p(d, 'Структура display_stats хранит число завершённых кадров, частоту за окно не короче секунды, максимальный интервал между кадрами, число интервалов более 50 мс и ошибки передачи. Проверку необходимо проводить также при обмене GET и SET с ПК.')
    h(d, 'Сравнение с реализацией без ОС')
    table(d, ('Свойство', 'Лабораторная 3', 'Лабораторная 4'), [
        ('Выполнение', 'Один кооперативный главный цикл.', 'Независимые задачи и вытесняющий планировщик.'),
        ('Общая шина', 'Транзакции последовательно выполняет один контекст.', 'Мьютекс сериализует транзакции задач.'),
        ('Передача данных', 'Общая структура в главном цикле.', 'Очередь передаёт копию состояния.'),
        ('Ресурсы', 'Меньше памяти и накладных расходов.', 'Дополнительные стеки, куча и переключения контекста.'),
        ('Сложность', 'Проще порядок действий; долгий шаг влияет на другие процессы.', 'Удобное разделение задач; необходимо следить за приоритетами и синхронизацией.')], [3, 6, 6.5])
    h(d, 'Руководство пользователя')
    p(d, 'Открыть lab4 через File → Import → Existing Projects into Workspace в STM32CubeIDE и собрать проект. Подключить стенд через DBG USB, загрузить ELF через FTDI/JTAG и запустить клиент предыдущей работы: python lab3/client/app.py COMx. После PONG проверить показания, задать время и убедиться, что счётчик на OLED изменяется.')
    h(d, 'Результаты проверки')
    p(d, 'Сборка STM32CubeIDE прошла без ошибок и предупреждений. В эмуляторе пройдены 46 проверок ARM-кода, включая проверку RTC, протокола, изображения, перехода счётчика 99→00 и статистики. Клиент прошёл три программных теста.')
    p(d, 'Стенд не был обнаружен при последней проверке. Запуск планировщика на микроконтроллере, обмен с RTC/OLED и соответствие требованию 20 кадров/с пока не подтверждены. Полученные результаты эмуляции не являются измерением частоты дисплея.')
    h(d, 'Вывод')
    p(d, 'Создана многозадачная версия программы часов с использованием FreeRTOS. Разделение задач и мьютекс упорядочивают работу дисплея и RTC. Для подтверждения стабильности и частоты обновления необходима аппаратная проверка под нагрузкой.')
    sources(d)
    p(d, 'FreeRTOS Kernel documentation. https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/01-Tasks')
    return d

if __name__ == '__main__':
    out3 = ROOT / 'lab3/docs/Отчет_ЛР3_вариант1_новый.docx'
    out4 = ROOT / 'lab4/docs/Отчет_ЛР4_вариант1.docx'
    for path, make in ((out3, lab3), (out4, lab4)):
        if path.exists() and '--replace' not in sys.argv: raise SystemExit(f'Refusing to overwrite {path}')
        make().save(path); print(path)
