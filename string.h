#include <algorithm>
#include <cstring>
#include <iostream>

class String {
private:
  char* data_;
  size_t size_;
  size_t capacity_;

  void sufficient_capacity(size_t new_capacity) {
    if (new_capacity > capacity_) {
      capacity_ = std::max(new_capacity, 2 * capacity_);
      char* new_data = new char[capacity_ + 1];
      std::copy(data_, data_ + size_, new_data);
      delete[] data_;
      data_ = new_data;
    }
  }

public:
  String() : data_(new char[1]{'\0'}), size_(0), capacity_(0) {}

  String(const char* s) : size_(std::strlen(s)), capacity_(size_) {
    data_ = new char[capacity_ + 1];
    std::copy(s, s + size_, data_);
    data_[size_] = '\0';
  }

  String(size_t n, char c) : size_(n), capacity_(n) {
    data_ = new char[capacity_ + 1];
    std::fill(data_, data_ + size_, c);
    data_[size_] = '\0';
  }

  String(const String& other) : size_(other.size_), capacity_(other.size_) {
    data_ = new char[capacity_ + 1];
    std::copy(other.data_, other.data_ + size_, data_);
    data_[size_] = '\0';
  }

  String& operator=(const String& other) {
    if (this != &other) {
      if (capacity_ < other.size_) {
        delete[] data_;
        capacity_ = other.size_;
        data_ = new char[capacity_ + 1];
      }
      size_ = other.size_;
      std::copy(other.data_, other.data_ + size_, data_);
      data_[size_] = '\0';
    }
    return *this;
  }

  ~String() { delete[] data_; }

  size_t length() const { return size_; }

  size_t size() const { return size_; }

  size_t capacity() const { return capacity_; }

  bool empty() const { return size_ == 0; }

  void clear() {
    size_ = 0;
    data_[0] = '\0';
  }

  void shrink_to_fit() {
    if (size_ < capacity_) {
      char* new_data = new char[size_ + 1];
      std::copy(data_, data_ + size_, new_data);
      delete[] data_;
      data_ = new_data;
      capacity_ = size_;
      data_[size_] = '\0';
    }
  }

  void push_back(char c) {
    sufficient_capacity(size_ + 1);
    data_[size_++] = c;
    data_[size_] = '\0';
  }

  void pop_back() {
    if (size_ > 0) {
      data_[--size_] = '\0';
    }
  }

  char& operator[](size_t index) { return data_[index]; }

  const char& operator[](size_t index) const { return data_[index]; }

  char& front() { return data_[0]; }

  const char& front() const { return data_[0]; }

  char& back() { return data_[size_ - 1]; }

  const char& back() const { return data_[size_ - 1]; }

  String& operator+=(const char c) {
    push_back(c);
    return *this;
  }

  String& operator+=(const String& other) {
    sufficient_capacity(size_ + other.size_);
    std::copy(other.data_, other.data_ + other.size_, data_ + size_);
    size_ += other.size_;
    data_[size_] = '\0';
    return *this;
  }

  friend String operator+(const String& left, const String& right) {
    String result(left);
    result += right;
    return result;
  }

  friend String operator+(const String& left, char right) {
    String result(left);
    result += right;
    return result;
  }

  friend String operator+(char left, const String& right) {
    String result(1, left);
    result += right;
    return result;
  }

  size_t find(const String& substring) const {
    if (substring.size_ == 0)
      return 0;
    for (size_t i = 0; i <= size_ - substring.size_; ++i) {
      if (std::memcmp(data_ + i, substring.data_, substring.size_) == 0) {
        return i;
      }
    }
    return size_;
  }
  size_t rfind(const String& substring) const {
    if (substring.size_ == 0)
      return 0;
    for (size_t i = size_ - substring.size_ + 1; i-- > 0;) {
      if (std::memcmp(data_ + i, substring.data_, substring.size_) == 0) {
        return i;
      }
    }
    return size_;
  }
  String substr(size_t start, size_t count) const {
    //if (start >= size_)
    //  return String();
    size_t actual_count = std::min(count, size_ - start);
    String result(actual_count, '0');
    std::copy(data_ + start, data_ + start + actual_count, result.data_);
    result.data_[actual_count] = '\0';
    result.size_ = actual_count;
    return result;
  }
  const char* data() const { return data_; }

  char* data() { return data_; }
  friend bool operator==(const String& left, const String& right) {
    return left.size_ == right.size_ &&
           std::memcmp(left.data_, right.data_, left.size_) == 0;
  }
  friend bool operator!=(const String& left, const String& right) {
    return !(left == right);
  }
  friend bool operator<(const String& left, const String& right) {
    int cmp =
        std::memcmp(left.data_, right.data_, std::min(left.size_, right.size_));
    return cmp < 0 || (cmp == 0 && left.size_ < right.size_);
  }
  friend bool operator>(const String& left, const String& right) {
    return right < left;
  }

  friend bool operator<=(const String& left, const String& right) {
    return !(right < left);
  }

  friend bool operator>=(const String& left, const String& right) {
    return !(left < right);
  }
  friend std::ostream& operator<<(std::ostream& os, const String& str) {
    os.write(str.data_, str.size_);
    return os;
  }
  friend std::istream& operator>>(std::istream& is, String& str) {
    str.clear();
    char c;
    while (is.get(c) && !std::isspace(c)) {
      str.push_back(c);
    }
    return is;
  }
};

