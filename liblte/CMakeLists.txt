include(GrPlatform)
add_library(lte
  src/liblte_common.cc
  src/liblte_interface.cc
  src/liblte_phy.cc
  src/liblte_mac.cc
  src/liblte_rlc.cc
  src/liblte_pdcp.cc
  src/liblte_rrc.cc
  src/liblte_mme.cc
  src/liblte_security.cc
)
include_directories(hdr ${CMAKE_SOURCE_DIR}/cmn_hdr)

add_executable(liblte_common_test
  tests/liblte_common_tests.cc
  src/liblte_common.cc
)
add_test(liblte_common_test liblte_common_test)
