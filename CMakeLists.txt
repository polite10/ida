# İDA — kök derleme dosyası (W2-depo).
#
# Tek depo, iki kök (09 §2.6): aynı ağaç iki biçimde yapılandırılır.
#   IDA_HEDEF=host : PC kökü (macOS arm64 Apple Clang, Windows x64 MSVC); ön ayarlar host-*
#   IDA_HEDEF=fw   : firmware kökü (arm-none-eabi, Cortex-M33); ön ayarlar fw-*
# İki kökte de `core/CMakeLists.txt` aynıdır.
#
# Ön ayarlar CMakePresets.json'dadır; elle yapılandırmada IDA_HEDEF verilmelidir.
cmake_minimum_required(VERSION 3.25)

set(IDA_HEDEF "" CACHE STRING "host ya da fw (ön ayarlar verir)")
set_property(CACHE IDA_HEDEF PROPERTY STRINGS host fw)
if(NOT IDA_HEDEF MATCHES "^(host|fw)$")
  message(FATAL_ERROR "IDA_HEDEF host ya da fw olmalı (ör. cmake --preset host-debug)")
endif()

option(IDA_BAGIMLILIK_ZORUNLU
  "vcpkg bağımlılıkları zorunlu (CI açar); kapalıyken VCPKG_ROOT yoksa duman hedefi atlanır" OFF)

if(IDA_HEDEF STREQUAL "host")
  # vcpkg manifest kipi (kökteki vcpkg.json). VCPKG_ROOT ortam değişkeni vcpkg'nin köküdür.
  if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" _ida_vcpkg)
    set(CMAKE_TOOLCHAIN_FILE "${_ida_vcpkg}" CACHE FILEPATH "vcpkg araç zinciri")
  endif()
  if(IDA_BAGIMLILIK_ZORUNLU AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
    message(FATAL_ERROR "IDA_BAGIMLILIK_ZORUNLU açık ama vcpkg etkin değil: VCPKG_ROOT verin")
  endif()
  project(ida VERSION 0.0.1 LANGUAGES C CXX)
else()
  if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "arm-none-eabi-cm33\\.cmake$")
    message(FATAL_ERROR "fw kökü cmake/toolchains/arm-none-eabi-cm33.cmake ile yapılandırılır")
  endif()
  project(ida VERSION 0.0.1 LANGUAGES C ASM)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(IdaAyarlar)
include(IdaCekirdekSiniri)
include(IdaBagimliliklar)
ida_msvc_asan_hazirla()
if(IDA_HEDEF STREQUAL "fw")
  include(IdaFirmware)
endif()

enable_testing()

add_subdirectory(core)

if(IDA_HEDEF STREQUAL "fw")
  add_subdirectory(firmware)
else()
  add_subdirectory(tests)
endif()

# Sözleşme başlıkları denetimi iki kökte de derlenir (fw: arm-none-eabi-gcc ile C17).
add_subdirectory(tests/sozlesme-basliklari)
add_subdirectory(protocol/ida_link)  # A1 tel katmanı (W2-protokol); üretilmiş MAVLink'i paylaşır
