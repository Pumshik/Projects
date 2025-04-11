#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <size_t N>
class StackStorage {
  alignas(std::max_align_t) char buffer[N];
  size_t offset = 0;

 public:
  StackStorage() = default;
  StackStorage(const StackStorage&) = delete;
  StackStorage& operator=(const StackStorage&) = delete;

  template <typename T>
  T* allocate(size_t count, size_t alignment = alignof(T)) {
    size_t size = count * sizeof(T);
    size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
    if (aligned_offset + size > N) {
      throw std::bad_alloc();
    }   
    T* ptr = reinterpret_cast<T*>(buffer + aligned_offset);
    offset = aligned_offset + size;
    return ptr;
  }

  void deallocate(void*, size_t) noexcept {}
};

template <typename T, size_t N>
class StackAllocator {
 public:
  StackStorage<N>* storage;

  using value_type = T;

  explicit StackAllocator(StackStorage<N>& storage) noexcept
      : storage(&storage) {}

  template <typename U>
  StackAllocator(const StackAllocator<U, N>& other) noexcept
      : storage(other.storage) {}

  T* allocate(size_t n) { return storage->template allocate<T>(n); }

  void deallocate(T*, size_t) noexcept {}

  bool operator==(const StackAllocator& other) const noexcept {
    return storage == other.storage;
  }

  bool operator!=(const StackAllocator& other) const noexcept {
    return !(*this == other);
  }

  template <typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };
};

template <typename T, typename Allocator = std::allocator<T>>
class List {
  struct BaseNode {
    BaseNode* prev;
    BaseNode* next;
    BaseNode() : prev(this), next(this) {}
  };

  struct Node : BaseNode {
    T value;
    template <typename... Args>
    Node(Args&&... args) : value(std::forward<Args>(args)...) {}
  };

  using NodeAlloc =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using AllocTraits = std::allocator_traits<NodeAlloc>;

  BaseNode endNode;
  size_t size_ = 0;
  [[no_unique_address]] NodeAlloc alloc_;

  void clear_nodes() noexcept {
    BaseNode* current = endNode.next;
    while (current != &endNode) {
      Node* deleted = static_cast<Node*>(current);
      current = current->next;
      AllocTraits::destroy(alloc_, deleted);
      AllocTraits::deallocate(alloc_, deleted, 1);
    }
    endNode.next = endNode.prev = &endNode;
    size_ = 0;
  }

  void link(BaseNode* left, BaseNode* right) noexcept {
    left->next = right;
    right->prev = left;
  }

  Node* create_node(const T& value) {
    Node* new_node = AllocTraits::allocate(alloc_, 1);
    try {
      AllocTraits::construct(alloc_, new_node, value);
    } catch (...) {
      AllocTraits::deallocate(alloc_, new_node, 1);
      throw;
    }
    return new_node;
  }

  void destroy_node(Node* node) noexcept {
    AllocTraits::destroy(alloc_, node);
    AllocTraits::deallocate(alloc_, node, 1);
  }

  void connect_nodes(BaseNode* left, BaseNode* middle,
                     BaseNode* right) noexcept {
    left->next = middle;
    middle->prev = left;
    middle->next = right;
    right->prev = middle;
  }

  void transfer_nodes(List& other) noexcept {
    endNode.next = other.endNode.next;
    endNode.prev = other.endNode.prev;
    endNode.next->prev = &endNode;
    endNode.prev->next = &endNode;
    size_ = other.size_;
    other.endNode.next = &other.endNode;
    other.endNode.prev = &other.endNode;
    other.size_ = 0;
  }

  void copy_elements(const List& other) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      insert(end(), *it);
    }
  }

 public:
  Allocator get_allocator() const noexcept { return alloc_; }
  List() = default;
  ~List() { clear_nodes(); }
  explicit List(const Allocator& alloc) : alloc_(alloc) {}

  explicit List(size_t count, const Allocator& alloc = Allocator())
      : alloc_(alloc) {
    size_t constructed = 0;
    try {
      for (; constructed < count; ++constructed) {
        Node* new_node = AllocTraits::allocate(alloc_, 1);
        try {
          AllocTraits::construct(alloc_, new_node);
        } catch (...) {
          AllocTraits::deallocate(alloc_, new_node, 1);
          throw;
        }
        link(endNode.prev, new_node);
        link(new_node, &endNode);
        ++size_;
      }
    } catch (...) {
      while (constructed-- > 0) {
        pop_back();
      }
      throw;
    }
  }
  List(size_t count, const T& value, const Allocator& alloc = Allocator())
      : alloc_(alloc) {
    try {
      for (; size_ < count; ++size_) {
        Node* new_node = AllocTraits::allocate(alloc_, 1);
        AllocTraits::construct(alloc_, new_node, value);
        link(endNode.prev, new_node);
        link(new_node, &endNode);
      }
    } catch (...) {
      clear_nodes();
      throw;
    }
  }

  List(List&& other) noexcept : alloc_(std::move(other.alloc_)) {
    if (other.size_ > 0) {
      transfer_nodes(other);
    }
  }

  List(const List& other)
      : alloc_(
            AllocTraits::select_on_container_copy_construction(other.alloc_)) {
    try {
      copy_elements(other);
    } catch (...) {
      clear_nodes();
      throw;
    }
  }

  List& operator=(List&& other) noexcept {
    if (this != &other) {
      if (AllocTraits::propagate_on_container_move_assignment::value ||
          alloc_ == other.alloc_) {
        clear_nodes();
        if (AllocTraits::propagate_on_container_move_assignment::value) {
          alloc_ = std::move(other.alloc_);
        }
        if (other.size_ > 0) {
          transfer_nodes(other);
        }
      } else {
        clear_nodes();
        auto it = other.begin();
        while (it != other.end()) {
          try {
            insert(end(), std::move(*it));
            it = other.erase(it);
          } catch (...) {
            clear_nodes();
            throw;
          }
        }
      }
    }
    return *this;
  }

  List& operator=(const List& other) {
    if (this != &other) {
      if (AllocTraits::propagate_on_container_copy_assignment::value) {
        alloc_ = other.alloc_;
      }
      List tmp(other);
      if (AllocTraits::propagate_on_container_copy_assignment::value) {
        tmp.alloc_ = alloc_;
      }
      swap(tmp);
    }
    return *this;
  }

  void push_back(const T& value) { insert(end(), value); }
  void push_front(const T& value) { insert(begin(), value); }
  void pop_front() {
    if (!empty()) {
      erase(begin());
    }
  }
  void pop_back() {
    if (!empty()) {
      erase(--end());
    }
  }

  size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }

  class iterator {
    BaseNode* node;

   public:
    friend class List;
    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = ptrdiff_t;
    explicit iterator(BaseNode* node = nullptr) : node(node) {}

    iterator& operator++() {
      node = node->next;
      return *this;
    }
    iterator& operator--() {
      node = node->prev;
      return *this;
    }
    iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }
    iterator operator--(int) {
      auto tmp = *this;
      --*this;
      return tmp;
    }

    bool operator!=(const iterator& other) const { return node != other.node; }
    bool operator==(const iterator& other) const { return node == other.node; }
    reference operator*() const { return static_cast<Node*>(node)->value; }
    pointer operator->() const { return &static_cast<Node*>(node)->value; }
  };

  class const_iterator {
    const BaseNode* node;

   public:
    friend class List;
    using value_type = const T;
    using reference = const T&;
    using pointer = const T*;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = ptrdiff_t;

    explicit const_iterator(const BaseNode* node) : node(node) {}
    const_iterator(const iterator& other) : node(other.node) {}

    const_iterator& operator++() {
      node = node->next;
      return *this;
    }
    const_iterator& operator--() {
      node = node->prev;
      return *this;
    }
    const_iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }
    const_iterator operator--(int) {
      auto tmp = *this;
      --*this;
      return tmp;
    }

    bool operator!=(const const_iterator& other) const {
      return node != other.node;
    }
    bool operator==(const const_iterator& other) const {
      return node == other.node;
    }
    reference operator*() const {
      return static_cast<const Node*>(node)->value;
    }
    pointer operator->() const {
      return &static_cast<const Node*>(node)->value;
    }
  };

  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  iterator begin() noexcept { return iterator(endNode.next); }
  iterator end() noexcept { return iterator(&endNode); }
  const_iterator begin() const noexcept { return const_iterator(endNode.next); }
  const_iterator end() const noexcept { return const_iterator(&endNode); }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }
  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  const_reverse_iterator crend() const noexcept { return rend(); }

  void swap(List& other) noexcept {
    using std::swap;
    if constexpr (AllocTraits::propagate_on_container_swap::value) {
      swap(alloc_, other.alloc_);
    }
    if (size_ == 0 && other.size_ == 0) {
    } else if (size_ == 0) {
      endNode.next = other.endNode.next;
      endNode.prev = other.endNode.prev;
      endNode.next->prev = &endNode;
      endNode.prev->next = &endNode;
      other.endNode.next = &other.endNode;
      other.endNode.prev = &other.endNode;
    } else if (other.size_ == 0) {
      other.endNode.next = endNode.next;
      other.endNode.prev = endNode.prev;
      other.endNode.next->prev = &other.endNode;
      other.endNode.prev->next = &other.endNode;
      endNode.next = &endNode;
      endNode.prev = &endNode;
    } else {
      BaseNode* this_first = endNode.next;
      BaseNode* this_last = endNode.prev;
      BaseNode* other_first = other.endNode.next;
      BaseNode* other_last = other.endNode.prev;
      endNode.next = other_first;
      endNode.prev = other_last;
      other.endNode.next = this_first;
      other.endNode.prev = this_last;
      this_first->prev = &other.endNode;
      this_last->next = &other.endNode;
      other_first->prev = &endNode;
      other_last->next = &endNode;
    }
    swap(size_, other.size_);
  }

  iterator erase(const_iterator pos) {
    if (pos == end() || empty()) {
      return end();
    }
    BaseNode* base_ptr = const_cast<BaseNode*>(pos.node);
    Node* to_delete = static_cast<Node*>(base_ptr);
    iterator next(to_delete->next);
    link(to_delete->prev, to_delete->next);
    destroy_node(to_delete);
    --size_;
    return next;
  }

  iterator insert(const_iterator pos, const T& value) {
    Node* new_node = create_node(value);
    connect_nodes(pos.node->prev, new_node, const_cast<BaseNode*>(pos.node));
    ++size_;
    return iterator(new_node);
  }
};