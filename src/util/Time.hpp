#pragma once

#include <cstdint>
#include <string>

namespace cliphist {

std::int64_t UnixTimeSeconds();
std::string FormatLocalTime(std::int64_t ts);
std::string FormatLocalDate(std::int64_t ts);
std::string FormatLocalDateTime(std::int64_t ts);
std::string DescribeLocalDay(std::int64_t ts);

}  // namespace cliphist
