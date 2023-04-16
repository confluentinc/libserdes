cmake_minimum_required(VERSION 3.23.0)
project(libserdes)

option(SERDES_ENABLE_AVRO_C "Include avro C facilities" OFF)
option(SERDES_ENABLE_AVRO_CPP "Include avro C++ facilities" ON)
option(SERDES_ENABLE_WITH_EXAMPLE "Build examples" OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(SERDES_ENABLE_AVRO_CPP)
  find_package(libavrocpp REQUIRED)
endif()
find_package(jansson REQUIRED)
find_package(CURL REQUIRED)

# Serdes common base library
# --------------------------
set(SERDES_COMMON_PUBLIC_HEADERS
  src/serdes.h
  src/serdes-common.h
)

set(SERDES_COMMON_SRC
  src/framing.c
  src/rest.c
  src/rest.h
  src/schema-cache.c
  src/serdes.c
  src/serdes_int.h
  src/tinycthread.h
)

# Serdes C thread library
# -----------------------
add_library(serdes_c_thread STATIC
  src/tinycthread.c
  src/tinycthread.h
)

target_compile_features(serdes_c_thread PUBLIC c_std_99)
# C_STANDARD required for macOS
set_target_properties(serdes_c_thread PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  LINKER_LANGUAGE C
  FOLDER "Serdes"
)

# Serdes C library
# ----------------
add_library(serdes STATIC)

target_compile_features(serdes PUBLIC c_std_99)
# C_STANDARD required for macOS
set_target_properties(serdes PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  LINKER_LANGUAGE C
  FOLDER "Serdes"
)

target_compile_definitions(serdes PRIVATE HAVE_NO_CONFIG)

target_include_directories(serdes
PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>
PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows/include>
)

IF (WIN32)
  target_include_directories(serdes
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows/include>
  )
ENDIF()

target_sources(serdes
PUBLIC
FILE_SET serdes_api
TYPE HEADERS
BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
FILES
  ${SERDES_COMMON_PUBLIC_HEADERS}
  src/serdes.h
PRIVATE
  ${SERDES_COMMON_SRC}
)

target_link_libraries(serdes
PRIVATE
  CURL::libcurl
  jansson::jansson
)

if(SERDES_ENABLE_AVRO_C)
  target_compile_definitions(serdes PUBLIC ENABLE_AVRO_C)

  target_sources(serdes
  PUBLIC
  FILE_SET serdes_api
  TYPE HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
  FILES
    src/serdes-avro.h
  PRIVATE
    src/deserialize-avro.c
    src/schema-avro.c
    src/serialize-avro.c
  )

  target_link_libraries(serdes
  PUBLIC
    avro-static
  )
endif()

# Serdes C++ thread library
# -----------------------
add_library(serdes_cxx_thread STATIC
  src/tinycthread.c
  src/tinycthread.h
)

target_compile_features(serdes_cxx_thread PUBLIC cxx_std_11)
# CXX_STANDARD required for macOS
set_target_properties(serdes_cxx_thread PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  FOLDER "Serdes"
)

# Serdes C++ library
# ------------------
add_library(serdes++ STATIC)

target_compile_features(serdes++ PUBLIC cxx_std_11)
# CXX_STANDARD required for macOS
set_target_properties(serdes++ PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  FOLDER "Serdes"
)

target_compile_definitions(serdes++ PRIVATE HAVE_NO_CONFIG)

target_include_directories(serdes++
PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src-cpp>
  $<INSTALL_INTERFACE:include>
PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

IF (WIN32)
  target_include_directories(serdes++
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows/include>
  )
ENDIF()

target_sources(serdes++
PUBLIC
FILE_SET serdes_api
TYPE HEADERS
BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
FILES
  ${SERDES_COMMON_PUBLIC_HEADERS}
PUBLIC
FILE_SET serdes_api
TYPE HEADERS
BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp
FILES
  src-cpp/serdescpp.h
PRIVATE
  ${SERDES_COMMON_SRC}
  src-cpp/Serdes.cpp
  src-cpp/serdescpp_int.h
)

target_link_libraries(serdes++
PRIVATE
  CURL::libcurl
  jansson::jansson
)

if(SERDES_ENABLE_AVRO_CPP)
  target_sources(serdes++
  PUBLIC
  FILE_SET serdes_api
  TYPE HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp
  FILES
    src-cpp/serdescpp-avro.h
  PRIVATE
    src-cpp/serdescpp-avro_int.h
    src-cpp/serdes-avro.cpp
  )

  target_link_libraries(serdes++
  PUBLIC
    libavrocpp::libavrocpp
  )
endif()

# TODO install directory: include/libserdes
include(GNUInstallDirs)
install(TARGETS
  serdes serdes_c_thread
  serdes++ serdes_cxx_thread
  FILE_SET serdes_api
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libserdes
)

if(SERDES_ENABLE_WITH_EXAMPLE)
  add_subdirectory(examples)
endif()
