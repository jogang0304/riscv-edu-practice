#pragma once

#include "helpers.h"
#include <fstream>
#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

static constexpr uint64_t PAGE_SIZE = 4096ULL;
static constexpr uint64_t PM_PRESENT = 1ULL << 63;
static constexpr uint64_t PM_SWAPPED = 1ULL << 62;
static constexpr uint64_t PM_FILE = 1ULL << 61;
static constexpr uint64_t PM_SOFT_DIRTY = 1ULL << 55;
static constexpr uint64_t PM_PFN_MASK = (1ULL << 55) - 1ULL;

struct MapsEntry {
  size_t start, end;
  bool r, w, x, s;
  std::string name;
};

struct PageInfo {
  uint64_t virtual_address;
  uint64_t pfn;
  bool present, swapped, file_backed, soft_dirty;
  bool r, w, x, shared;
  std::string vma_name; // from maps
};

struct PMDNode {
  uint64_t idx;
  std::map<uint64_t, PageInfo> pages;
};
struct PUDNode {
  uint64_t idx;
  std::map<uint64_t, PMDNode> pmds;
};
struct PGDNode {
  uint64_t idx;
  std::map<uint64_t, PUDNode> puds;
};

class Parser {
  std::string maps_file;
  std::string pagemap_file;
  std::vector<MapsEntry> maps;
  std::map<size_t, PGDNode> pagetree;
  int total_pages; // TODO maybe remove

public:
  Parser(pid_t pid);
  void parse_maps();
  void parse_pagemap();
  const std::map<size_t, PGDNode> &get_tree() const;
  int get_total_pages() const;
};
