#include <fcntl.h>
#include <ncurses.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "parser.h"
#include "ui.h"
#include <cstdio>
#include <map>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <PID>\n", argv[0]);
    return 1;
  }
  pid_t pid = (pid_t)std::atoi(argv[1]);
  if (pid <= 0) {
    fprintf(stderr, "Incorrect PID\n");
    return 1;
  }
  if (access(("/proc/" + std::to_string(pid)).c_str(), F_OK)) {
    fprintf(stderr, "No such process or permission denied.\n");
    return 1;
  }

  Parser parser = Parser(pid);

  parser.parse_maps();
  parser.parse_pagemap();

  auto tree = parser.get_tree();
  auto total_pages = parser.get_total_pages();

  Ui ui = Ui(pid, tree, total_pages);
  ui.run();
}
