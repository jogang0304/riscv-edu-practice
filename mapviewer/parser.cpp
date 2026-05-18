#include "parser.h"
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

inline uint64_t pgd_idx(uint64_t va) { return (va >> 39) & 0x1FF; }
inline uint64_t pud_idx(uint64_t va) { return (va >> 30) & 0x1FF; }
inline uint64_t pmd_idx(uint64_t va) { return (va >> 21) & 0x1FF; }
inline uint64_t pte_idx(uint64_t va) { return (va >> 12) & 0x1FF; }

Parser::Parser(pid_t pid)
    : maps_file("/proc/" + std::to_string(pid) + "/maps"),
      pagemap_file("/proc/" + std::to_string(pid) + "/pagemap") {}

void Parser::parse_maps() {
  std::ifstream f(this->maps_file);
  if (!f)
    return;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty())
      continue;
    MapsEntry e{};
    unsigned long long offset;
    unsigned int dev1, dev2, inode;
    char permissions[8], name[512] = {};
    int n =
        sscanf(line.c_str(), "%lx-%lx %7s %llx %x:%x %x %511[^\n]", &e.start,
               &e.end, permissions, &offset, &dev1, &dev2, &inode, name);
    e.r = (permissions[0] == 'r');
    e.w = (permissions[1] == 'w');
    e.x = (permissions[2] == 'x');
    e.s = (permissions[3] == 's');
    if (n >= 8) {
      e.name = trim(std::string(name));
    }
    this->maps.push_back(e);
  }
}

void Parser::parse_pagemap() {
  this->total_pages = 0;

  int fd = open(this->pagemap_file.c_str(), O_RDONLY);
  if (fd < 0) {
    throw std::runtime_error("Failed to open(" + this->pagemap_file +
                             "): " + strerror(errno));
  }

  for (auto &m : this->maps) {
    uint64_t number_of_pages = (m.end - m.start) / PAGE_SIZE;
    std::vector<uint64_t> entries(number_of_pages, 0);
    uint64_t offset = (m.start / PAGE_SIZE) * 8;
    if (lseek64(fd, (off64_t)offset, SEEK_SET) >= 0) {
      uint8_t *buf = (uint8_t *)entries.data();
      ssize_t total_read = 0, need = (ssize_t)(number_of_pages * 8);
      while (total_read < need) {
        ssize_t r = read(fd, buf + total_read, need - total_read);
        if (r <= 0)
          break;
        total_read += r;
      }
    }

    for (uint64_t i = 0; i < number_of_pages; i++) {
      uint64_t entry = entries[i];
      if (!(entry & PM_PRESENT) && !(entry & PM_SWAPPED))
        continue;
      uint64_t va = m.start + i * PAGE_SIZE;
      PageInfo pi{};
      pi.virtual_address = va;
      pi.present = !!(entry & PM_PRESENT);
      pi.swapped = !!(entry & PM_SWAPPED);
      pi.file_backed = !!(entry & PM_FILE);
      pi.soft_dirty = !!(entry & PM_SOFT_DIRTY);
      pi.pfn = pi.present ? (entry & PM_PFN_MASK) : 0;
      pi.r = m.r;
      pi.w = m.w;
      pi.x = m.x;
      pi.shared = m.s;
      pi.vma_name = m.name;

      auto &G = this->pagetree[pgd_idx(va)];
      G.idx = pgd_idx(va);
      auto &U = G.puds[pud_idx(va)];
      U.idx = pud_idx(va);
      auto &M = U.pmds[pmd_idx(va)];
      M.idx = pmd_idx(va);
      M.pages[pte_idx(va)] = pi;
      total_pages++;
    }
  }
  close(fd);
}

const std::map<size_t, PGDNode> &Parser::get_tree() const {
  return this->pagetree;
}

int Parser::get_total_pages() const { return this->total_pages; }
