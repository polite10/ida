# Bir programın çıkış kodunu sınar (CTest, cmake -P).
#
#   cmake -DPROGRAM=<ikili> -DBEKLENEN=<kod> [-DARGUMANLAR=<a;b;c>] [-DILETI=<stderr'de aranan>]
#         [-DILETILER=<a|b|c: hepsi aranır; ';' içeremez>] [-DYOK=<stderr'de bulunmaması gereken>]
#         [-DTEMIZLE=<dizin: koşudan önce ve sonra silinir>] -P cikis_kodu.cmake
if(DEFINED TEMIZLE)
  file(REMOVE_RECURSE "${TEMIZLE}")
endif()
execute_process(
  COMMAND "${PROGRAM}" ${ARGUMANLAR}
  RESULT_VARIABLE _kod
  OUTPUT_VARIABLE _cikti
  ERROR_VARIABLE _hata)
if(DEFINED TEMIZLE)
  file(REMOVE_RECURSE "${TEMIZLE}")   # çıktı dosyaları (ör. .mcap) derleme dizininde birikmesin
endif()
if(NOT "${_kod}" STREQUAL "${BEKLENEN}")
  message(FATAL_ERROR "çıkış kodu ${_kod}, beklenen ${BEKLENEN}\nstdout:\n${_cikti}\nstderr:\n${_hata}")
endif()
if(DEFINED ILETI)
  string(FIND "${_hata}" "${ILETI}" _yer)
  if(_yer EQUAL -1)
    message(FATAL_ERROR "standart hatada '${ILETI}' yok:\n${_hata}")
  endif()
endif()
# ILETILER: '|' ile ayrılmış, hepsi standart hatada bulunması gereken dizgiler; YOK: bulunmaması gereken.
if(DEFINED ILETILER)
  string(REPLACE "|" ";" _liste "${ILETILER}")
  foreach(_aranan IN LISTS _liste)
    string(FIND "${_hata}" "${_aranan}" _yer)
    if(_yer EQUAL -1)
      message(FATAL_ERROR "standart hatada '${_aranan}' yok:\n${_hata}")
    endif()
  endforeach()
endif()
if(DEFINED YOK)
  string(FIND "${_hata}" "${YOK}" _yer)
  if(NOT _yer EQUAL -1)
    message(FATAL_ERROR "standart hatada '${YOK}' olmamalıydı:\n${_hata}")
  endif()
endif()
message(STATUS "çıkış kodu ${_kod} (beklenen)")
