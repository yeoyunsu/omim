project(style_tests)

include_directories(${OMIM_ROOT}/3party/protobuf/protobuf/src)

set(
  SRC
  classificator_tests.cpp
  dashes_test.cpp
  helpers.hpp
  style_symbols_consistency_test.cpp
)

omim_add_test(${PROJECT_NAME} ${SRC})

omim_link_libraries(
  ${PROJECT_NAME}
  expat
  map
  traffic
  indexer
  osrm
  pugixml
  opening_hours
  editor
  oauthcpp
  platform
  jansson
  geometry
  coding
  base
  protobuf
  icu
  stats_client
  ${LIBZ}
)

link_qt5_core(${PROJECT_NAME})
