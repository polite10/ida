# -fstack-usage çıktılarını (.su) tek rapora birleştirir (cmake -P).
#   -DKOK=<derleme dizini> -DCIKTI=<rapor.txt>
# Biçim: <kaynak:satır:sütun:işlev> <bayt> <nitelik>, bayta göre azalan sırada.
# Yığın tavanı (sınır) W3 kart yazılımı işinde konur; bu rapor yalnız ölçümdür.
file(GLOB_RECURSE _su_dosyalari "${KOK}/*.su")
set(_satirlar "")
foreach(_f IN LISTS _su_dosyalari)
  file(STRINGS "${_f}" _icerik)
  foreach(_s IN LISTS _icerik)
    if(_s MATCHES "^(.*)\t([0-9]+)\t(.*)$")
      # Sıralama için baytı sola, sıfırla doldurulmuş yaz.
      string(LENGTH "${CMAKE_MATCH_2}" _u)
      math(EXPR _dolgu "10 - ${_u}")
      string(REPEAT "0" ${_dolgu} _sifir)
      file(RELATIVE_PATH _goreli "${KOK}" "${_f}")
      list(APPEND _satirlar "${_sifir}${CMAKE_MATCH_2}\t${CMAKE_MATCH_3}\t${CMAKE_MATCH_1}\t(${_goreli})")
    endif()
  endforeach()
endforeach()
list(SORT _satirlar ORDER DESCENDING)
set(_metin "# Yığın kullanımı (-fstack-usage), bayt; sınır W3'te konur\n")
foreach(_s IN LISTS _satirlar)
  string(REGEX REPLACE "^0+([0-9])" "\\1" _s "${_s}")
  string(APPEND _metin "${_s}\n")
endforeach()
file(WRITE "${CIKTI}" "${_metin}")
