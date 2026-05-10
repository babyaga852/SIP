
add_executable(sip_tests
  test_scheduler.cpp
  test_storage.cpp
  test_behavior_dna.cpp
  test_thermal_scheduler.cpp
  test_state_diff.cpp
  test_process_mapper.cpp
  test_file_decay.cpp
  test_os_layer.cpp
  test_event_loop.cpp
)
target_link_libraries(sip_tests PRIVATE
  sip_core sip_os
  sip_behavior_dna sip_file_decay sip_process_mapper
  sip_thermal_scheduler sip_state_diff
  GTest::gtest_main
)
add_test(NAME sip_unit_tests COMMAND sip_tests --gtest_color=yes)
