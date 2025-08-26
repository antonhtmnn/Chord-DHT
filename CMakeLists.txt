cmake_minimum_required(VERSION 3.16)
project(Chord-DHT C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Threads (pthread)
find_package(Threads REQUIRED)

# Math library (for pow) — present on UNIX as libm
if(UNIX AND NOT APPLE)
  find_library(MATH_LIBRARY m)
endif()

set(PROJECT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# -------- Client --------
add_executable(client
  src/client.c
  src/packet.c
  src/util.c
)
target_include_directories(client PRIVATE ${PROJECT_INCLUDE_DIR})
target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)
# (Client usually doesn't need libm; link only if found)
if(MATH_LIBRARY)
  target_link_libraries(client PRIVATE ${MATH_LIBRARY})
endif()

# -------- Peer --------
add_executable(peer
  src/peer.c
  src/server.c
  src/packet.c
  src/util.c
  src/hash_table.c
  src/neighbour.c
  src/requests.c
)
target_include_directories(peer PRIVATE ${PROJECT_INCLUDE_DIR})
target_compile_options(peer PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(peer PRIVATE Threads::Threads)
if(MATH_LIBRARY)
  target_link_libraries(peer PRIVATE ${MATH_LIBRARY})
endif()

# -------- Source packaging (tgz) --------
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  ${CMAKE_BINARY_DIR}
  "/\\..*$"
)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
