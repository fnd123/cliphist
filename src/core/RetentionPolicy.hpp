#pragma once

namespace cliphist {

class HistoryRepository;

class RetentionPolicy {
 public:
  int Apply(HistoryRepository& repo, int max_items) const;
};

}  // namespace cliphist
