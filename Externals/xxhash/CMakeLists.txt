project(xxhash C)

add_library(xxhash STATIC xxHash/xxhash.c)
dolphin_disable_warnings(xxhash)
target_include_directories(xxhash
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/xxHash
)
add_library(xxhash::xxhash ALIAS xxhash)
