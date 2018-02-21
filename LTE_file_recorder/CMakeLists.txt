include(GrPlatform)
include_directories(hdr
  ${CMAKE_SOURCE_DIR}/liblte/hdr
  ${CMAKE_SOURCE_DIR}/libtools/hdr
  ${CMAKE_SOURCE_DIR}/cmn_hdr
)
add_executable(LTE_file_recorder
  src/LTE_file_recorder_main.cc
  src/LTE_file_recorder_interface.cc
  src/LTE_file_recorder_flowgraph.cc
)
target_link_libraries(LTE_file_recorder lte fftw3f tools pthread ${UHD_LIBRARIES} ${Boost_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_BLOCKS_LIBRARIES} ${GNURADIO_PMT_LIBRARIES} ${GNURADIO_OSMOSDR_LIBRARIES})
install(TARGETS LTE_file_recorder DESTINATION bin)
