# Стек (C++)

Реализация стека строк с ручным управлением памятью в соответствии с ограничениями задачи.

## Структура проекта

## Команды стека
### Поддерживаемые операции:
- **`push <string>`**  
  Добавляет строку в стек → выводит `ok`  
  *Пример:* `push test123` → `ok`

- **`pop`**  
  Удаляет и возвращает верхний элемент → выводит значение или `error`  
  *Пример:* `pop` → `test123`

- **`back`**  
  Показывает верхний элемент → выводит значение или `error`

- **`size`**  
  Возвращает количество элементов → *число*

- **`clear`**  
  Очищает стек → `ok`

- **`exit`**  
  Завершает программу → `bye`

## Особенности реализации
### Ключевые механизмы:
- **Динамическая память**:
  - Стек хранится как массив указателей `char**`
  - Начальная емкость: 10 элементов (автоматическое расширение ×2)
  - Строки копируются в отдельно выделенные буферы

### Ограничения:
1. Максимальная длина строки: теоретически до 2^64 символов
2. Буфер ввода имеет начальный размер 100 символов

## Требования
- **Компилятор**: GCC/Clang с поддержкой C++11
- **Стандарт**: C++98 + ограниченные возможности C++11
