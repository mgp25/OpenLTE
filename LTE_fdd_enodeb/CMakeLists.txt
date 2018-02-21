include(GrPlatform)
include_directories(hdr
  ${CMAKE_SOURCE_DIR}/liblte/hdr
  ${CMAKE_SOURCE_DIR}/libtools/hdr
  ${CMAKE_SOURCE_DIR}/cmn_hdr
)
add_executable(LTE_fdd_enodeb
  src/LTE_fdd_enb_main.cc
  src/LTE_fdd_enb_interface.cc
  src/LTE_fdd_enb_cnfg_db.cc
  src/LTE_fdd_enb_msgq.cc
  src/LTE_fdd_enb_hss.cc
  src/LTE_fdd_enb_user.cc
  src/LTE_fdd_enb_user_mgr.cc
  src/LTE_fdd_enb_rb.cc
  src/LTE_fdd_enb_timer.cc
  src/LTE_fdd_enb_timer_mgr.cc
  src/LTE_fdd_enb_radio.cc
  src/LTE_fdd_enb_phy.cc
  src/LTE_fdd_enb_mac.cc
  src/LTE_fdd_enb_rlc.cc
  src/LTE_fdd_enb_pdcp.cc
  src/LTE_fdd_enb_rrc.cc
  src/LTE_fdd_enb_mme.cc
  src/LTE_fdd_enb_gw.cc
)
target_link_libraries(LTE_fdd_enodeb lte fftw3f tools pthread rt ${POLARSSL_LIBRARIES} ${UHD_LIBRARIES} ${LIBBLADERF_LIBRARIES} ${Boost_LIBRARIES} ${GNURADIO_RUNTIME_LIBRARIES} ${GNURADIO_PMT_LIBRARIES})
install(TARGETS LTE_fdd_enodeb DESTINATION bin)
install(CODE "execute_process(COMMAND chmod +x \"${CMAKE_SOURCE_DIR}/enodeb_nat_script.sh\")")
install(CODE "execute_process(COMMAND \"${CMAKE_SOURCE_DIR}/enodeb_nat_script.sh\")")
