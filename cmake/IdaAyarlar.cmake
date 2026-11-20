# İDA ortak derleme ayarları: uyarı ve kayan nokta politikası (09 §2.6, §2.8; W2-depo §2).
#
#   ida_ayar_uygula(<hedef> [CEKIRDEK])
#
# Bütün İDA hedeflerine (çekirdek, testler, firmware, denetim çeviri birimleri) uygulanır;
# üçüncü taraf kodu (Unity, Eigen, üretilen MAVLink başlıkları) bu ayarları almaz.
#
# GCC/Clang: -Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Werror
#            -ffp-contract=off -fno-fast-math
# MSVC:      /W4 /WX /fp:precise /utf-8; C++'ta /permissive-
#            (C'de /std:c17 uygunluk kipini zaten açar; /permissive- bir C++ seçeneğidir.)
#            /utf-8: kaynak ve başlıklardaki Türkçe metin UTF-8'dir; kod sayfasına bağlı
#            C4819 uyarısını önler.
# CEKIRDEK:  ISO C17, uzantı yok, -pedantic-errors (MSVC'de /std:c17 ile).
#
# Seçenekler (önbellek):
#   IDA_SANITIZER     ASan + UBSan (Clang/GCC), ASan (MSVC); host-asan ön ayarı açar.
#   IDA_GCC_ANALYZER  GCC -fanalyzer (yalnız firmware derlemesinde anlamlı); fw-debug açar.
#   IDA_MSVC_ANALYZE  MSVC /analyze (yalnız CEKIRDEK ayarlı hedeflerde).

include_guard(GLOBAL)

option(IDA_SANITIZER "ASan (+ UBSan GCC/Clang'da) ile derle" OFF)
option(IDA_GCC_ANALYZER "GCC -fanalyzer statik analizini aç" OFF)
option(IDA_MSVC_ANALYZE "MSVC /analyze statik analizini aç" OFF)

function(ida_ayar_uygula hedef)
  cmake_parse_arguments(PARSE_ARGV 1 A "CEKIRDEK" "" "")

  get_target_property(_tur ${hedef} TYPE)
  if(_tur STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR "ida_ayar_uygula: arayüz hedefi desteklenmez: ${hedef}")
  endif()

  set_target_properties(${hedef} PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
  if(A_CEKIRDEK)
    set_target_properties(${hedef} PROPERTIES C_EXTENSIONS OFF)
  endif()

  if(MSVC)
    target_compile_options(${hedef} PRIVATE
      /W4 /WX /fp:precise /utf-8
      $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
    if(IDA_MSVC_ANALYZE AND A_CEKIRDEK)
      # Yalnız çekirdek kurallarıyla derlenen hedefler (çekirdek, testi, sözleşme başlıkları).
      target_compile_options(${hedef} PRIVATE /analyze /analyze:external-)
    endif()
  else()
    target_compile_options(${hedef} PRIVATE
      -Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Werror
      -ffp-contract=off -fno-fast-math)
    if(A_CEKIRDEK)
      target_compile_options(${hedef} PRIVATE -pedantic-errors)
    endif()
    if(IDA_GCC_ANALYZER AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${hedef} PRIVATE $<$<COMPILE_LANGUAGE:C>:-fanalyzer>)
    endif()
  endif()

  if(IDA_SANITIZER AND NOT CMAKE_CROSSCOMPILING)
    if(MSVC)
      target_compile_options(${hedef} PRIVATE /fsanitize=address)
      target_link_options(${hedef} PRIVATE /INCREMENTAL:NO)
    else()
      target_compile_options(${hedef} PRIVATE
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
      target_link_options(${hedef} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

# MSVC'de /fsanitize=address, CMake'in Debug için verdiği /RTC1 ile birlikte kullanılamaz.
function(ida_msvc_asan_hazirla)
  if(MSVC AND IDA_SANITIZER)
    foreach(_dil C CXX)
      string(REGEX REPLACE "/RTC[1csu]+" "" _b "${CMAKE_${_dil}_FLAGS_DEBUG}")
      set(CMAKE_${_dil}_FLAGS_DEBUG "${_b}" CACHE STRING "" FORCE)
    endforeach()
  endif()
endfunction()
