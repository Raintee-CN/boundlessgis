cmake_minimum_required(VERSION 3.25)
project(boundlessgis LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_program(PG_CONFIG pg_config REQUIRED)
execute_process(COMMAND ${PG_CONFIG} --includedir-server OUTPUT_VARIABLE PG_INCLUDEDIR_SERVER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --includedir OUTPUT_VARIABLE PG_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --pkglibdir OUTPUT_VARIABLE PG_PKGLIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --sharedir OUTPUT_VARIABLE PG_SHAREDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --cflags OUTPUT_VARIABLE PG_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --cppflags OUTPUT_VARIABLE PG_CPPFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)

set(MLT_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../maplibre-tile-spec/cpp" CACHE PATH "maplibre-tile-spec/cpp")

add_subdirectory("${MLT_CPP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/mlt-cpp" EXCLUDE_FROM_ALL)

add_library(boundlessgis MODULE
  src/asmlt.cpp
)
target_include_directories(boundlessgis PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${PG_INCLUDEDIR}
  ${PG_INCLUDEDIR_SERVER}
)
target_compile_definitions(boundlessgis PRIVATE POSTGRES_EXT)
target_compile_options(boundlessgis PRIVATE
  -fPIC
  -fexceptions
  -frtti
  -fno-strict-aliasing
)
target_link_options(boundlessgis PRIVATE -fPIC)
target_link_libraries(boundlessgis PRIVATE mlt-cpp-encoder)
set_target_properties(boundlessgis PROPERTIES
  PREFIX ""
  OUTPUT_NAME "boundlessgis"
)

install(TARGETS boundlessgis LIBRARY DESTINATION ${PG_PKGLIBDIR})
install(FILES
  boundlessgis.control
  sql/boundlessgis--1.0.sql
  DESTINATION ${PG_SHAREDIR}/extension
)
