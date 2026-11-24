# Bir programın çıkış kodunu sınar (CTest, cmake -P).
#
#   cmake -DPROGRAM=<ikili> -DBEKLENEN=<kod> [-DARGUMANLAR=<a;b;c>] [-DILETI=<stderr'de aranan>]
#         -P cikis_kodu.cmake
execute_process(
  COMMAND "${PROGRAM}" ${ARGUMANLAR}
  RESULT_VARIABLE _kod
  OUTPUT_VARIABLE _cikti
  ERROR_VARIABLE _hata)
if(NOT "${_kod}" STREQUAL "${BEKLENEN}")
  message(FATAL_ERROR "çıkış kodu ${_kod}, beklenen ${BEKLENEN}\nstdout:\n${_cikti}\nstderr:\n${_hata}")
endif()
if(DEFINED ILETI)
  string(FIND "${_hata}" "${ILETI}" _yer)
  if(_yer EQUAL -1)
    message(FATAL_ERROR "standart hatada '${ILETI}' yok:\n${_hata}")
  endif()
endif()
message(STATUS "çıkış kodu ${_kod} (beklenen)")
