add_library(cukee_simulator game.cpp state.cpp)

target_include_directories(cukee_simulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cukee_simulator PUBLIC cukee_common cukee_log cukee_game cukee_ai cukee_server)
