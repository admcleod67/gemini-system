#include <cstdio>

#include <pick_system/version.hpp>

int main() {
  std::printf("pick-system %s\n", pick_system::version_string);
  return 0;
}
