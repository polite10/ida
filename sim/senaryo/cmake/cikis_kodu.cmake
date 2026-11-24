# ida_senaryo_dogrula'nın çıkış kodunu sınar (CTest, cmake -P).
#
#   cmake -DPROGRAM=<ikili> -DBEKLENEN=<kod> [-DARGUMANLAR=<a;b;c>] [-DCIKTI_DIZINI=<dizin>]
#         [-DKARSILASTIR=<altın dosya>] -P cikis_kodu.cmake
#
# KARSILASTIR verilirse `--cozumlenmis` çıktısı (CIKTI_DIZINI/cozumlenmis.json) o dosyayla
# bayt bayt karşılaştırılır.

if(DEFINED CIKTI_DIZINI)
  file(MAKE_DIRECTORY "${CIKTI_DIZINI}")
endif()
execute_process(
  COMMAND "${PROGRAM}" ${ARGUMANLAR}
  RESULT_VARIABLE _kod
  OUTPUT_VARIABLE _cikti
  ERROR_VARIABLE _hata)
if(NOT "${_kod}" STREQUAL "${BEKLENEN}")
  message(FATAL_ERROR "çıkış kodu ${_kod}, beklenen ${BEKLENEN}\nstdout:\n${_cikti}\nstderr:\n${_hata}")
endif()
if(DEFINED KARSILASTIR)
  file(SHA256 "${CIKTI_DIZINI}/cozumlenmis.json" _a)
  file(SHA256 "${KARSILASTIR}" _b)
  if(NOT _a STREQUAL _b)
    message(FATAL_ERROR "çözümlenmiş çıktı altın dosyadan farklı")
  endif()
endif()
message(STATUS "çıkış kodu ${_kod} (beklenen)")
