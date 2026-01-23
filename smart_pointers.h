#include <memory>
#include <type_traits>
#include <utility>

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

struct BaseControlBlock {
  size_t shared_count = 0;
  size_t weak_count = 0;

  virtual ~BaseControlBlock() = default;
  virtual void destroy() noexcept = 0;
  virtual void deallocate() noexcept = 0;
  virtual void* get_ptr() = 0;
};

template <typename T, typename Deleter, typename Alloc>
struct PointerControlBlock : BaseControlBlock {
  T* ptr;
  Deleter deleter;
  Alloc alloc;

  explicit PointerControlBlock(T* ptr, Deleter deleter, Alloc alloc)
      : ptr(ptr), deleter(std::move(deleter)), alloc(std::move(alloc)) {
    this->shared_count = 1;
  }

  void destroy() noexcept override {
    if (ptr) {
      deleter(ptr);
      ptr = nullptr;
    }
  }

  void deallocate() noexcept override {
    using BlockAlloc = typename std::allocator_traits<
        Alloc>::template rebind_alloc<PointerControlBlock>;
    BlockAlloc block_alloc(alloc);
    this->~PointerControlBlock();
    block_alloc.deallocate(this, 1);
  }

  void* get_ptr() override { return ptr; }
};

template <typename T, typename Alloc, typename... Args>
struct ObjectControlBlock : BaseControlBlock {
  T object;
  Alloc alloc;

  template <typename... Args_>
  explicit ObjectControlBlock(Alloc alloc, Args_&&... args)
      : object(std::forward<Args_>(args)...), alloc(std::move(alloc)) {
    this->shared_count = 1;
  }

  void destroy() noexcept override {
    std::allocator_traits<Alloc>::destroy(alloc, &object);
  }

  void deallocate() noexcept override {
    using BlockAlloc = typename std::allocator_traits<
        Alloc>::template rebind_alloc<ObjectControlBlock>;
    BlockAlloc block_alloc(alloc);
    block_alloc.deallocate(this, 1);
  }

  void* get_ptr() override { return &object; }
};

template <typename T>
class SharedPtr {
 private:
  T* ptr = nullptr;
  BaseControlBlock* cb = nullptr;

  template <typename Y>
  friend class WeakPtr;

  template <typename Y>
  friend class SharedPtr;

  explicit SharedPtr(BaseControlBlock* cb) : cb(cb) {
    ptr = static_cast<T*>(cb->get_ptr());
    if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
      auto enable = static_cast<EnableSharedFromThis<T>*>(ptr);
      enable->weak_this = *this;
    }
  }

 public:
  SharedPtr() = default;

  template <typename U, typename... Args>
  friend SharedPtr<U> makeShared(Args&&...);

  template <typename U, typename Alloc, typename... Args>
  friend SharedPtr<U> allocateShared(const Alloc&, Args&&...);

  template <typename Y,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  explicit SharedPtr(Y* ptr)
      : SharedPtr(ptr, std::default_delete<Y>(), std::allocator<void>()) {}

  template <typename Y, typename Deleter, typename Alloc>
  explicit SharedPtr(Y* ptr, Deleter deleter, Alloc alloc) {
    using ActualBlock = PointerControlBlock<Y, Deleter, Alloc>;
    using BlockAlloc = typename std::allocator_traits<
        Alloc>::template rebind_alloc<ActualBlock>;

    BlockAlloc block_alloc(alloc);
    ActualBlock* block = block_alloc.allocate(1);
    new (block) ActualBlock(ptr, std::move(deleter), std::move(alloc));

    cb = block;
    this->ptr = ptr;

    if constexpr (std::is_base_of_v<EnableSharedFromThis<Y>, Y>) {
      auto enable = static_cast<EnableSharedFromThis<Y>*>(ptr);
      enable->weak_this = *this;
    }
  }

  template <typename Y,
            typename Deleter,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  SharedPtr(Y* ptr, Deleter deleter)
      : SharedPtr(ptr, std::move(deleter), std::allocator<void>()) {}

  SharedPtr(const SharedPtr& other) noexcept : ptr(other.ptr), cb(other.cb) {
    if (cb) {
      ++cb->shared_count;
    }
  }

  template <typename Y,
            typename = std::enable_if_t<std::is_convertible_v<Y*, T*>>>
  SharedPtr(const SharedPtr<Y>& other) noexcept : ptr(other.ptr), cb(other.cb) {
    ++cb->shared_count;
  }

  template <typename Y>
  explicit SharedPtr(SharedPtr<Y>&& other) noexcept
      : ptr(other.ptr), cb(other.cb) {
    other.ptr = nullptr;
    other.cb = nullptr;
  }

  SharedPtr& operator=(const SharedPtr& other) noexcept {
    SharedPtr(other).swap(*this);
    return *this;
  }

  template <typename Y>
  SharedPtr& operator=(SharedPtr<Y>&& other) noexcept {
    SharedPtr temp(std::move(other));
    temp.swap(*this);
    return *this;
  }

  SharedPtr& operator=(SharedPtr&& other) noexcept {
    SharedPtr(std::move(other)).swap(*this);
    return *this;
  }

  ~SharedPtr() {
    if (cb) {
      if (--cb->shared_count == 0) {
        cb->destroy();
        if (cb->weak_count == 0) {
          cb->deallocate();
        }
      }
    }
  }

  void swap(SharedPtr& other) noexcept {
    std::swap(ptr, other.ptr);
    std::swap(cb, other.cb);
  }

  void reset() noexcept { SharedPtr().swap(*this); }

  template <typename Y,
            typename Deleter = std::default_delete<T>,
            typename Alloc = std::allocator<void>>
  void reset(Y* new_ptr, Deleter deleter = Deleter(), Alloc alloc = Alloc()) {
    SharedPtr<T>(new_ptr, std::move(deleter), std::move(alloc)).swap(*this);
  }

  T* get() const noexcept { return ptr; }
  T& operator*() const noexcept { return *ptr; }
  T* operator->() const noexcept { return ptr; }
  size_t use_count() const noexcept { return cb->shared_count; }
};

template <typename T>
class WeakPtr {
 private:
  BaseControlBlock* cb = nullptr;

  template <typename U>
  friend class WeakPtr;

 public:
  WeakPtr() = default;

  WeakPtr(const WeakPtr& other) noexcept : cb(other.cb) { ++cb->weak_count; }

  template <typename Y>
  WeakPtr(const SharedPtr<Y>& other) noexcept : cb(other.cb) {
    ++cb->weak_count;
  }

  template <typename Y>
  WeakPtr(const WeakPtr<Y>& other) noexcept : cb(other.cb) {
    ++cb->weak_count;
  }

  template <typename Y>
  WeakPtr(WeakPtr<Y>&& other) noexcept : cb(other.cb) {
    other.cb = nullptr;
  }

  ~WeakPtr() {
    if (!cb)
      return;
    if (--cb->weak_count == 0 && cb->shared_count == 0) {
      cb->deallocate();
    }
    cb = nullptr;
  }

  WeakPtr& operator=(const WeakPtr& other) noexcept {
    WeakPtr(other).swap(*this);
    return *this;
  }

  WeakPtr& operator=(WeakPtr&& other) noexcept {
    WeakPtr temp(std::move(other));
    temp.swap(*this);
    return *this;
  }

  bool expired() const noexcept { return !cb || cb->shared_count == 0; }

  SharedPtr<T> lock() const noexcept {
    if (!expired() && cb->shared_count > 0) {
      ++cb->shared_count;
      return SharedPtr<T>(cb);
    }
    return SharedPtr<T>();
  }

  void swap(WeakPtr& other) noexcept { std::swap(cb, other.cb); }

  void reset() noexcept {
    if (--cb->weak_count == 0 && cb->shared_count == 0) {
      cb->deallocate();
    }
    cb = nullptr;
  }

  size_t use_count() const noexcept { return cb->shared_count; }
};

template <typename T>
class EnableSharedFromThis {
 protected:
  WeakPtr<T> weak_this;

  template <typename U>
  friend class SharedPtr;

 public:
  SharedPtr<const T> shared_from_this() const{
    if (weak_this.expired()) {
      throw std::bad_weak_ptr();
    }
    return weak_this.lock();
  }

  SharedPtr<T> shared_from_this() {
    auto const_ptr = static_cast<const EnableSharedFromThis*>(this)->shared_from_this();
    return SharedPtr<T>(const_cast<T*>(const_ptr.get()), const_ptr.cb);
  }

  virtual ~EnableSharedFromThis() = default;
};

template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(const Alloc& alloc_, Args&&... args) {
  using Block = ObjectControlBlock<T, Alloc, Args...>;
  using BlockAlloc =
      typename std::allocator_traits<Alloc>::template rebind_alloc<Block>;
  BlockAlloc alloc(alloc_);
  Block* block = alloc.allocate(1);
  std::allocator_traits<BlockAlloc>::construct(alloc, block, alloc_,
                                               std::forward<Args>(args)...);
  return SharedPtr<T>(block);
}
template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  return allocateShared<T>(std::allocator<void>(), std::forward<Args>(args)...);
}