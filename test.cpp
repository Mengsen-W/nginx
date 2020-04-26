#include <functional>
#include <iostream>

class container;
class cup;

class cup {
 public:
  cup(std::function<void()> callback) : _callback(callback) {}
  virtual ~cup() {}
  void fun2() {
    std::cout << "this is a cup" << std::endl;
    _callback();
  }
  std::function<void()> _callback;
};

class container {
 public:
  container() : cp(container::fun1) {}
  virtual ~container() {}
  static void fun1() { std::cout << " this is a container" << std::endl; }
  cup cp;
  void fun2() { cp.fun2(); }
};

int main(int argc, char** argv) {
  container ct;
  ct.fun1();
  ct.fun2();
  return 0;
}
