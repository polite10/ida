# Deneme projesi: çekirdeğe stm32 başlığı eklemeye çalışan hedef derlenmez (W2-depo kabul md.4).
#
# Gerçek core/CMakeLists.txt'yi alır, sonra kipe göre çekirdeği bozar:
#   yol    : ida_core'a ST HAL include yolu eklenir           -> yapılandırma reddedilir
#   kaynak : ida_core'a aynı dizindeki stm32 başlığını içeren -> yapılandırma reddedilir
#            bir kaynak eklenir (include yolu gerekmeden)
#   temiz  : bozma yok                                        -> yapılandırma geçer
# tests/CMakeLists.txt bu projeyi CTest ile üç kipte yapılandırır.
cmake_minimum_required(VERSION 3.25)
project(ida_cekirdek_siniri_denemesi LANGUAGES C)

if(NOT IDA_KOK OR NOT IDA_DENEME_KIPI)
  message(FATAL_ERROR "IDA_KOK ve IDA_DENEME_KIPI verilmeli")
endif()

list(APPEND CMAKE_MODULE_PATH "${IDA_KOK}/cmake")
include(IdaAyarlar)
include(IdaCekirdekSiniri)

set(BUILD_TESTING OFF)
add_subdirectory("${IDA_KOK}/core" cekirdek)

if(IDA_DENEME_KIPI STREQUAL "yol")
  target_include_directories(ida_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/stm32u5xx_hal/Inc")
elseif(IDA_DENEME_KIPI STREQUAL "kaynak")
  target_sources(ida_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/kaynak/stm32_iceren.c")
elseif(NOT IDA_DENEME_KIPI STREQUAL "temiz")
  message(FATAL_ERROR "Bilinmeyen kip: ${IDA_DENEME_KIPI}")
endif()
