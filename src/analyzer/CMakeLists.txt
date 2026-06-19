add_library(cukee_analyzer INTERFACE)

target_include_directories(cukee_analyzer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cukee_analyzer INTERFACE cukee_game cukee_ai cukee_server)

