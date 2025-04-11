#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>

constexpr std::size_t DYNAMIC_CAPACITY =
    std::numeric_limits<std::size_t>::max();

template <typename T, std::size_t Capacity = DYNAMIC_CAPACITY>
class CircularBuffer {
 public:
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  CircularBuffer() : capacity_(Capacity), size_(0), head_(0), tail_(0) {
    static_assert(Capacity != DYNAMIC_CAPACITY,
                  "Default constructor is only for static buffers");
  }

  explicit CircularBuffer(size_type capacity)
      : capacity_(capacity), size_(0), head_(0), tail_(0) {
    if constexpr (Capacity == DYNAMIC_CAPACITY) {
      if (capacity == 0) {
        throw std::invalid_argument("Capacity must be positive");
      }
      buffer_ = std::make_unique<std::byte[]>(capacity_ * sizeof(T));
    } else {
      if (capacity != Capacity) {
        throw std::invalid_argument("Capacity must match template parameter");
      }
    }
  }

  CircularBuffer(const CircularBuffer& other)
      : capacity_(other.capacity_), size_(0), head_(0), tail_(0) {
    if constexpr (Capacity == DYNAMIC_CAPACITY) {
      buffer_ = std::make_unique<std::byte[]>(capacity_ * sizeof(T));
    }

    try {
      for (size_type i = 0; i < other.size_; ++i) {
        new (&get(i)) T(other[i]);
        ++size_;
        tail_ = (tail_ + 1) % capacity_;
      }
    } catch (...) {
      clear();
      throw;
    }
  }

  CircularBuffer& operator=(const CircularBuffer& other) {
    if (this != &other) {
      clear();

      capacity_ = other.capacity_;
      size_ = other.size_;
      head_ = other.head_;
      tail_ = other.tail_;

      if constexpr (Capacity == DYNAMIC_CAPACITY) {
        buffer_ = std::make_unique<std::byte[]>(capacity_ * sizeof(T));
      }

      for (size_type i = 0; i < size_; ++i) {
        new (&get_buffer()[(head_ + i) % capacity_ * sizeof(T)])
            T(*reinterpret_cast<const T*>(
                &other.get_buffer()[(other.head_ + i) % other.capacity_ *
                                    sizeof(T)]));
      }
    }
    return *this;
  }

  ~CircularBuffer() { clear(); }

  void push_back(const T& value) {
    if (full()) {
      T* old = reinterpret_cast<T*>(&get_buffer()[tail_ * sizeof(T)]);
      T temp(value);
      old->~T();
      new (old) T(std::move(temp));
      head_ = (head_ + 1) % capacity_;
      tail_ = (tail_ + 1) % capacity_;
    } else {
      new (&get_buffer()[tail_ * sizeof(T)]) T(value);
      tail_ = (tail_ + 1) % capacity_;
      ++size_;
    }
  }

  void push_front(const T& value) {
    if (full()) {
      size_type new_head = (head_ - 1 + capacity_) % capacity_;
      T* old = reinterpret_cast<T*>(&get_buffer()[new_head * sizeof(T)]);
      T temp(value);
      old->~T();
      new (old) T(std::move(temp));
      head_ = new_head;
      tail_ = (tail_ - 1 + capacity_) % capacity_;
    } else {
      size_type new_head = (head_ - 1 + capacity_) % capacity_;
      new (&get_buffer()[new_head * sizeof(T)]) T(value);
      head_ = new_head;
      ++size_;
    }
  }

  void pop_back() {
    if (empty()) {
      throw std::out_of_range("Buffer is empty");
    }
    tail_ = (tail_ - 1 + capacity_) % capacity_;
    reinterpret_cast<T*>(&get_buffer()[tail_ * sizeof(T)])->~T();
    --size_;
  }

  void pop_front() {
    if (empty()) {
      throw std::out_of_range("Buffer is empty");
    }
    reinterpret_cast<T*>(&get_buffer()[head_ * sizeof(T)])->~T();
    head_ = (head_ + 1) % capacity_;
    --size_;
  }

  size_type size() const { return size_; }
  size_type capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }
  bool full() const { return size_ == capacity_; }

  const T& operator[](size_type index) const {
    return *reinterpret_cast<const T*>(
        &get_buffer()[(head_ + index) % capacity_ * sizeof(T)]);
  }

  T& operator[](size_type index) {
    return *reinterpret_cast<T*>(
        &get_buffer()[(head_ + index) % capacity_ * sizeof(T)]);
  }

  const T& at(size_type index) const {
    if (index >= size_) {
      throw std::out_of_range("Index out of range");
    }
    return *reinterpret_cast<const T*>(
        &get_buffer()[(head_ + index) % capacity_ * sizeof(T)]);
  }

  T& at(size_type index) {
    if (index >= size_) {
      throw std::out_of_range("Index out of range");
    }
    return *reinterpret_cast<T*>(
        &get_buffer()[(head_ + index) % capacity_ * sizeof(T)]);
  }

  void clear() {
    while (!empty()) {
      pop_back();
    }
  }

  class iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    iterator(T* buffer, size_type capacity, size_type raw_index, size_type head)
        :raw_index_(raw_index),
        buffer_(buffer),
        capacity_(capacity),
        head_(head) {}

    reference operator*() const {
      return buffer_[(head_ + raw_index_) % capacity_];
    }

    pointer operator->() const {
      return &buffer_[(head_ + raw_index_) % capacity_];
    }

    iterator& operator++() {
      ++raw_index_;
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++raw_index_;
      return tmp;
    }

    iterator& operator--() {
      --raw_index_;
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      --raw_index_;
      return tmp;
    }

    iterator& operator+=(difference_type n) {
      raw_index_ += n;
      return *this;
    }

    iterator& operator-=(difference_type n) {
      raw_index_ -= n;
      return *this;
    }

    iterator operator+(difference_type n) const {
      return iterator(buffer_, capacity_, raw_index_ + n, head_);
    }

    iterator operator-(difference_type n) const {
      return iterator(buffer_, capacity_, raw_index_ - n, head_);
    }

    difference_type operator-(const iterator& other) const {
      return static_cast<difference_type>(raw_index_) -
             static_cast<difference_type>(other.raw_index_);
    }

    friend iterator operator+(difference_type n, const iterator& it) {
      return it + n;
    }

    bool operator==(const iterator& other) const {
      return raw_index_ == other.raw_index_;
    }
    bool operator!=(const iterator& other) const {
      return raw_index_ != other.raw_index_;
    }
    bool operator<(const iterator& other) const {
      return raw_index_ < other.raw_index_;
    }
    bool operator<=(const iterator& other) const {
      return raw_index_ <= other.raw_index_;
    }
    bool operator>(const iterator& other) const {
      return raw_index_ > other.raw_index_;
    }
    bool operator>=(const iterator& other) const {
      return raw_index_ >= other.raw_index_;
    }

    size_type raw_index_;

   private:
    T* buffer_;
    size_type capacity_;
    size_type head_;
    std::array<char, (Capacity == DYNAMIC_CAPACITY) ? 16 : 0> padding_;
  };

  class const_iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = const T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    const_iterator(const T* buffer, size_type capacity, size_type index)
        : buffer_(buffer), capacity_(capacity), index_(index) {}

    reference operator*() const { return buffer_[index_]; }
    pointer operator->() const { return &buffer_[index_]; }

    const_iterator& operator++() {
      index_ = (index_ + 1) % capacity_;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    const_iterator& operator--() {
      index_ = (index_ - 1 + capacity_) % capacity_;
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator tmp = *this;
      --(*this);
      return tmp;
    }

    const_iterator& operator+=(difference_type n) {
      index_ = (index_ + n) % capacity_;
      return *this;
    }

    const_iterator& operator-=(difference_type n) {
      index_ = (index_ - n + capacity_) % capacity_;
      return *this;
    }

    const_iterator operator+(difference_type n) const {
      return const_iterator(buffer_, capacity_, (index_ + n) % capacity_);
    }

    const_iterator operator-(difference_type n) const {
      return const_iterator(buffer_, capacity_,
                            (index_ - n + capacity_) % capacity_);
    }

    difference_type operator-(const const_iterator& other) const {
      return (index_ - other.index_ + capacity_) % capacity_;
    }

    bool operator==(const const_iterator& other) const {
      return index_ == other.index_;
    }
    bool operator!=(const const_iterator& other) const {
      return index_ != other.index_;
    }
    bool operator<(const const_iterator& other) const {
      return index_ < other.index_;
    }
    bool operator<=(const const_iterator& other) const {
      return index_ <= other.index_;
    }
    bool operator>(const const_iterator& other) const {
      return index_ > other.index_;
    }
    bool operator>=(const const_iterator& other) const {
      return index_ >= other.index_;
    }

   private:
    const T* buffer_;
    size_type capacity_;
    size_type index_;
    std::array<char, (Capacity == DYNAMIC_CAPACITY) ? 16 : 0> padding_;
  };

  iterator begin() {
    return iterator(reinterpret_cast<T*>(get_buffer()), capacity_, 0, head_);
  }

  const_iterator begin() const {
    return const_iterator(reinterpret_cast<const T*>(get_buffer()), capacity_,
                          head_);
  }

  const_iterator cbegin() const {
    return const_iterator(reinterpret_cast<const T*>(get_buffer()), capacity_,
                          head_);
  }

  iterator end() {
    return iterator(reinterpret_cast<T*>(get_buffer()), capacity_, size_,
                    head_);
  }

  const_iterator end() const {
    return const_iterator(reinterpret_cast<const T*>(get_buffer()), capacity_,
                          tail_);
  }

  const_iterator cend() const {
    return const_iterator(reinterpret_cast<const T*>(get_buffer()), capacity_,
                          tail_);
  }

  class reverse_iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    reverse_iterator(iterator it) : it_(it) {}

    reference operator*() const {
      auto tmp = it_;
      --tmp;
      return *tmp;
    }

    pointer operator->() const {
      auto tmp = it_;
      --tmp;
      return tmp.operator->();
    }

    reverse_iterator& operator++() {
      --it_;
      return *this;
    }

    reverse_iterator operator++(int) {
      reverse_iterator tmp = *this;
      --it_;
      return tmp;
    }

    reverse_iterator& operator--() {
      ++it_;
      return *this;
    }

    reverse_iterator operator--(int) {
      reverse_iterator tmp = *this;
      ++it_;
      return tmp;
    }

    reverse_iterator& operator+=(difference_type n) {
      it_ -= n;
      return *this;
    }

    reverse_iterator& operator-=(difference_type n) {
      it_ += n;
      return *this;
    }

    reverse_iterator operator+(difference_type n) const {
      return reverse_iterator(it_ - n);
    }

    reverse_iterator operator-(difference_type n) const {
      return reverse_iterator(it_ + n);
    }

    difference_type operator-(const reverse_iterator& other) const {
      return other.it_ - it_;
    }

    friend reverse_iterator operator+(difference_type n,
                                      const reverse_iterator& it) {
      return it + n;
    }

    bool operator==(const reverse_iterator& other) const {
      return it_ == other.it_;
    }
    bool operator!=(const reverse_iterator& other) const {
      return it_ != other.it_;
    }
    bool operator<(const reverse_iterator& other) const {
      return it_ > other.it_;
    }
    bool operator<=(const reverse_iterator& other) const {
      return it_ >= other.it_;
    }
    bool operator>(const reverse_iterator& other) const {
      return it_ < other.it_;
    }
    bool operator>=(const reverse_iterator& other) const {
      return it_ <= other.it_;
    }

   private:
    iterator it_;
  };

  class const_reverse_iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = const T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    const_reverse_iterator(const_iterator it) : it_(it) {}

    reference operator*() const { return *it_; }
    pointer operator->() const { return it_.operator->(); }

    const_reverse_iterator& operator++() {
      --it_;
      return *this;
    }

    const_reverse_iterator operator++(int) {
      const_reverse_iterator tmp = *this;
      --it_;
      return tmp;
    }

    const_reverse_iterator& operator--() {
      ++it_;
      return *this;
    }

    const_reverse_iterator operator--(int) {
      const_reverse_iterator tmp = *this;
      ++it_;
      return tmp;
    }

    const_reverse_iterator& operator+=(difference_type n) {
      it_ -= n;
      return *this;
    }

    const_reverse_iterator& operator-=(difference_type n) {
      it_ += n;
      return *this;
    }

    const_reverse_iterator operator+(difference_type n) const {
      return const_reverse_iterator(it_ - n);
    }

    const_reverse_iterator operator-(difference_type n) const {
      return const_reverse_iterator(it_ + n);
    }

    difference_type operator-(const const_reverse_iterator& other) const {
      return it_ - other.it_;
    }

    bool operator==(const const_reverse_iterator& other) const {
      return it_ == other.it_;
    }
    bool operator!=(const const_reverse_iterator& other) const {
      return it_ != other.it_;
    }
    bool operator<(const const_reverse_iterator& other) const {
      return it_ > other.it_;
    }
    bool operator<=(const const_reverse_iterator& other) const {
      return it_ >= other.it_;
    }
    bool operator>(const const_reverse_iterator& other) const {
      return it_ < other.it_;
    }
    bool operator>=(const const_reverse_iterator& other) const {
      return it_ <= other.it_;
    }

   private:
    const_iterator it_;
  };
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(cend());
  }

  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  }

  void swap(CircularBuffer& other) {
    if constexpr (Capacity == DYNAMIC_CAPACITY) {
      std::swap(buffer_, other.buffer_);
    } else {
      for (size_type i = 0; i < Capacity; ++i) {
        std::swap(buffer_static_[i], other.buffer_static_[i]);
      }
    }
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    std::swap(head_, other.head_);
    std::swap(tail_, other.tail_);
  }

  iterator insert(iterator pos, const T& value) {
    size_type offset = pos - begin();

    if (full()) {
      if (offset == 0) {
        return begin();
      }
      pop_front();
      offset--;
    }

    if (empty()) {
      push_back(value);
      return begin();
    }

    push_back((*this)[size() - 1]);

    auto first = begin() + offset;
    auto middle = begin() + (size() - 1);
    auto last = end();
    std::rotate(first, middle, last);

    (*this)[offset] = value;

    return begin() + offset;
  }

  iterator erase(iterator pos) {
    for (auto it = pos; it != end() - 1; ++it) {
      *it = *(it + 1);
    }
    pop_back();
    return pos;
  }

 private:
  T& get(size_type index) {
    return *reinterpret_cast<T*>(
        &get_buffer()[((head_ + index) % capacity_) * sizeof(T)]);
  }

  const T& get(size_type index) const {
    return *reinterpret_cast<const T*>(
        &get_buffer()[((head_ + index) % capacity_) * sizeof(T)]);
  }

  std::byte* get_buffer() {
    if constexpr (Capacity == DYNAMIC_CAPACITY) {
      return buffer_.get();
    } else {
      return buffer_static_.data();
    }
  }

  const std::byte* get_buffer() const {
    if constexpr (Capacity == DYNAMIC_CAPACITY) {
      return buffer_.get();
    } else {
      return buffer_static_.data();
    }
  }

  std::unique_ptr<std::byte[]> buffer_;
  std::array<std::byte,
             (Capacity == DYNAMIC_CAPACITY) ? 0 : Capacity * sizeof(T)>
      buffer_static_;
  size_type capacity_;
  size_type size_;
  size_type head_;
  size_type tail_;
};