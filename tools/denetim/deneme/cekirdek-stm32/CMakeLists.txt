# Deneme projesi: çekirdek sınırını aşmaya çalışan hedef derlenmez ya da derlense de denetim
# düşer (W2-depo kabul md.4; sol S1).
#
# Gerçek core/'u (ya da derleme dizinindeki bozulmuş bir kopyasını) alır; kipler:
#
#  Yapılandırma katmanı (yapılandırma reddedilmeli):
#   yol         : ida_core'a ST HAL include yolu eklenir
#   kaynak      : ida_core'a aynı dizindeki stm32 başlığını içeren bir kaynak eklenir
#   uretilen    : ida_core'a yapılandırma sırasında var olmayan bir kaynak eklenir
#   genex       : ida_core'a generator expression ile bir kaynak eklenir
#
#  Derleme sonrası katman (yapılandırma ve derleme geçer, cekirdek.siniri.basliklar düşmeli):
#   dolayli_ad  : kaynak değişmeden, kopyadaki iskelet.h'ye aynı dizindeki stm32u5xx.h
#                 içermesi eklenir (yasak kimlik, çekirdek kökünün içinde)
#   dolayli_kok : kaynak değişmeden, kopyadaki iskelet.h'ye çekirdek dışındaki nötr adlı bir
#                 başlığın içermesi eklenir (yasak kök)
#   kaynak_dizini_stm32 : (Clang) kaynak_dizini kipindeki taşınmış derleyici kaynak dizininin
#                 include/ dizinine stm32u5xx.h konur ve kopyadaki iskelet.h onu <...> ile
#                 içerir: sistem kökündeki yasak kimlik yine yakalanmalı
#
#  Karşı örnekler:
#   temiz       : bozma yok; yapılandırma, derleme ve başlık denetimi geçer
#   asan        : temiz + IDA_SANITIZER=ON (GCC/Clang); -fsanitize=address derleyicinin yok
#                 sayma listesini (<kaynak dizini>/share/asan_ignorelist.txt) bağımlılığa yazar
#   kaynak_dizini : (Clang) asan + derleyicinin kaynak dizini sürüm taşıyan, farklı adlı bir
#                 Xcode dizinine taşınmış gibi (-resource-dir; calistir.cmake kurar)
#
# tests/CMakeLists.txt bu projeyi CTest ile bütün kiplerde koşturur (derleme sonrası kipler
# calistir.cmake ile; sanitizer ve -resource-dir bayraklarını o verir).
cmake_minimum_required(VERSION 3.25)
project(ida_cekirdek_siniri_denemesi LANGUAGES C)

if(NOT IDA_KOK OR NOT IDA_DENEME_KIPI)
  message(FATAL_ERROR "IDA_KOK ve IDA_DENEME_KIPI verilmeli")
endif()

list(APPEND CMAKE_MODULE_PATH "${IDA_KOK}/cmake")
include(IdaAyarlar)
include(IdaCekirdekSiniri)
enable_testing()
set(BUILD_TESTING OFF)

set(_kaynak "${CMAKE_CURRENT_SOURCE_DIR}/kaynak")

if(IDA_DENEME_KIPI MATCHES "^(dolayli_|kaynak_dizini_stm32$)")
  # Gerçek çekirdeğe dokunulmaz: derleme dizinine kopyalanır ve kopyanın başlığı bozulur.
  set(_kopya "${CMAKE_CURRENT_BINARY_DIR}/kopya/core")
  file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/kopya")
  file(COPY "${IDA_KOK}/core" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/kopya")
  if(IDA_DENEME_KIPI STREQUAL "dolayli_ad")
    file(COPY "${_kaynak}/stm32u5xx.h" DESTINATION "${_kopya}/src")
    set(_ek "#include \"stm32u5xx.h\"")
  elseif(IDA_DENEME_KIPI STREQUAL "dolayli_kok")
    set(_ek "#include \"${_kaynak}/platform_baslik.h\"")
  elseif(IDA_DENEME_KIPI STREQUAL "kaynak_dizini_stm32")
    set(_ek "#include <stm32u5xx.h>")   # derleyicinin kaynak dizininin include/'undan
  else()
    message(FATAL_ERROR "Bilinmeyen kip: ${IDA_DENEME_KIPI}")
  endif()
  file(READ "${_kopya}/src/iskelet.h" _baslik)
  string(REPLACE "#include <stdint.h>" "#include <stdint.h>\n${_ek}" _yeni "${_baslik}")
  if(_yeni STREQUAL _baslik)
    message(FATAL_ERROR "Deneme kurulamadı: iskelet.h'de '#include <stdint.h>' bulunamadı")
  endif()
  file(WRITE "${_kopya}/src/iskelet.h" "${_yeni}")
  add_subdirectory("${_kopya}" cekirdek)
else()
  add_subdirectory("${IDA_KOK}/core" cekirdek)
  if(IDA_DENEME_KIPI STREQUAL "yol")
    target_include_directories(ida_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/stm32u5xx_hal/Inc")
  elseif(IDA_DENEME_KIPI STREQUAL "kaynak")
    target_sources(ida_core PRIVATE "${_kaynak}/stm32_iceren.c")
  elseif(IDA_DENEME_KIPI STREQUAL "uretilen")
    target_sources(ida_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sonra_uretilecek.c")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/sonra_uretilecek.c"
      TARGET_DIRECTORY ida_core PROPERTIES GENERATED TRUE)
  elseif(IDA_DENEME_KIPI STREQUAL "genex")
    target_sources(ida_core PRIVATE "$<1:${_kaynak}/notr.c>")
  elseif(NOT IDA_DENEME_KIPI MATCHES "^(temiz|asan|kaynak_dizini)$")
    message(FATAL_ERROR "Bilinmeyen kip: ${IDA_DENEME_KIPI}")
  endif()
endif()
