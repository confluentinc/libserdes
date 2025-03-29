cmake_minimum_required(VERSION 3.23.0)
project(libserdes-examples)

option(SERDES_ENABLE_EXAMPLES_LIBRDKAFKA "Include examples with kafka" OFF)

# Serdes tool
# -----------
add_executable(serdes-tool
  serdes-tool.cpp
)

target_compile_features(serdes-tool PUBLIC cxx_std_11)
set_target_properties(serdes-tool PROPERTIES
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  FOLDER "Examples"
)

target_include_directories(serdes-tool
PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

IF (WIN32)
  target_include_directories(serdes-tool
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows>
  )
ENDIF()

target_link_libraries(serdes-tool
PRIVATE
  serdes_cxx_thread
  serdes++
)

if(SERDES_ENABLE_EXAMPLES_LIBRDKAFKA)
  find_package(RdKafka REQUIRED)
  
  # Kafka console to consume avro with serdes avro
  # ----------------------------------------------
  add_executable(kafka-serdes-avro-console-consumer
    kafka-serdes-avro-console-consumer.cpp
  )

  target_compile_features(kafka-serdes-avro-console-consumer PUBLIC cxx_std_11)
  set_target_properties(kafka-serdes-avro-console-consumer PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "Examples"
  )
  
  target_include_directories(kafka-serdes-avro-console-consumer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  
  IF (WIN32)
    target_include_directories(kafka-serdes-avro-console-consumer
    PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows>
  )
  ENDIF()
  
  target_link_libraries(kafka-serdes-avro-console-consumer
  PRIVATE
    serdes++
    RdKafka::rdkafka++
  )

  # Kafka console to produce avro with serdes avro
  # ----------------------------------------------
  add_executable(kafka-serdes-avro-console-producer
    kafka-serdes-avro-console-producer.cpp
  )

  target_compile_features(kafka-serdes-avro-console-producer PUBLIC cxx_std_11)
  set_target_properties(kafka-serdes-avro-console-producer PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "Examples"
  )
  
  target_include_directories(kafka-serdes-avro-console-producer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  
  IF (WIN32)
    target_include_directories(kafka-serdes-avro-console-producer
    PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows>
  )
  ENDIF()
  
  target_link_libraries(kafka-serdes-avro-console-producer
  PRIVATE
    serdes++
    RdKafka::rdkafka++
  )

  # TODO rdkafka c and serdes
  # serdes-kafka-avro-client.c

endif()
