include(GrPlatform)
add_library(tools
  src/libtools_socket_wrap.cc
  src/libtools_scoped_lock.cc
  src/libtools_ipc_msgq.cc
  src/libtools_helpers.cc
)
include_directories(hdr ${CMAKE_SOURCE_DIR}/cmn_hdr ${CMAKE_SOURCE_DIR}/liblte/hdr)
