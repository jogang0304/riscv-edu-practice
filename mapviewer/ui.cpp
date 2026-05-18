#include "ui.h"
#include "parser.h"
#include <iomanip>
#include <memory>
#include <ncurses.h>
#include <string>

static std::string to_hex16_fixedlen(uint64_t v) {
  std::ostringstream s;
  s << "0x" << std::hex << std::setw(16) << std::setfill('0') << v;
  return s.str();
}

static std::string get_flags_string(const PageInfo &p) {
  std::string s;
  s += p.r ? 'r' : '-';
  s += p.w ? 'w' : '-';
  s += p.x ? 'x' : '-';
  s += p.shared ? 's' : 'p';
  s += p.file_backed ? 'F' : 'A';
  s += p.soft_dirty ? 'D' : '.';
  return s;
}

Ui::Ui(pid_t pid, const std::map<uint64_t, PGDNode> &pgd_table,
       size_t total_pages)
    : pid(pid), root_node(std::make_shared<TreeNode>(
                    "PID " + std::to_string(pid) + "  [" +
                        std::to_string(total_pages) + " pages]",
                    NodeKind::ROOT, 0, true, false)) {
  for (auto &[gi, pgd] : pgd_table) {
    size_t pgd_pages = 0;
    for (auto &[ui, pud] : pgd.puds) {
      for (auto &[mi, pmd] : pud.pmds) {
        pgd_pages += pmd.pages.size();
      }
    }

    auto gn = std::make_shared<TreeNode>(
        "PGD[" + std::to_string(gi) + "]" + "  base " +
            to_hex16_fixedlen((uint64_t)gi << 39) + "  (" +
            std::to_string(pgd_pages) + " pages)",
        NodeKind::PGD, 1);

    for (auto &[ui, pud] : pgd.puds) {
      size_t pud_pages = 0;
      for (auto &[mi, pmd] : pud.pmds) {
        pud_pages += pmd.pages.size();
      }

      auto un = std::make_shared<TreeNode>(
          "PUD[" + std::to_string(ui) + "]" + "  base " +
              to_hex16_fixedlen(((uint64_t)gi << 39) | ((uint64_t)ui << 30)) +
              "  (" + std::to_string(pud_pages) + " pages)",
          NodeKind::PUD, 2);

      for (auto &[mi, pmd] : pud.pmds) {
        auto mn = std::make_shared<TreeNode>(
            "PMD[" + std::to_string(mi) + "]" + "  base " +
                to_hex16_fixedlen(((uint64_t)gi << 39) | ((uint64_t)ui << 30) |
                                  ((uint64_t)mi << 21)) +
                "  (" + std::to_string(pmd.pages.size()) + " ptes)",
            NodeKind::PMD, 3);

        for (auto &[ti, pi] : pmd.pages) {
          auto pn = std::make_shared<TreeNode>();
          pn->kind = NodeKind::PTE;
          pn->depth = 4;
          pn->leaf = true;
          pn->page = pi;
          std::ostringstream lbl;
          lbl << "PTE[" << std::setw(3) << ti << "]"
              << "  VA:" << to_hex16_fixedlen(pi.virtual_address);
          if (pi.present)
            lbl << "  PFN:0x" << std::hex << std::setw(9) << std::setfill('0')
                << pi.pfn << std::dec;
          else
            lbl << "  SWAPPED";
          lbl << "  [" << get_flags_string(pi) << "]";
          if (!pi.vma_name.empty())
            lbl << "  " << pi.vma_name;
          pn->label = lbl.str();
          mn->children.push_back(pn);
        }
        un->children.push_back(mn);
      }
      gn->children.push_back(un);
    }
    this->root_node->children.push_back(gn);
  }
}

static void flatten(const std::shared_ptr<TreeNode> &node,
                    std::vector<FlatItem> &out, int d, bool last,
                    std::vector<bool> has_siblings) {
  out.push_back({node, d, last, has_siblings});
  if (node->expanded) {
    std::vector<bool> child_sibs = has_siblings;
    child_sibs.push_back(!last);
    for (size_t i = 0; i < node->children.size(); i++) {
      bool child_is_last = (i + 1 == node->children.size());
      flatten(node->children[i], out, d + 1, child_is_last, child_sibs);
    }
  }
}

static void init_colors() {
  start_color();
  use_default_colors();
  init_pair(CP_NORMAL, COLOR_WHITE, -1);
  init_pair(CP_SELECTED, COLOR_BLACK, COLOR_WHITE);
  init_pair(CP_ROOT, COLOR_YELLOW, -1);
  init_pair(CP_PGD, COLOR_MAGENTA, -1);
  init_pair(CP_PUD, COLOR_CYAN, -1);
  init_pair(CP_PMD, COLOR_GREEN, -1);
  init_pair(CP_PTE, COLOR_WHITE, -1);
  init_pair(CP_HEADER, COLOR_RED, COLOR_CYAN + 8);
  init_pair(CP_STATUS, COLOR_BLACK, COLOR_WHITE);
  init_pair(CP_LEGEND, COLOR_BLACK + 8, -1);
}

static int node_color(NodeKind k) {
  switch (k) {
  case NodeKind::ROOT:
    return CP_ROOT;
  case NodeKind::PGD:
    return CP_PGD;
  case NodeKind::PUD:
    return CP_PUD;
  case NodeKind::PMD:
    return CP_PMD;
  case NodeKind::PTE:
    return CP_PTE;
  }
  return CP_NORMAL;
}

void Ui::draw_header(int cols, pid_t pid) {
  attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
  mvhline(0, 0, ' ', cols);
  std::string t =
      "  pagetree  PID:" + std::to_string(pid) + "  |  Enter=expand q=quit";
  mvaddnstr(0, 0, t.c_str(), cols);
  attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
}

void Ui::draw_status(int rows, int cols, const FlatItem *it, int total,
                     int cur) {
  attron(COLOR_PAIR(CP_STATUS));
  mvhline(rows - 1, 0, ' ', cols);
  std::string s =
      " item " + std::to_string(cur + 1) + "/" + std::to_string(total);
  if (it && it->node->kind == NodeKind::PTE) {
    const PageInfo &p = it->node->page;
    std::ostringstream ss;
    ss << " VA:" << to_hex16_fixedlen(p.virtual_address);
    if (p.present)
      ss << "  PA:" << to_hex16_fixedlen(p.pfn << 12) << "  PFN:0x" << std::hex
         << p.pfn;
    else
      ss << "  [SWAPPED]";
    ss << "  info:[" << get_flags_string(p) << "]";
    if (!p.vma_name.empty())
      ss << "  " << p.vma_name;
    s += "  " + ss.str();
  } else {
    if (it)
      s += "  " + it->node->label;
  }
  mvaddnstr(rows - 1, 0, s.c_str(), cols);
  attroff(COLOR_PAIR(CP_STATUS));
}

void Ui::draw_legend(int row, int cols) {
  attron(COLOR_PAIR(CP_LEGEND));
  std::string line =
      " [r]read [w]write [x]exec [s/p]shared/priv [F/A]file/anon [D]soft-dirty";
  mvaddnstr(row, 0, line.c_str(), cols);
  attroff(COLOR_PAIR(CP_LEGEND));
}

void Ui::draw_item(int row, int cols, const FlatItem &it, bool is_selected) {
  auto &n = it.node;

  std::string prefix = std::string(3 * it.depth, ' '); // 1 отступ = 3 символа
  if (it.depth > 0) {
    prefix += (it.is_last_child || n->expanded) ? "└─ " : "├─ ";
  }

  std::string expand_indicator; // 2 символа
  if (!n->leaf) {
    expand_indicator = n->expanded ? "▼ " : "▶ "; // ▼ ▶
  } else {
    expand_indicator = "  ";
  }

  std::string line = prefix + expand_indicator + n->label;

  if (is_selected) {
    attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
    while ((int)line.size() < cols) { // чтобы заливка во всю ширину была
      line += ' ';
    }
    mvaddnstr(row, 0, line.c_str(), cols);
    attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
  } else {
    attron(COLOR_PAIR(node_color(n->kind)));
    mvaddnstr(row, 0, line.c_str(), cols);
    attroff(COLOR_PAIR(node_color(n->kind)));
  }
}

void Ui::run() {
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  init_colors();

  int selected = 0, top = 0;

  while (true) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int view = rows - 3; // header + legend + status

    std::vector<FlatItem> flattened_items;
    flatten(this->root_node, flattened_items, 0, true, {});

    int sz = (int)flattened_items.size();
    // проверка выхода за пределы массива
    if (selected >= sz) {
      selected = sz - 1;
    }
    if (selected < 0) {
      selected = 0;
    }

    // проверка выхода за пределы экрана (скролл)
    if (selected < top) {
      top = selected;
    }
    if (selected >= top + view) {
      top = selected - view + 1;
    }

    erase();
    this->draw_all(rows, cols, view, top, flattened_items, selected, sz);

    refresh();
    int ch = getch();
    switch (ch) {
    case 'q':
      endwin();
      return;
    case KEY_UP:
      selected--;
      break;
    case KEY_DOWN:
      selected++;
      break;
    case '\n':
      if (selected < sz && !flattened_items[selected].node->leaf) {
        flattened_items[selected].node->expanded =
            !flattened_items[selected].node->expanded;
      }
      break;
    }
  }
}

void Ui::draw_all(int rows, int cols, int view, int top,
                  const std::vector<FlatItem> &flattened_items, int selected,
                  int sz) {
  this->draw_header(cols, this->pid);

  for (int row = 0; row < view; row++) {
    int i = top + row;
    if (i >= sz) {
      mvhline(1 + row, 0, ' ', cols);
      continue;
    }
    this->draw_item(1 + row, cols, flattened_items[i], i == selected);
  }

  this->draw_legend(rows - 2, cols);
  this->draw_status(rows, cols,
                    selected < sz ? &flattened_items[selected] : nullptr, sz,
                    selected);
}
