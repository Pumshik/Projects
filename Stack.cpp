#include <cstring>
#include <iostream>

const int cInit_cap = 10;

char** data;
int capacity;
int top;

void initialize() {
  capacity = cInit_cap;
  data = new char* [capacity];
  top = 0;
}

void resize() {
  capacity *= 2;
  char** new_data = new char* [capacity];
  for (int i = 0; i < top; ++i) {
    new_data[i] = data[i];
  }
  delete[] data;
  data = new_data;
}

void push(const char* str) {
  if (top == capacity) {
    resize();
  }

  data[top] = new char[strlen(str) + 1];
  strcpy(data[top], str);
  top++;
  std::cout << "ok"
            << "\n";
}

void pop() {
  if (top == 0) {
    std::cout << "error"
              << "\n";
    return;
  }
  top--;
  std::cout << data[top] << "\n";
  delete[] data[top];
}

void back() {
  if (top == 0) {
    std::cout << "error"
              << "\n";
    return;
  }
  std::cout << data[top - 1] << "\n";
}

int size() { 
  return top;
}

void clear() {
  for (int i = 0; i < top; ++i) {
    delete[] data[i];
  }
  top = 0;
  std::cout << "ok"
            << "\n";
}

void exit() {
  for (int i = 0; i < top; ++i) {
    delete[] data[i];
  }
  top = 0;
  delete[] data;
}

char *read_string() {
  size_t capacity = 100;
  size_t size = 0;
  char* buffer = new char[capacity];

  while (true) {
    char ch = getchar();
    if (ch == '\n') {
      break;
    }
    if (size >= capacity) {
      capacity *= 2;
      char* new_buffer = new char[capacity];
      for (int i = 0; i < capacity / 2; ++i) {
        new_buffer[i] = buffer[i];
      }
      delete[] buffer;
      buffer = new_buffer;
    }

    buffer[size++] = ch;
  }

  buffer[size] = '\0';
  return buffer;
}


int main() {
  initialize();

  char command[10];
  char* buffer;

  while (true) {
    std::cin >> command;

    if (strcmp(command, "push") == 0) {
      buffer = read_string();
      push(buffer);
      delete[] buffer;
    } else if (strcmp(command, "pop") == 0) {
      pop();
    } else if (strcmp(command, "back") == 0) {
      back();
    } else if (strcmp(command, "size") == 0) {
      std::cout << size() << "\n";
    } else if (strcmp(command, "clear") == 0) {
      clear();
    } else if (strcmp(command, "exit") == 0) {
      exit();
      std::cout << "bye"
                << "\n";
      break;
    }
  }

  return 0;
}