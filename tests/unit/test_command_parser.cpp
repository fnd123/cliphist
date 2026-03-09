#include <iostream>

#include "cli/CommandParser.hpp"

int main() {
  cliphist::CommandParser parser;

  {
    char arg0[] = "cliphist";
    char arg1[] = "ui";
    char arg2[] = "--limit=30";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kUi || opts.limit != 30) {
      std::cerr << "expected ui command with limit 30\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "daemon";
    char arg2[] = "--selection=both";
    char arg3[] = "--max-items=321";
    char* argv[] = {arg0, arg1, arg2, arg3};

    auto opts = parser.Parse(4, argv);
    if (opts.type != cliphist::CommandType::kDaemon) {
      std::cerr << "expected daemon command\n";
      return 1;
    }
    if (opts.selection_mode != cliphist::SelectionMode::kBoth) {
      std::cerr << "expected selection both\n";
      return 1;
    }
    if (opts.max_items != 321) {
      std::cerr << "expected max-items 321\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "desktop";
    char arg2[] = "--max-items=256";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kDesktop || opts.max_items != 256) {
      std::cerr << "expected desktop command with max-items 256\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "list";
    char arg2[] = "--contains=hello";
    char arg3[] = "--since=1700000000";
    char arg4[] = "--exact=hello world";
    char arg5[] = "--json";
    char arg6[] = "--sort=created_at";
    char arg7[] = "--order=asc";
    char arg8[] = "--count-only";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8};

    auto opts = parser.Parse(9, argv);
    if (opts.type != cliphist::CommandType::kList) {
      std::cerr << "expected list command\\n";
      return 1;
    }
    if (opts.contains != "hello") {
      std::cerr << "expected contains=hello\\n";
      return 1;
    }
    if (opts.since != 1700000000LL) {
      std::cerr << "expected since=1700000000\\n";
      return 1;
    }
    if (opts.exact != "hello world") {
      std::cerr << "expected exact=hello world\\n";
      return 1;
    }
    if (!opts.json) {
      std::cerr << "expected json output mode\\n";
      return 1;
    }
    if (opts.sort_field != cliphist::SortField::kCreatedAt) {
      std::cerr << "expected sort=created_at\\n";
      return 1;
    }
    if (opts.sort_order != cliphist::SortOrder::kAsc) {
      std::cerr << "expected order=asc\\n";
      return 1;
    }
    if (!opts.count_only) {
      std::cerr << "expected count-only\\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "list";
    char arg2[] = "--limit=abc";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kInvalid) {
      std::cerr << "expected invalid command for bad integer\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "list";
    char arg2[] = "--since=not_number";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kInvalid) {
      std::cerr << "expected invalid command for bad since\\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "list";
    char arg2[] = "--sort=oops";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kInvalid) {
      std::cerr << "expected invalid command for bad sort\\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "list";
    char arg2[] = "--order=nope";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kInvalid) {
      std::cerr << "expected invalid command for bad order\\n";
      return 1;
    }
  }

  {
    char arg0[] = "cliphist";
    char arg1[] = "daemon";
    char arg2[] = "--selection=weird";
    char* argv[] = {arg0, arg1, arg2};

    auto opts = parser.Parse(3, argv);
    if (opts.type != cliphist::CommandType::kInvalid) {
      std::cerr << "expected invalid command for bad selection\n";
      return 1;
    }
  }

  return 0;
}
