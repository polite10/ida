# cmake-moduller deneme projesini yapılandırır, derler ve iç testlerini koşturur (cmake -P).
#   -DIDA_KOK=<depo> -DIKILI=<derleme dizini> -DURETEC=<CMake üreteci>
#   -DMAKE_PROGRAM=<ninja> -DC_DERLEYICI=<derleyici> [-DPYMAVLINK_KAYNAK=<dizin>]
#   [-DTEST_SAYISI=<n>]  verilirse iç projede tam n test kurulmuş olmalı (MSVC dışında 2)
# PYMAVLINK_KAYNAK verilirse CPM indirme yapmaz (CPM_pymavlink_SOURCE).
foreach(_d IDA_KOK IKILI URETEC MAKE_PROGRAM C_DERLEYICI)
  if(NOT DEFINED ${_d})
    message(FATAL_ERROR "calistir.cmake: ${_d} verilmedi")
  endif()
endforeach()

set(_ek "")
if(PYMAVLINK_KAYNAK)
  list(APPEND _ek "-DCPM_pymavlink_SOURCE=${PYMAVLINK_KAYNAK}")
endif()

function(_adim ad)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cmake-moduller denemesi: ${ad} başarısız (${_rc})")
  endif()
endfunction()

_adim(yapilandirma "${CMAKE_COMMAND}" --fresh
  -S "${IDA_KOK}/tools/denetim/deneme/cmake-moduller" -B "${IKILI}"
  -G "${URETEC}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}" "-DCMAKE_C_COMPILER=${C_DERLEYICI}"
  "-DIDA_KOK=${IDA_KOK}" -DIDA_DENEME_KIPI=iki_dizin ${_ek})
_adim(derleme "${CMAKE_COMMAND}" --build "${IKILI}")
if(TEST_SAYISI)
  # İki dizindeki bellek denetimi testi de kurulmuş olmalı (sessizce atlanmış olmamalı).
  execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${IKILI}" -N
    OUTPUT_VARIABLE _liste RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0 OR NOT _liste MATCHES "Total Tests: ${TEST_SAYISI}\n")
    message(FATAL_ERROR "cmake-moduller denemesi: ${TEST_SAYISI} test bekleniyordu:\n${_liste}")
  endif()
  _adim(testler "${CMAKE_CTEST_COMMAND}" --test-dir "${IKILI}" --output-on-failure --no-tests=error)
else()
  # MSVC: bellek denetimi testleri bilerek kurulmaz (IdaDenetim.cmake).
  _adim(testler "${CMAKE_CTEST_COMMAND}" --test-dir "${IKILI}" --output-on-failure --no-tests=ignore)
endif()
message(STATUS "cmake-moduller denemesi: iki dizin geçti")
