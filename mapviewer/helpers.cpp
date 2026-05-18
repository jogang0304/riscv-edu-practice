#include "helpers.h"

std::string trim(const std::string &str) {
  const std::string whitespace = " \t\n\r\f\v";
  size_t start = str.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return ""; // String is all whitespace

  size_t end = str.find_last_not_of(whitespace);
  return str.substr(start, end - start + 1);
}
