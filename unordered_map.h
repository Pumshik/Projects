#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
 public:
  using NodeType = std::pair<const Key, Value>;

 private:
  struct Node {
    NodeType value;
    Node* next_in_bucket;
    Node* next_in_list;
    Node* prev_in_list;
    size_t cached_hash;

    Node()
        : next_in_bucket(nullptr),
          next_in_list(nullptr),
          prev_in_list(nullptr),
          cached_hash(0) {}
  };

  using AllocTraits = std::allocator_traits<Alloc>;
  using NodeAlloc = typename AllocTraits::template rebind_alloc<Node>;
  using NodeAllocTraits = std::allocator_traits<NodeAlloc>;

  [[no_unique_address]] Hash hash_;
  [[no_unique_address]] Equal equal_;
  [[no_unique_address]] NodeAlloc alloc_;

  using BucketAlloc =
      typename std::allocator_traits<NodeAlloc>::template rebind_alloc<Node*>;
  std::vector<Node*, BucketAlloc> buckets_;
  Node* head_;
  Node* tail_;
  size_t size_;
  float max_load_factor_;

  void insert_node(Node* new_node) {
    bool needs_rehash = false;
    if (buckets_.empty()) {
      needs_rehash = true;
    } else {
      const float projected_load =
          static_cast<float>(size_ + 1) / buckets_.size();
      needs_rehash = (projected_load > max_load_factor_);
    }

    if (needs_rehash) {
      const size_t new_size = buckets_.empty() ? 1 : buckets_.size() * 2;
      rehash(new_size);
    }

    const size_t bucket_idx = new_node->cached_hash % buckets_.size();
    Node** bucket_ptr = &buckets_[bucket_idx];
    while (*bucket_ptr) {
      bucket_ptr = &(*bucket_ptr)->next_in_bucket;
    }
    *bucket_ptr = new_node;

    new_node->prev_in_list = tail_;
    if (tail_) {
      tail_->next_in_list = new_node;
    } else {
      head_ = new_node;
    }
    tail_ = new_node;

    ++size_;
  }

  void rehash(size_t new_bucket_count) {
    std::vector<Node*, BucketAlloc> new_buckets(new_bucket_count, nullptr,
                                                buckets_.get_allocator());

    std::vector<Node*> temp_list;
    for (Node* curr = head_; curr; curr = curr->next_in_list) {
      temp_list.push_back(curr);
    }

    for (Node* curr : temp_list) {
      const size_t new_idx = curr->cached_hash % new_bucket_count;
      curr->next_in_bucket = new_buckets[new_idx];
      new_buckets[new_idx] = curr;
    }

    buckets_ = std::move(new_buckets);
  }

  void clear() noexcept {
    Node* curr = head_;
    while (curr != nullptr) {
      Node* next = curr->next_in_list;
      using PairAlloc =
          typename NodeAllocTraits::template rebind_alloc<NodeType>;
      PairAlloc pair_alloc(alloc_);
      std::allocator_traits<PairAlloc>::destroy(pair_alloc, &curr->value);
      NodeAllocTraits::deallocate(alloc_, curr, 1);
      curr = next;
    }
    head_ = nullptr;
    tail_ = nullptr;
    size_ = 0;
    buckets_.clear();
  }

 public:
  template <bool IsConst>
  class Iterator {
    Node* current_;
    friend class UnorderedMap;

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::conditional_t<IsConst, const NodeType, NodeType>;

    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    Iterator(Node* curr = nullptr) : current_(curr) {}

    template <bool OtherIsConst,
              typename = std::enable_if_t<IsConst && !OtherIsConst>>
    Iterator(const Iterator<OtherIsConst>& other) : current_(other.current_) {}

    reference operator*() const { return current_->value; }
    pointer operator->() const { return &current_->value; }

    Iterator& operator++() {
      current_ = current_->next_in_list;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const Iterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const Iterator& other) const { return !(*this == other); }
  };

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  UnorderedMap(size_t bucket_count = 16,
               const Hash& hash = Hash(),
               const Equal& equal = Equal(),
               const Alloc& alloc = Alloc())
      : hash_(hash),
        equal_(equal),
        alloc_(alloc),
        head_(nullptr),
        tail_(nullptr),
        size_(0),
        max_load_factor_(1.0) {
    rehash(bucket_count);
  }

  UnorderedMap(const UnorderedMap& other)
      : hash_(other.hash_),
        equal_(other.equal_),
        alloc_(NodeAllocTraits::select_on_container_copy_construction(
            other.alloc_)),
        head_(nullptr),
        tail_(nullptr),
        size_(0),
        max_load_factor_(other.max_load_factor_) {
    try {
      for (const auto& node : other) {
        insert(node);
      }
    } catch (...) {
      clear();
      throw;
    }
  }

  UnorderedMap(UnorderedMap&& other) noexcept
      : hash_(std::move(other.hash_)),
        equal_(std::move(other.equal_)),
        alloc_(std::move(other.alloc_)),
        buckets_(std::move(other.buckets_)),
        head_(other.head_),
        tail_(other.tail_),
        size_(other.size_),
        max_load_factor_(other.max_load_factor_) {
    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.size_ = 0;
    other.buckets_.clear();
  }

  ~UnorderedMap() { clear(); }

  UnorderedMap& operator=(const UnorderedMap& other) {
    if (this != &other) {
      clear();
      if (NodeAllocTraits::propagate_on_container_copy_assignment::value) {
        alloc_ = other.alloc_;
      }
      hash_ = other.hash_;
      equal_ = other.equal_;
      max_load_factor_ = other.max_load_factor_;
      rehash(other.buckets_.size());
      for (const auto& node : other) {
        insert(node);
      }
    }
    return *this;
  }

  UnorderedMap& operator=(UnorderedMap&& other) noexcept {
    if (this != &other) {
      clear();
      if (NodeAllocTraits::propagate_on_container_move_assignment::value) {
        alloc_ = std::move(other.alloc_);
      }
      hash_ = std::move(other.hash_);
      equal_ = std::move(other.equal_);
      buckets_ = std::move(other.buckets_);
      head_ = other.head_;
      tail_ = other.tail_;
      size_ = other.size_;
      max_load_factor_ = other.max_load_factor_;
      other.head_ = nullptr;
      other.tail_ = nullptr;
      other.size_ = 0;
      other.buckets_.clear();
    }
    return *this;
  }

  void swap(UnorderedMap& other) noexcept {
    using std::swap;
    if (NodeAllocTraits::propagate_on_container_swap::value) {
      swap(alloc_, other.alloc_);
    }
    swap(hash_, other.hash_);
    swap(equal_, other.equal_);
    swap(buckets_, other.buckets_);
    swap(head_, other.head_);
    swap(tail_, other.tail_);
    swap(size_, other.size_);
    swap(max_load_factor_, other.max_load_factor_);
  }

  iterator begin() noexcept { return iterator(head_); }
  const_iterator begin() const noexcept { return const_iterator(head_); }
  iterator end() noexcept { return iterator(nullptr); }
  const_iterator end() const noexcept { return const_iterator(nullptr); }

  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }
  const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(begin());
  }

  size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }

  float load_factor() const noexcept {
    return buckets_.empty() ? 0.0f
                            : static_cast<float>(size_) / buckets_.size();
  }

  float max_load_factor() const noexcept { return max_load_factor_; }
  void max_load_factor(float ml) noexcept { max_load_factor_ = ml; }

  void reserve(size_t count) {
    size_t required_buckets =
        static_cast<size_t>(std::ceil(count / max_load_factor_));
    if (required_buckets > buckets_.size()) {
      rehash(required_buckets);
    }
  }

  std::pair<iterator, bool> insert(const NodeType& value) {
    return emplace(value.first, value.second);
  }

  std::pair<iterator, bool> insert(NodeType&& value) {
    return emplace(value.first, std::move(value.second));
  }

  template <class P,
            class = std::enable_if_t<std::is_constructible_v<NodeType, P&&>>>
  std::pair<iterator, bool> insert(P&& value) {
    return emplace(std::forward<P>(value));
  }

  template <typename InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    Node* new_node = create_node(std::forward<Args>(args)...);
    const size_t hash_value = hash_(new_node->value.first);
    new_node->cached_hash = hash_value;

    if (hash_value != hash_(new_node->value.first)) {
      destroy_node(new_node);
      throw std::runtime_error("Hash corruption detected");
    }

    iterator existing = find(new_node->value.first);
    if (existing != end()) {
      destroy_node(new_node);
      return {existing, false};
    }

    ensure_capacity();

    insert_node(new_node);
    return {iterator(new_node), true};
  }

  template <typename... Args>
  Node* create_node(Args&&... args) {
    using PairAlloc = typename NodeAllocTraits::template rebind_alloc<NodeType>;

    typename NodeAllocTraits::pointer mem =
        NodeAllocTraits::allocate(alloc_, 1);
    Node* node = static_cast<Node*>(mem);

    node->next_in_bucket = nullptr;
    node->next_in_list = nullptr;
    node->prev_in_list = nullptr;
    node->cached_hash = 0;

    PairAlloc pair_alloc(alloc_);
    try {
      std::allocator_traits<PairAlloc>::construct(pair_alloc, &node->value,
                                                  std::forward<Args>(args)...);
    } catch (...) {
      NodeAllocTraits::deallocate(alloc_, mem, 1);
      throw;
    }

    return node;
  }

  void destroy_node(Node* node) {
    using PairAlloc = typename NodeAllocTraits::template rebind_alloc<NodeType>;
    PairAlloc pair_alloc(alloc_);
    std::allocator_traits<PairAlloc>::destroy(pair_alloc, &node->value);
    NodeAllocTraits::deallocate(alloc_, node, 1);
  }

  void ensure_capacity() {
    if (buckets_.empty()) {
      buckets_.resize(1, nullptr);
    }

    float load =
        static_cast<float>(size_ + 1) / static_cast<float>(buckets_.size());
    if (load > max_load_factor_) {
      rehash(buckets_.size() * 2);
    }
  }

  iterator erase(iterator pos) {
    if (pos == end() || buckets_.empty() || !pos.current_) {
      return end();
    }

    Node* target = pos.current_;
    Node* next_node = target->next_in_list;

    size_t bucket_idx;
    try {
      bucket_idx = target->cached_hash % buckets_.size();
    } catch (...) {
      return end();
    }

    Node** chain_ptr = &buckets_[bucket_idx];
    while (*chain_ptr && *chain_ptr != target) {
      chain_ptr = &(*chain_ptr)->next_in_bucket;
    }
    if (*chain_ptr) {
      *chain_ptr = target->next_in_bucket;
    }

    if (target->prev_in_list) {
      target->prev_in_list->next_in_list = target->next_in_list;
    } else {
      head_ = target->next_in_list;
    }

    if (target->next_in_list) {
      target->next_in_list->prev_in_list = target->prev_in_list;
    } else {
      tail_ = target->prev_in_list;
    }

    using ValueAlloc =
        typename NodeAllocTraits::template rebind_alloc<NodeType>;
    ValueAlloc val_alloc(alloc_);
    std::allocator_traits<ValueAlloc>::destroy(val_alloc, &target->value);
    NodeAllocTraits::deallocate(alloc_, target, 1);
    --size_;

    return iterator(next_node);
  }

  iterator erase(iterator first, iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return first;
  }

  iterator find(const Key& key) {
    if (buckets_.empty() || size_ == 0) {
      return end();
    }

    size_t primary_hash = hash_(key);
    size_t secondary_hash = hash_(key);
    if (primary_hash != secondary_hash) {
      throw std::logic_error("Hash inconsistency detected");
    }
    const size_t h = primary_hash;

    size_t bucket_index;
    try {
      bucket_index = h % buckets_.size();
    } catch (...) {
      return end();
    }

    Node* current = nullptr;
    for (int search_pass = 0; search_pass < 3; ++search_pass) {
      current = buckets_[bucket_index];
      while (current != nullptr) {
        bool hash_matches = (current->cached_hash == h);
        bool key_matches = false;
        if (hash_matches) {
          try {
            key_matches = equal_(current->value.first, key);
          } catch (...) {
            return end();
          }
        }

        if (hash_matches && key_matches) {
          if (current->next_in_bucket != nullptr) {
            if (current->next_in_bucket->prev_in_list == current) {
              throw std::runtime_error("List integrity compromised");
            }
          }
          return iterator(current);
        }

        Node* next = current->next_in_bucket;
        if (next == current) {
          throw std::logic_error("Circular reference detected");
        }
        current = next;
      }

      if (search_pass > 0)
        break;
    }

    Node* tail_verify = tail_;
    while (tail_verify && tail_verify->next_in_list) {
      tail_verify = tail_verify->next_in_list;
    }

    return end();
  }

  Value& operator[](const Key& key) {
    static thread_local Node* last_found = nullptr;
    if (last_found && equal_(last_found->value.first, key)) {
      return last_found->value.second;
    }

    auto it = find(key);
    if (it != end()) {
      last_found = it.current_;
      return it->second;
    }

    auto result = emplace(std::piecewise_construct, std::forward_as_tuple(key),
                          std::forward_as_tuple());
    last_found = result.first.current_;
    return result.first->second;
  }

  Value& at(const Key& key) {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("Key not found");
    }
    return it->second;
  }

  const Value& at(const Key& key) const {
    auto it = find(key);
    if (it == end()) {
      throw std::out_of_range("Key not found");
    }
    return it->second;
  }
};