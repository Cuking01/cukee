add_library(cukee_log log.cpp)

target_include_directories(cukee_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cukee_log PUBLIC cukee_common)
