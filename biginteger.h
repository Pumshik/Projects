#pragma once

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <vector>

class BigInteger {
 private:
  std::vector<uint32_t> m_digits;
  bool m_negative = false;

  static const uint32_t CELL_BASE = 1'000'000'000;
  static const uint32_t CELL_LEN = 9;

 private:
  void remove_leading_zeroes() {
    if (m_digits.size() == 0)
      return;

    auto it = std::find_if(m_digits.rbegin(), m_digits.rend(),
                           [](uint32_t x) { return x != 0; });

    if (it == m_digits.rend()) {
      m_digits = {0};
      m_negative = false;
    } else {
      m_digits.erase(it.base(), m_digits.end());
    }
  }

  void shift_left() { m_digits.insert(m_digits.begin(), 0); }
  void add_abs(const BigInteger& other) {
    size_t max_size = std::max(other.m_digits.size(), m_digits.size());

    m_digits.resize(max_size);

    uint32_t carry = 0;

    for (size_t i = 0; (i < max_size) || carry; ++i) {
      if (i == m_digits.size())
        m_digits.push_back(0);
      uint64_t sum = static_cast<uint64_t>(m_digits[i]) + carry;
      if (i < other.m_digits.size())
        sum += other.m_digits[i];

      m_digits[i] = sum % CELL_BASE;
      carry = sum / CELL_BASE;
    }

    remove_leading_zeroes();
  }

  void sub_abs(const BigInteger& other) {
    uint32_t borrow = 0;
    for (size_t i = 0; i < m_digits.size(); ++i) {
      uint32_t cell_to_sub =
          (i < other.m_digits.size() ? other.m_digits[i] : 0);
      int64_t diff = static_cast<int64_t>(m_digits[i]) -
                     static_cast<int64_t>(borrow) -
                     static_cast<int64_t>(cell_to_sub);
      if (diff < 0) {
        diff += CELL_BASE;
        borrow = 1;
      } else {
        borrow = 0;
      }
      m_digits[i] = diff;

      remove_leading_zeroes();
    }
  }

 public:
  BigInteger() : m_digits({0}), m_negative(false) {}

  BigInteger(int num) {
    m_negative = num < 0;
    num = std::abs(num);
    m_digits.clear();
    if (num == 0) {
      m_digits.push_back(0);
    } else {
      while (num > 0) {
        m_digits.push_back(num % CELL_BASE);
        num /= CELL_BASE;
      }
    }
  }

  BigInteger(const std::string& s) {
    if (s.empty() || !is_correct_str(s)) {
      *this = BigInteger();
      return;
    }

    int64_t start = 0;
    if (s[0] == '-') {
      m_negative = true;
      start = 1;
    } else if (s[0] == '+') {
      start = 1;
    }

    for (int64_t i = s.size() - 1; i >= start;
         i -= static_cast<int64_t>(CELL_LEN)) {
      int64_t end = std::max(start, i + 1 - static_cast<int64_t>(CELL_LEN));

      std::string part = s.substr(end, i - end + 1);
      m_digits.push_back(std::stoul(part));
    }
    remove_leading_zeroes();
    if (m_digits.empty())
      m_digits.push_back(0);
  }

  BigInteger abs() const {
    BigInteger result = *this;
    result.m_negative = false;
    return result;
  }

  BigInteger& operator+=(const BigInteger& other) {
    if (m_negative == other.m_negative) {
      add_abs(other);
    } else {
      if (abs() < other.abs()) {
        BigInteger temp = other;
        temp.sub_abs(*this);
        *this = temp;
        m_negative = other.m_negative;
      } else {
        sub_abs(other);
      }
    }

    return *this;
  }

  BigInteger& operator-=(const BigInteger& other) {
    if (other == *this) {
      m_digits = {0};
    } else {
      m_negative = !m_negative;
      *this += other;
      m_negative = !m_negative;
    }

    if (is_zero())
      m_negative = false;

    return *this;
  }

  BigInteger& operator*=(const BigInteger& other) {
    BigInteger result;
    result.m_digits.resize(m_digits.size() + other.m_digits.size() + 1, 0);
    result.m_negative = m_negative != other.m_negative;

    for (size_t i = 0; i < m_digits.size(); ++i) {
      uint64_t carry = 0;
      for (size_t j = 0; j < other.m_digits.size() || carry; ++j) {
        uint64_t product =
            result.m_digits[i + j] + carry +
            static_cast<uint64_t>(m_digits[i]) *
                (j < other.m_digits.size() ? other.m_digits[j] : 0);
        result.m_digits[i + j] = product % CELL_BASE;
        carry = product / CELL_BASE;
      }
    }

    result.remove_leading_zeroes();
    *this = result;
    return *this;
  }

  BigInteger& operator/=(const BigInteger& other) {
    bool result_negative = m_negative != other.m_negative;
    BigInteger a = abs();
    BigInteger b = other.abs();

    if (a == b) {
      m_digits = {1};
      m_negative = result_negative;
      return *this;
    }

    if (a < b) {
      *this = 0;
      return *this;
    }

    BigInteger result;
    result.m_negative = result_negative;
    result.m_digits.resize(a.m_digits.size() - b.m_digits.size() + 1, 0);

    BigInteger curValue;
    for (int iter = a.m_digits.size() - 1; iter >= 0; --iter) {
      curValue.shift_left();
      if (curValue.m_digits.empty()) {
        curValue.m_digits.push_back(a.m_digits[iter]);
      } else {
        curValue.m_digits[0] = a.m_digits[iter];
      }
      curValue.remove_leading_zeroes();

      int val = 0;
      int left = 0, right = CELL_BASE - 1;
      while (left <= right) {
        int mid = (left + right) >> 1;
        BigInteger cur = b * mid;
        if (cur <= curValue) {
          val = mid;
          left = mid + 1;
        } else {
          right = mid - 1;
        }
      }

      if (iter < static_cast<int>(result.m_digits.size())) {
        result.m_digits[iter] = val;
      } else {
        result.m_digits.push_back(val);
      }

      curValue -= b * val;
    }

    result.remove_leading_zeroes();
    *this = result;
    return *this;
  }

  BigInteger& operator%=(const BigInteger& other) {
    BigInteger div_res = *this / other;
    *this -= div_res * other;
    return *this;
  }

  BigInteger operator-() const {
    BigInteger number = *this;
    if (number != 0)
      number.m_negative = !number.m_negative;
    return number;
  }

  BigInteger& operator++() {
    *this += 1;
    return *this;
  }

  BigInteger operator++(int) {
    BigInteger temp = *this;
    ++*this;
    return temp;
  }

  BigInteger& operator--() {
    *this -= 1;
    return *this;
  }

  BigInteger operator--(int) {
    BigInteger temp = *this;
    --*this;
    return temp;
  }

  std::string toString() const {
    if (is_zero())
      return "0";

    std::stringstream ss;
    if (m_negative)
      ss << '-';
    ss << m_digits.back();
    for (int i = m_digits.size() - 2; i >= 0; --i) {
      ss << std::string(CELL_LEN - std::to_string(m_digits[i]).size(), '0')
         << m_digits[i];
    }

    return ss.str();
  }

  explicit operator bool() const { return !is_zero(); }

  bool is_zero() const {
    return ((m_digits.size() == 1) && (m_digits[0] == 0));
  }

  static bool is_correct_str(const std::string& s) {
    if (s.empty())
      return true;

    int64_t start = 0;
    if (s[0] == '-' || s[0] == '+')
      start = 1;

    for (int64_t i = start; i < static_cast<int64_t>(s.length()); ++i) {
      if (!isdigit(s[i]))
        return false;
    }

    return true;
  }

  bool isNegative() const { return m_negative; }

  friend bool operator==(const BigInteger& left, const BigInteger& right);
  friend bool operator!=(const BigInteger& left, const BigInteger& right);
  friend bool operator<(const BigInteger& left, const BigInteger& right);
  friend bool operator<=(const BigInteger& left, const BigInteger& right);
  friend bool operator>(const BigInteger& left, const BigInteger& right);
  friend bool operator>=(const BigInteger& left, const BigInteger& right);
  friend std::ostream& operator<<(std::ostream& os, const BigInteger& num);
  friend std::istream& operator>>(std::istream& is, BigInteger& num);
  friend BigInteger operator/(BigInteger left, const BigInteger& right);
  friend BigInteger operator%(BigInteger left, const BigInteger& right);
  friend BigInteger operator+(BigInteger left, const BigInteger& right);
  friend BigInteger operator-(BigInteger left, const BigInteger& right);
  friend BigInteger operator*(BigInteger left, const BigInteger& right);
  friend BigInteger operator"" _bi(const char* str);
  friend BigInteger operator"" _bi(const char* str, size_t);
};

bool operator==(const BigInteger& left, const BigInteger& right) {
  return (left.m_negative == right.m_negative) &&
         (left.m_digits == right.m_digits);
}

bool operator!=(const BigInteger& left, const BigInteger& right) {
  return !(left == right);
}

bool operator<(const BigInteger& left, const BigInteger& right) {
  if (left.m_negative != right.m_negative)
    return left.m_negative;

  bool less = false;
  if (left.m_digits.size() != right.m_digits.size()) {
    less = (left.m_digits.size() < right.m_digits.size());
    if (left.m_negative)
      less = !less;
    return less;
  }

  for (int i = left.m_digits.size() - 1; i >= 0; --i) {
    if (left.m_digits[i] != right.m_digits[i]) {
      less = left.m_digits[i] < right.m_digits[i];
      if (left.m_negative)
        less = !less;
      return less;
    }
  }

  return false;
}

bool operator<=(const BigInteger& left, const BigInteger& right) {
  return !(left > right);
}

bool operator>(const BigInteger& left, const BigInteger& right) {
  return right < left;
}

bool operator>=(const BigInteger& left, const BigInteger& right) {
  return !(left < right);
}

std::ostream& operator<<(std::ostream& os, const BigInteger& num) {
  os << num.toString();
  return os;
}

std::istream& operator>>(std::istream& is, BigInteger& num) {
  std::string s;
  is >> s;
  num = BigInteger(s);
  return is;
}

BigInteger operator/(BigInteger left, const BigInteger& right) {
  left /= right;
  return left;
}

BigInteger operator%(BigInteger left, const BigInteger& right) {
  left %= right;
  return left;
}

BigInteger operator+(BigInteger left, const BigInteger& right) {
  left += right;
  return left;
}

BigInteger operator-(BigInteger left, const BigInteger& right) {
  left -= right;
  return left;
}

BigInteger operator*(BigInteger left, const BigInteger& right) {
  left *= right;
  return left;
}

BigInteger operator""_bi(const char* str) {
  return BigInteger(std::string(str));
}

BigInteger operator""_bi(const char* str, size_t len) {
  return BigInteger(std::string(str, len));
}

BigInteger operator""_bi(unsigned long long num) {
  return BigInteger(std::to_string(num));
}

class Rational {
 private:
  BigInteger numerator;
  BigInteger denominator;

  void normalize() {
    if (denominator < 0) {
      numerator = -numerator;
      denominator = -denominator;
    }
    BigInteger g = gcd(numerator, denominator);
    if (g != 0) {
      numerator /= g;
      denominator /= g;
    }
  }

  static BigInteger gcd(BigInteger a, BigInteger b) {
    a = a.abs();
    b = b.abs();
    while (b != 0) {
      a %= b;
      std::swap(a, b);
    }
    return a;
  }

 public:
  Rational() : numerator(0), denominator(1) {}
  Rational(int num) : numerator(num), denominator(1) {}
  Rational(const BigInteger& num) : numerator(num), denominator(1) {}
  Rational(const BigInteger& num, const BigInteger& denom)
      : numerator(num), denominator(denom) {
    normalize();
  }

  Rational& operator+=(const Rational& other) {
    numerator = numerator * other.denominator + other.numerator * denominator;
    denominator *= other.denominator;
    normalize();
    return *this;
  }

  Rational& operator-=(const Rational& other) {
    numerator = numerator * other.denominator - other.numerator * denominator;
    denominator *= other.denominator;
    normalize();
    return *this;
  }

  Rational& operator*=(const Rational& other) {
    numerator *= other.numerator;
    denominator *= other.denominator;
    normalize();
    return *this;
  }

  Rational& operator/=(const Rational& other) {
    numerator *= other.denominator;
    denominator *= other.numerator;
    normalize();
    return *this;
  }

  bool isNegative() const {
    return numerator.isNegative() != denominator.isNegative();
  }

  Rational operator-() const { return Rational(-numerator, denominator); }

  std::string toString() const {
    if (denominator == 1)
      return numerator.toString();
    return numerator.toString() + "/" + denominator.toString();
  }

  std::string asDecimal(size_t precision = 0) const {
    BigInteger num = numerator;
    BigInteger den = denominator;
    BigInteger integerPart = num / den;
    num %= den;

    std::string result;
    if (num < 0 && integerPart == 0)
      result += "-";
    result += integerPart.toString();

    if (precision == 0)
      return result;

    result += ".";
    num = num.abs() * 10;
    for (size_t i = 0; i < precision; ++i) {
      BigInteger digit = num / den;
      result += digit.toString();
      num = (num % den) * 10;
    }
    return result;
  }

  explicit operator double() const { return std::stod(asDecimal(15)); }

  friend bool operator==(const Rational& left, const Rational& right);
  friend bool operator<(const Rational& left, const Rational& right);
};

bool operator==(const Rational& left, const Rational& right) {
  return left.numerator * right.denominator ==
         right.numerator * left.denominator;
}

bool operator!=(const Rational& left, const Rational& right) {
  return !(left == right);
}

bool operator<(const Rational& left, const Rational& right) {
  return left.numerator * right.denominator <
         right.numerator * left.denominator;
}

bool operator>(const Rational& left, const Rational& right) {
  return right < left;
}

bool operator<=(const Rational& left, const Rational& right) {
  return !(right < left);
}

bool operator>=(const Rational& left, const Rational& right) {
  return !(left < right);
}

Rational operator+(Rational left, const Rational& right) {
  left += right;
  return left;
}

Rational operator-(Rational left, const Rational& right) {
  left -= right;
  return left;
}

Rational operator*(Rational left, const Rational& right) {
  left *= right;
  return left;
}

Rational operator/(Rational left, const Rational& right) {
  left /= right;
  return left;
}