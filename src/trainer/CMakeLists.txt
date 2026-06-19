add_library(cukee_trainer INTERFACE)

target_include_directories(cukee_trainer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cukee_trainer INTERFACE cukee_game cukee_ai cukee_server)

