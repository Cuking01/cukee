add_executable(cukee main.cpp)

target_link_libraries(cukee PRIVATE
    cukee_game
    cukee_ai
    cukee_trainer
    cukee_analyzer
    cukee_simulator
)

