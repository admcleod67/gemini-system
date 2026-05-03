#include <cstdio>

#include <pick_system/version.hpp>
#include <pickvm/core.hpp>
#include <DefaultFileSystemRoot.h>
#include <Shell.h>

int main() {
  std::printf("%s %s\n", pick_system::system_title, pick_system::version_string);
  PickVM::Runtime vm;
  PickShell::Shell shell(vm);
  applyDefaultFileSystemRoot(shell);
  shell.run();
  return 0;
}
