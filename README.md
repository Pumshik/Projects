# Shared Ptr (C++)

Полная реализация системы умных указателей с разделяемым владением (аналог std::shared_ptr, std::weak_ptr, enable_shared_from_this) с поддержкой кастомных делитеров, аллокаторов и всех стандартных функций.

## Архитектура системы

### Структура Control Block
BaseControlBlock (абстрактный)
- PointerControlBlock<T, Deleter, Alloc>
  - ptr: T* (управляемый объект)
  - deleter: Deleter
  - alloc: Alloc
- ObjectControlBlock<T, Alloc, Args...>
  - object: T
  - alloc: Alloc

## Основные классы

### 1. SharedPtr<T>

#### Методы:
| Метод | Описание |
|-------|----------|
| get() | Возвращает сырой указатель |
| operator*() | Разыменование |
| operator->() | Доступ к членам |
| use_count() | Количество shared_ptr, владеющих объектом |
| reset() | Освобождает текущий объект |
| reset(Y* ptr, Deleter, Alloc) | Заменяет управляемый объект |
| swap(SharedPtr&) | Обменивает содержимое |

### 2. WeakPtr<T>

#### Методы:
| Метод | Описание |
|-------|----------|
| expired() | Проверяет, жив ли объект |
| lock() | Возвращает SharedPtr, если объект жив |
| use_count() | Количество shared_ptr, владеющих объектом |
| swap(WeakPtr&) | Обменивает содержимое |

### 3. EnableSharedFromThis<T>

#### Методы:
- shared_from_this() - возвращает SharedPtr<T> на текущий объект

## Ключевые особенности реализации

### 1. Type Erasure
- Делитер и аллокатор хранятся в ControlBlock, не являются шаблонными параметрами SharedPtr
- Позволяет создавать массивы SharedPtr с разными делитерами

### 2. Два типа Control Block
PointerControlBlock:
- Для объектов, созданных отдельно от control block
- Содержит указатель, делитер и аллокатор
- При уничтожении вызывает делитер

ObjectControlBlock:
- Для объектов, созданных makeShared/allocateShared
- Содержит объект встроенным образом
- Одновременное выделение объекта и control block

### 3. Наследование и преобразования
- Поддержка преобразований SharedPtr<Derived> -> SharedPtr<Base>
- Корректное уничтожение объектов через правильный тип
- Проверки времени компиляции с std::enable_if_t и std::is_convertible_v

### 4. Безопасность исключений
- Строгая гарантия для большинства операций
- Copy-and-swap в операторах присваивания
- Защита от double free

## Оптимизации

### 1. Single allocation
- Объект и control block в одной области памяти
- Улучшает локальность данных

### 2. Small buffer optimization
- Для малых объектов можно хранить делитер/аллокатор в control block
- Избегаем дополнительных выделений памяти

### 3. Type erasure
- Единый интерфейс BaseControlBlock

### EnableSharedFromThis механизм
1. При создании SharedPtr проверяем, наследуется ли T от EnableSharedFromThis<T>
2. Если да, устанавливаем weak_this в созданный SharedPtr
3. shared_from_this() проверяет weak_this и создает SharedPtr

## Требования

- Язык и стандарт: C++17
- Компиляторы: Clang 10+, GCC 10+, MSVC 2019+
- Библиотеки: `<memory>`, `<type_traits>`, `<utility>`, `<stdexcept>`
