set(INCLUDE_DIRECTORIES
  ${BASE_INCLUDE_DIRECTORIES}
  ${Boost_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/devices/flp2epn-distributed
)

# DDS
set(INCLUDE_DIRECTORIES
  ${INCLUDE_DIRECTORIES}
  $ENV{DDS_LOCATION}/include
)

include_directories(${INCLUDE_DIRECTORIES})

configure_file(
  ${CMAKE_SOURCE_DIR}/devices/flp2epn-distributed/run/startFLP2EPN-distributed.sh.in ${CMAKE_BINARY_DIR}/bin/startFLP2EPN-distributed.sh
)

set(LINK_DIRECTORIES
  ${Boost_LIBRARY_DIRS}
  ${FAIRROOT_LIBRARY_DIR}
)

# DDS
set(LINK_DIRECTORIES
  ${LINK_DIRECTORIES}
  $ENV{DDS_LOCATION}/lib
)

link_directories(${LINK_DIRECTORIES})

set(SRCS
  FLPex.cxx
  EPNex.cxx
  FLPexSampler.cxx
)

set(DEPENDENCIES
  ${DEPENDENCIES}
  ${CMAKE_THREAD_LIBS_INIT}
  boost_date_time boost_thread boost_timer boost_system boost_program_options FairMQ
)

set(LIBRARY_NAME FLP2EPNex_distributed)

GENERATE_LIBRARY()

Set(Exe_Names
  ${Exe_Names}
  testFLP_distributed
  testEPN_distributed
  testFLPSampler
)

set(Exe_Source
  run/runFLP_distributed.cxx
  run/runEPN_distributed.cxx
  run/runFLPSampler.cxx
)

list(LENGTH Exe_Names _length)
math(EXPR _length ${_length}-1)

ForEach(_file RANGE 0 ${_length})
  list(GET Exe_Names ${_file} _name)
  list(GET Exe_Source ${_file} _src)
  set(EXE_NAME ${_name})
  set(SRCS ${_src})
  set(DEPENDENCIES FLP2EPNex_distributed)
  GENERATE_EXECUTABLE()
EndForEach(_file RANGE 0 ${_length})

# DDS
target_link_libraries (
	testFLP_distributed
	dds-key-value-lib
)

target_link_libraries (
	testEPN_distributed
	dds-key-value-lib
)

target_link_libraries (
	testFLPSampler
	dds-key-value-lib
)
