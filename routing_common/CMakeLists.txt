project(routing_common)

set(
  SRC
  bicycle_model.cpp
  bicycle_model.hpp
  car_model.cpp
  car_model.hpp
  pedestrian_model.cpp
  pedestrian_model.hpp
  vehicle_model.cpp
  vehicle_model.hpp
)

add_library(${PROJECT_NAME} ${SRC})
omim_add_test_subdirectory(routing_common_tests)
