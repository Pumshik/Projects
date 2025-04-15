# Circular Buffer (C++)

Реализация циклического буфера с поддержкой статического и динамического размещения

## Основной функционал:

### Два режима работы:
- **Статический буфер** (шаблонный параметр `Capacity`):
  - Размещается на стеке
  - Нет аллокаций в куче
  - Конструктор по умолчанию и конструктор с параметром `std::size_t capacity`
  - Пример: `CircularBuffer<int, 128>`
  - Копирование и присваивание разрешено только между буферами с одинаковой ёмкостью

- **Динамический буфер** (`Capacity == DYNAMIC_CAPACITY`):
  - Выделение памяти в куче
  - Ёмкость задаётся в рантайме
  - Конструктор с параметром `std::size_t capacity`
  - Пример: `CircularBuffer<int> buffer(256)`
  - Конструктор копирования и оператор присваивания работают с буферами того же типа

### Основные операции:
| Метод          | Сложность | Особенности                          |
|----------------|-----------|--------------------------------------|
| `push_back`    | O(1)      | Перезаписывает начало при переполнении|
| `push_front`   | O(1)      | Перезаписывает конец при переполнении |
| `pop_back`     | O(1)      | Удаляет последний элемент            |
| `pop_front`    | O(1)      | Удаляет первый элемент               |
| `operator[]`   | O(1)      | Быстрый доступ без проверок          |
| `at(pos)`      | O(1)      | Доступ с проверкой. Кидает `std::out_of_range` при выходе за границы|

### Прочие методы:
  - `insert(iterator, const T&)` делает вставку значения перед элементом, на который указывает итератор. Элемент, на который указывает итератор, и все остальные правее его сдвигаются на один вправо. При отсутствии свободного места затирается первый элемент в контейнере. Если свободное место отсутствует и итератор указывает на первый элемент, то вставка не происходит, содержимое контейнера остаётся как было. Работает за O(n)
  - Метод `erase(iterator)` удаляет элемент из контейнера по итератору. Все элементы справа сдвигаются на один влево, работает за O(n)
  - `size()`, `capacity()`, `empty()`, `full()`: Информация о состоянии буфера
  - `swap(other)`: Обмен содержимым. Для динамических буферов итераторы остаются валидными

### Итераторы:
- **Полная поддержка STL-совместимых итераторов:**
  - `iterator`, `const_iterator`
  - `reverse_iterator`, `const_reverse_iterator`
  - Поддержка арифметики указателей и операций сравнения
  - `end--` указывает на последний элемент
- **Методы итераторов:**
  - Методы `begin`, `cbegin`, `end` и `cend` возвращают неконстантные и константные итераторы на начало и на “элемент после конца” контейнера соответственно
  - Методы `rbegin`, `rend`, `crbegin`, `crend` - аналогично `для reverse_iterator`

### Управление памятью:

**Статический буфер**:
- Использует `std::array` для хранения
- Нулевые накладные расходы

**Динамический буфер**:
- Использует `unique_ptr` для автоматического управления
- Стратегия удвоения ёмкости при переаллокации

### Гарантии безопасности:
- Строгая гарантия исключений для всех методов кроме `insert` и `erase`
- Константная корректность
- Корректная обработка self-assignment

### Требования:
- Компилятор: C++17 (Clang/GCC)
- Библиотеки: `<algorithm>`, `<memory>`, `<stdexcept>`

## Тестирование:
Код готов к интеграции в тестовую среду:
```cpp
#include "circular_buffer.h"
