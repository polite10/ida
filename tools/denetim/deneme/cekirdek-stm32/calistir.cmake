# cekirdek-stm32 deneme projesinin derleme sonrası kiplerini koşturur (cmake -P; CTest).
#   -DIDA_KOK=<depo> -DIKILI=<derleme dizini>
#   -DKIP=<temiz|dolayli_ad|dolayli_kok|asan|kaynak_dizini|kaynak_dizini_stm32>
#   -DURETEC=<CMake üreteci> -DMAKE_PROGRAM=<ninja> -DC_DERLEYICI=<derleyici>
# Yapılandırır, ida_core'u derler ve cekirdek.siniri.basliklar testini koşturur. Bütün alt
# adımların çıktısı basılır (CTest düzenli ifadeleri bu çıktıya bakar). Son satır:
#   "cekirdek-stm32 denemesi: başlık denetimi GEÇTİ" ya da "... DÜŞTÜ".
# Yapılandırma ya da derleme başarısızsa betik hata verir (denetim koşmadan düşmüş sayılmaz).
#
# asan ve kaynak_dizini* kipleri IDA_SANITIZER=ON ile derler. kaynak_dizini* kipleri (yalnız
# Clang) derleyicinin kaynak dizininin include/ ve share/ alt dizinlerini, adı sürüm taşıyan
# farklı bir Xcode dizinini taklit eden bir yola kopyalar ve derleyiciye -resource-dir ile
# verir (CI koşucusundaki /Applications/Xcode_<sürüm>.app durumu). Kopyanın gerçekten
# kullanıldığı derlemeden sonra bağımlılık kaydından doğrulanır; kullanılmadıysa betik hata
# verir (deneme boş yere geçmez).
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

set(_ek_ayarlar "")
set(_sahte_kaynak "")
if(KIP MATCHES "^(asan|kaynak_dizini|kaynak_dizini_stm32)$")
  list(APPEND _ek_ayarlar "-DIDA_SANITIZER=ON")
endif()
if(KIP MATCHES "^kaynak_dizini")
  execute_process(COMMAND "${C_DERLEYICI}" -print-resource-dir
    OUTPUT_VARIABLE _gercek RESULT_VARIABLE _rc OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _rc EQUAL 0 OR NOT IS_DIRECTORY "${_gercek}/include")
    message(FATAL_ERROR "cekirdek-stm32 denemesi: derleyicinin kaynak dizini alınamadı (${KIP}): '${_gercek}'")
  endif()
  get_filename_component(_surum "${_gercek}" NAME)
  set(_arac "${IKILI}-arac")
  set(_sahte_kaynak
    "${_arac}/Xcode_99.9.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/${_surum}")
  file(REMOVE_RECURSE "${_arac}")
  file(MAKE_DIRECTORY "${_sahte_kaynak}")
  file(COPY "${_gercek}/include" DESTINATION "${_sahte_kaynak}")
  if(IS_DIRECTORY "${_gercek}/share")
    file(COPY "${_gercek}/share" DESTINATION "${_sahte_kaynak}")
  endif()
  if(KIP STREQUAL "kaynak_dizini_stm32")
    file(COPY "${IDA_KOK}/tools/denetim/deneme/cekirdek-stm32/kaynak/stm32u5xx.h"
      DESTINATION "${_sahte_kaynak}/include")
  endif()
  # Yol bayrak değerinin içinde tırnaklanır: CMAKE_C_FLAGS komut satırı olarak yeniden
  # ayrıştırılır; derleme dizini boşluk içerirse tırnaksız yol bölünürdü (sol inceleme md.1).
  list(APPEND _ek_ayarlar "-DCMAKE_C_FLAGS=-resource-dir=\"${_sahte_kaynak}\"")
endif()

_adim(yapilandirma "${CMAKE_COMMAND}" --fresh
  -S "${IDA_KOK}/tools/denetim/deneme/cekirdek-stm32" -B "${IKILI}"
  -G "${URETEC}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}" "-DCMAKE_C_COMPILER=${C_DERLEYICI}"
  "-DIDA_KOK=${IDA_KOK}" "-DIDA_DENEME_KIPI=${KIP}" ${_ek_ayarlar})
if(NOT _adim_rc EQUAL 0)
  message(FATAL_ERROR "cekirdek-stm32 denemesi: yapılandırma başarısız (${KIP})")
endif()
_adim(derleme "${CMAKE_COMMAND}" --build "${IKILI}" --target ida_core)
if(NOT _adim_rc EQUAL 0)
  message(FATAL_ERROR "cekirdek-stm32 denemesi: derleme başarısız (${KIP})")
endif()
if(_sahte_kaynak)
  execute_process(COMMAND "${MAKE_PROGRAM}" -C "${IKILI}" -t deps
    OUTPUT_VARIABLE _bag RESULT_VARIABLE _rc)
  string(FIND "${_bag}" "${_sahte_kaynak}/include/" _i)
  if(NOT _rc EQUAL 0 OR _i EQUAL -1)
    message(FATAL_ERROR
      "cekirdek-stm32 denemesi: taşınmış kaynak dizini derlemede kullanılmadı (${KIP})\n${_bag}")
  endif()
  if(EXISTS "${_sahte_kaynak}/share/asan_ignorelist.txt")
    string(FIND "${_bag}" "${_sahte_kaynak}/share/asan_ignorelist.txt" _i)
    if(_i EQUAL -1)
      message(FATAL_ERROR
        "cekirdek-stm32 denemesi: taşınmış kaynak dizinindeki sanitizer listesi bağımlılıkta yok (${KIP})")
    endif()
  endif()
  message("cekirdek-stm32 denemesi: taşınmış kaynak dizini kullanıldı: ${_sahte_kaynak}")
endif()
_adim(baslik_denetimi "${CMAKE_CTEST_COMMAND}" --test-dir "${IKILI}"
  -R "^cekirdek\\.siniri\\.basliklar$" --output-on-failure --no-tests=error)
if(_adim_rc EQUAL 0)
  message("cekirdek-stm32 denemesi: başlık denetimi GEÇTİ")
else()
  message("cekirdek-stm32 denemesi: başlık denetimi DÜŞTÜ")
endif()
