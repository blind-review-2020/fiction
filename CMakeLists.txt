cmake_minimum_required(VERSION 3.2)
project(fiction
        LANGUAGES CXX
        VERSION 0.2.1)

# C++17
set(CMAKE_CXX_STANDARD 17)

# Set debug build options
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wpedantic -Wno-unknown-pragmas")
# Set release build options
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -w -O3")
# Set release mode to default
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()
# Set the possible values of build type for cmake-gui
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# Include header files
include_directories(src/algo/ src/io/ src/tech/ src/topo/ src/util/)

# Find source files
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
# Add configuration file
configure_file(src/util/version.h.in util/version.h)
# Include configuration file
include_directories(${PROJECT_BINARY_DIR}/util/)

# Require Boost libraries
find_package(Boost COMPONENTS system filesystem REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

# Custom install prefix for libraries
set(LIB_PREFIX ${CMAKE_SOURCE_DIR}/libs)

# Clone, build and locally install Z3
include(ExternalProject)
set(Z3_DIR ${CMAKE_BINARY_DIR}/z3)
ExternalProject_Add(z3
        SOURCE_DIR        ${LIB_PREFIX}/z3/
        BUILD_IN_SOURCE   1
        INSTALL_DIR       ${Z3_DIR}/
        CONFIGURE_COMMAND env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
                          ./configure -p <INSTALL_DIR> -b build --staticlib ${Z3_DEBUG}
        BUILD_COMMAND     make -j3 -C build
        INSTALL_COMMAND   make -C build install
        COMMAND           ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_LIST_FILE}
        LOG_CONFIGURE     1
        LOG_INSTALL       1
        LOG_BUILD         1)

# Include Z3
include_directories(${Z3_DIR}/include/)

# Include cppitertools
include_directories(${LIB_PREFIX}/cppitertools/)

# Include alice
add_subdirectory(${LIB_PREFIX}/alice/)

# Include mockturtle
add_subdirectory(${LIB_PREFIX}/mockturtle/)

# Build executable
add_executable(fiction ${SOURCES})
add_dependencies(fiction z3)

# Link against Boost, Z3, alice, and lorina
target_link_libraries(fiction ${Boost_LIBRARIES} ${Z3_DIR}/lib/libz3.so alice mockturtle)
