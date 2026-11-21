# cekirdek-stm32 deneme projesinin derleme sonrası kiplerini koşturur (cmake -P; CTest).
#   -DIDA_KOK=<depo> -DIKILI=<derleme dizini> -DKIP=<temiz|dolayli_ad|dolayli_kok>
#   -DURETEC=<CMake üreteci> -DMAKE_PROGRAM=<ninja> -DC_DERLEYICI=<derleyici>
# Yapılandırır, ida_core'u derler ve cekirdek.siniri.basliklar testini koşturur. Bütün alt
# adımların çıktısı basılır (CTest düzenli ifadeleri bu çıktıya bakar). Son satır:
#   "cekirdek-stm32 denemesi: başlık denetimi GEÇTİ" ya da "... DÜŞTÜ".
# Yapılandırma ya da derleme başarısızsa betik hata verir (denetim koşmadan düşmüş sayılmaz).
foreach(_d IDA_KOK IKILI KIP URETEC MAKE_PROGRAM C_DERLEYICI)
  if(NOT DEFINED ${_d})
    message(FATAL_ERROR "calistir.cmake: ${_d} verilmedi")
  endif()
endforeach()

function(_adim ad)
  execute_process(COMMAND ${ARGN} OUTPUT_VARIABLE _o ERROR_VARIABLE _e RESULT_VARIABLE _rc)
  message("---- ${ad} (çıkış ${_rc}) ----\n${_o}${_e}")
  set(_adim_rc ${_rc} PARENT_SCOPE)
endfunction()

_adim(yapilandirma "${CMAKE_COMMAND}" --fresh
  -S "${IDA_KOK}/tools/denetim/deneme/cekirdek-stm32" -B "${IKILI}"
  -G "${URETEC}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}" "-DCMAKE_C_COMPILER=${C_DERLEYICI}"
  "-DIDA_KOK=${IDA_KOK}" "-DIDA_DENEME_KIPI=${KIP}")
if(NOT _adim_rc EQUAL 0)
  message(FATAL_ERROR "cekirdek-stm32 denemesi: yapılandırma başarısız (${KIP})")
endif()
_adim(derleme "${CMAKE_COMMAND}" --build "${IKILI}" --target ida_core)
if(NOT _adim_rc EQUAL 0)
  message(FATAL_ERROR "cekirdek-stm32 denemesi: derleme başarısız (${KIP})")
endif()
_adim(baslik_denetimi "${CMAKE_CTEST_COMMAND}" --test-dir "${IKILI}"
  -R "^cekirdek\\.siniri\\.basliklar$" --output-on-failure --no-tests=error)
if(_adim_rc EQUAL 0)
  message("cekirdek-stm32 denemesi: başlık denetimi GEÇTİ")
else()
  message("cekirdek-stm32 denemesi: başlık denetimi DÜŞTÜ")
endif()
