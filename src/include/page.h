#pragma once

#include <array>
#include <cstddef>

namespace minirel {

inline constexpr std::size_t MINIREL_PAGE_SIZE = 1024;

/**
 * @brief Invalid page number constant.
 */
inline constexpr int INVALID_PAGE_NO = -1;

/**
 * @brief Page struct, used to represent a page in memory. 
 * Pages contain a fixed-size array of bytes.
 */
struct Page {
  std::array<char, MINIREL_PAGE_SIZE> data{};
};

}  // namespace minirel
