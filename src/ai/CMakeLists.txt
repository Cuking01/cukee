add_library(cukee_ai INTERFACE)

target_include_directories(cukee_ai INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cukee_ai INTERFACE cukee_game ${TORCH_LIBRARIES})

