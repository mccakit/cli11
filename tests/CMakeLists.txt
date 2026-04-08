find_package(Catch2 3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
add_library(test_helper)
target_sources(
    test_helper
    PUBLIC
        FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/test_helper.cpp
)
target_link_libraries(test_helper PUBLIC cli11)
# Test data
file(GLOB_RECURSE DATA_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "data/*")
foreach(f IN LISTS DATA_FILES)
    configure_file("${f}" "${CMAKE_CURRENT_BINARY_DIR}/${f}" COPYONLY)
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
# Dependent test apps
set(CLI11_DEPENDENT_APPS ensure_utf8 ensure_utf8_twice)
foreach(APP IN LISTS CLI11_DEPENDENT_APPS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/applications/${APP}.cpp")
        add_executable(${APP} applications/${APP}.cpp)
        target_link_libraries(${APP} PRIVATE cli11)
    endif()
endforeach()
# All module tests
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*Test.cpp")
foreach(SRC IN LISTS TEST_SOURCES)
    get_filename_component(T "${SRC}" NAME_WE)
    add_executable(${T} ${SRC})
    target_link_libraries(
        ${T}
        PRIVATE cli11 test_helper Catch2::Catch2WithMain Threads::Threads
    )
    target_include_directories(${T} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${T} COMMAND ${T})
    # Pass dependent app paths
    foreach(APP IN LISTS CLI11_DEPENDENT_APPS)
        if(TARGET ${APP})
            string(TOUPPER ${APP} APP_UPPER)
            target_compile_definitions(
                ${T}
                PRIVATE CLI11_${APP_UPPER}_EXE="$<TARGET_FILE:${APP}>"
            )
        endif()
    endforeach()
endforeach()
