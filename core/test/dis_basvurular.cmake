# A4-K12, K21, K97 denetimi: çekirdek arşivinin kendi içinde tanımlanmayan başvuruları yalnız
# izinli listede mi (simge tablosundan). Saat, sayaç, rastgele kaynak, bellek ayırma ya da
# herhangi bir platform işlevi listede olmadığı için bu başvurulardan biri hata verir.
#
#   cmake -DNM=<nm> -DARSIV=<.a> -DBICIM=macho|elf -P dis_basvurular.cmake
#
# İzinli: bellek doldurma/kopyalama (derleyicinin de ürettiği), float aşkın işlevler (libm),
# derleyici yardımcıları (__aeabi_*), yığın koruması (Apple Clang'ın _chk ve stack_chk'i).
# bzero: Apple Clang -O2 sıfır ilklendirmeyi (memset(…, 0, …)) bzero çağrısına çevirir (yalnız host; firmware arşivinde geçmez).

if(NOT NM OR NOT ARSIV OR NOT BICIM)
  message(FATAL_ERROR "NM, ARSIV ve BICIM verilmeli")
endif()
# Adlar baştaki alt çizgiler atılarak karşılaştırılır (Mach-O öneki ve __aeabi_ / __*_chk).
set(_izinli "^(memset|bzero|memcpy|memmove|memset_chk|memcpy_chk|stack_chk_fail|stack_chk_guard|sqrtf|sinf|cosf|asinf|atan2f|fmodf|aeabi_[a-z0-9_]+)$")

if(BICIM STREQUAL "macho")
  execute_process(COMMAND "${NM}" -m "${ARSIV}" OUTPUT_VARIABLE _cikti RESULT_VARIABLE _rc)
else()
  execute_process(COMMAND "${NM}" "${ARSIV}" OUTPUT_VARIABLE _cikti RESULT_VARIABLE _rc)
endif()
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "nm çalıştırılamadı (${_rc})")
endif()

string(REPLACE "\n" ";" _satirlar "${_cikti}")
set(_tanimli "")
set(_tanimsiz "")
foreach(_s IN LISTS _satirlar)
  if(_s STREQUAL "" OR _s MATCHES ":$")
    continue()
  endif()
  string(REGEX MATCH "[^ ]+$" _ad "${_s}")
  string(REGEX REPLACE "^_+" "" _ad "${_ad}")
  if(BICIM STREQUAL "macho")
    if(_s MATCHES "\\(undefined\\)")
      list(APPEND _tanimsiz "${_ad}")
    else()
      list(APPEND _tanimli "${_ad}")
    endif()
  else()
    if(_s MATCHES "^ +U ")
      list(APPEND _tanimsiz "${_ad}")
    else()
      list(APPEND _tanimli "${_ad}")
    endif()
  endif()
endforeach()

list(REMOVE_DUPLICATES _tanimsiz)
set(_bulgu 0)
set(_dis 0)
foreach(_ad IN LISTS _tanimsiz)
  if(_ad IN_LIST _tanimli)
    continue() # arşivin başka bir nesnesinde tanımlı
  endif()
  math(EXPR _dis "${_dis} + 1")
  if(NOT _ad MATCHES "${_izinli}")
    message("izinsiz dış başvuru: ${_ad}")
    math(EXPR _bulgu "${_bulgu} + 1")
  endif()
endforeach()
if(_bulgu GREATER 0)
  message(FATAL_ERROR "${_bulgu} izinsiz dış başvuru (${ARSIV})")
endif()
message("dış başvurular izinli listede (${_dis} dış başvuru)")
