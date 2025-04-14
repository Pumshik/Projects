# Stack Allocator & Linked List Implementation (C++17)

Реализация высокопроизводительного аллокатора на стеке и аллокатор-осознанного двусвязного списка

## Основной функционал

### Класс `StackAllocator`
#### Особенности:
- **Статическое/динамическое выделение**:
  - `StackAllocator<T, N>`: фиксированный размер N (стек)
  - `StackAllocator<T>`: динамическая ёмкость (куча)
- **Выравнивание памяти**:
  - Гарантированное соблюдение alignment для типов
  - Использование `alignas(std::max_align_t)` для буфера
- **Нулевые аллокации**:
  ```cpp
  StackStorage<1024> storage;
  StackAllocator<int, 1024> alloc(storage);
  std::list<int, decltype(alloc)> lst(alloc); // Без heap-аллокаций

### Класс List
#### Особенности:

  -**Полная совместимость с STL**:

        - Поддержка всех стандартных итераторов

        - Конструкторы с аллокаторами

        - Методы push_back, insert, erase и др.

  -**Производительность**:

        - Оптимизированные перемещения узлов

        - Локальность данных при использовании StackAllocator

  -**Безопасность**:

        - Строгая гарантия исключений

        - Корректная обработка self-assignment

## Архитектура
### StackStorage

    - Запрет копирования

    - Методы allocate/deallocate с учётом выравнивания

### StackAllocator

    - Реализация требований Allocator concept

    - Поддержка rebind для внутренних типов

### List

- **Оптимизации**:

        - [[no_unique_address]] для аллокатора

        - EBO

- **Итераторы**:

        - Bidirectional и reverse итераторы

        - Конверсия между const/non-const
  
## Производительность
 - 
⚙️ Пример использования
cpp
Copy

// Стек-аллокатор на 1MB
StackStorage<1024*1024> storage;
StackAllocator<std::string, 1024*1024> alloc(storage);

// Список с кастомным аллокатором
List<std::string, decltype(alloc)> strings(alloc);
strings.push_back("Hello");
strings.emplace_back("World");

// Совместимость со STL алгоритмами
auto it = std::find(strings.begin(), strings.end(), "World");
if (it != strings.end()) {
    strings.erase(it);
}

🛠 Требования

    Компилятор: C++17+

    Библиотеки: <memory>, <type_traits>, <iterator>

    Оптимизации: Включенные (-O2/-O3)

⚠️ Ограничения

    Максимальный размер StackStorage ограничен стеком

    Нет поддержки аллокаторов с состоянием

    StackAllocator не thread-safe
