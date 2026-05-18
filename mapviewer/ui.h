#include "parser.h"
#include <memory>

enum class NodeKind { ROOT, PGD, PUD, PMD, PTE };

struct TreeNode {
  std::string label;
  NodeKind kind;
  int depth;
  bool expanded = false;
  bool leaf = false;
  PageInfo page{};
  std::vector<std::shared_ptr<TreeNode>> children;
};

struct FlatItem {
  std::shared_ptr<TreeNode> node;
  int depth;
  bool is_last_child;
  std::vector<bool> has_sibling_below; // per depth level
};

enum {
  CP_NORMAL = 1,
  CP_SELECTED,
  CP_ROOT,
  CP_PGD,
  CP_PUD,
  CP_PMD,
  CP_PTE,
  CP_HEADER,
  CP_STATUS,
  CP_LEGEND
};

class Ui {
  pid_t pid;
  std::shared_ptr<TreeNode> root_node;

  void draw_all(int rows, int cols, int view, int top,
                const std::vector<FlatItem> &flattened_items, int selected,
                int sz);
  void draw_legend(int row, int cols);
  void draw_item(int row, int cols, const FlatItem &it, bool is_selected);
  void draw_status(int rows, int cols, const FlatItem *it, int total, int cur);
  void draw_header(int cols, pid_t pid);

public:
  Ui(pid_t pid, const std::map<uint64_t, PGDNode> &pgd_table,
             size_t total_pages);
  void run();
};
