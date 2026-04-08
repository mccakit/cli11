add_library(subcommand_a_mod)
target_sources(subcommand_a_mod PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/subcommand_a.cpp)
target_link_libraries(subcommand_a_mod PUBLIC cli11)

add_executable(subcommand_main ${CMAKE_CURRENT_SOURCE_DIR}/subcommand_main.cpp)
target_link_libraries(subcommand_main PRIVATE subcommand_a_mod cli11)
