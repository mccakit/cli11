file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(cli11 STATIC)
target_sources(cli11 PUBLIC FILE_SET CXX_MODULES FILES ${srcs})
target_include_directories(cli11 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
