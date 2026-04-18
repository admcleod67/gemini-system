#include <pick_system/version.hpp>

#include "core/vm/Runtime.h"
#include "tcl/Shell.h"

int main() {
  std::printf("%s %s\n", pick_system::system_title, pick_system::version_string);
  PickVM::Runtime vm;
  PickShell::Shell shell(vm);
  shell.run();
  return 0;
}
