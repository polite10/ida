# Bağlanmış firmware imajında bellek ayırma simgesi olmadığını denetler (cmake -P; CTest).
#   -DNM=<arm-none-eabi-nm> -DELF=<imaj.elf>
# Nesne düzeyindeki denetim tools/denetim/bellek-ayirma.sh'tadır (tanımsız başvurular);
# bu betik bağlama sonrasını sınar: imajda ayırma işlevi tanımlı ya da tanımsız bulunmaz.
foreach(_d NM ELF)
  if(NOT DEFINED ${_d})
    message(FATAL_ERROR "imaj_ayirma_denetimi.cmake: ${_d} verilmedi")
  endif()
endforeach()
execute_process(COMMAND "${NM}" "${ELF}" OUTPUT_VARIABLE _cikti RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "nm başarısız (${_rc}): ${ELF}")
endif()
set(_yasak malloc calloc realloc free memalign aligned_alloc posix_memalign valloc
           _malloc_r _calloc_r _realloc_r _free_r _memalign_r _sbrk _sbrk_r sbrk)
# Windows'taki nm CRLF yazabilir; satır sonu ayıklanır. Liste ayıracı ';' simge adında
# bulunmaz.
string(REPLACE "\r" "" _cikti "${_cikti}")
string(REPLACE "\n" ";" _satirlar "${_cikti}")
set(_bulgu "")
set(_sayi 0)
foreach(_s IN LISTS _satirlar)
  if(_s MATCHES "^[0-9a-fA-F ]*[ ]([A-Za-z])[ ]([^ ]+)$")
    math(EXPR _sayi "${_sayi} + 1")
    set(_ad "${CMAKE_MATCH_2}")
    if(_ad IN_LIST _yasak OR _ad MATCHES "^__wrap_")
      list(APPEND _bulgu "${_s}")
    endif()
  endif()
endforeach()
if(_sayi EQUAL 0)
  message(FATAL_ERROR "nm çıktısı tanınmadı: ${ELF}")
endif()
if(_bulgu)
  list(JOIN _bulgu "\n  " _metin)
  message(FATAL_ERROR "İmajda bellek ayırma simgesi var (${ELF}):\n  ${_metin}")
endif()
message(STATUS "İmajda ayırma simgesi yok (${_sayi} simge tarandı): ${ELF}")
