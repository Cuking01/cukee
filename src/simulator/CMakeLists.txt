add_library(cukee_simulator INTERFACE)

target_include_directories(cukee_simulator INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cukee_simulator INTERFACE cukee_game cukee_ai cukee_server)

