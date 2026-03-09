#include "core/RetentionPolicy.hpp"

#include "db/HistoryRepository.hpp"

namespace cliphist {

int RetentionPolicy::Apply(HistoryRepository& repo, int max_items) const {
  return repo.EnforceMaxItems(max_items);
}

}  // namespace cliphist
