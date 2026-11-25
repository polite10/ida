# sim/kayit — A6 kayıt kitaplığı (W5-kayit dilim 1): MCAP yazıcı, okuyucu, kurtarma okuması.
#
#   ida_kayit       kitaplık (C++20): yazıcı (üreticileri bekletmeyen yazım yolu), okuyucu (kendi
#                   ayrıştırıcısı; CRC doğrulaması ve kurtarma okuması), katalog tabloları
#   ida_kayit_mcap  `foxglove/mcap` C++ yazıcısının sarmalayıcısı; kitaplığın başlıkları yalnız
#                   burada içerilir ve İDA uyarı politikasıyla derlenmez (üçüncü taraf)
#   ida_kayit_dok   bir kayıt dosyasının özetini yazan program
#   ida_kayit_test  CTest testleri (sim.kayit.*)
#
# Bağımlılıklar (kökteki vcpkg.json, sürüm sabit; W5-K35): mcap 2.1.3 (başlık-yalnız; yalnız zstd
# özelliğiyle, lz4 yok: port `MCAP_COMPRESSION_NO_LZ4` tanımını verir) ve zstd 1.5.7. vcpkg etkin
# değilse dizin atlanır (sim/senaryo ile aynı politika).
#
# Yük kodlayıcıları ve katalog tabloları derleme zamanında docs/sozlesmeler/sema/a6-kanallar.json'dan
# üretilir (arac/uret_kayit.py; elle yazılmış alan sırası yok). A3 ile ortak yükler `ida_a3_yukler`
# hedefindeki A3 yapıları ve kodlayıcılarıdır (W5-K36).

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  if(IDA_BAGIMLILIK_ZORUNLU)
    message(FATAL_ERROR "sim/kayit: vcpkg etkin değil")
  endif()
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): sim/kayit atlandı")
  return()
endif()

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(unofficial-mcap CONFIG REQUIRED)

set(_ida_kayit_a6 "${PROJECT_SOURCE_DIR}/docs/sozlesmeler/sema/a6-kanallar.json")
set(_ida_kayit_a3 "${PROJECT_SOURCE_DIR}/docs/sozlesmeler/sema/a3-kanallar.json")
set(_ida_kayit_a3_uretec "${PROJECT_SOURCE_DIR}/sim/a3/arac/uret_katalog.py")
set(_ida_kayit_uretec "${CMAKE_CURRENT_SOURCE_DIR}/arac/uret_kayit.py")
set(_ida_kayit_uretim "${CMAKE_CURRENT_BINARY_DIR}/uretim")
add_custom_command(
  OUTPUT "${_ida_kayit_uretim}/include/ida/kayit/yukler.hpp" "${_ida_kayit_uretim}/yukler.cpp"
  COMMAND "${Python3_EXECUTABLE}" -I "${_ida_kayit_uretec}"
          "${_ida_kayit_a6}" "${_ida_kayit_a3}" "${_ida_kayit_a3_uretec}" "${_ida_kayit_uretim}"
  DEPENDS "${_ida_kayit_uretec}" "${_ida_kayit_a3_uretec}" "${_ida_kayit_a6}" "${_ida_kayit_a3}"
  COMMENT "A6 kanal kataloğundan kodlayıcılar üretiliyor"
  VERBATIM)

# GCC/Clang'da -Wshadow (MSVC /W4'teki C4456–C4459'un karşılığı).
function(ida_kayit_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

# Üçüncü taraf başlıklarını içeren çeviri birimleri: İDA uyarı politikası (uyarı = hata) uygulanmaz;
# kitaplığın başlıkları sistem başlığı sayılır. `MCAP_PUBLIC` boş tanımlanır: kitaplık statik
# bağlanır (Windows'ta başlığın varsayılanı dllexport/dllimport'tur).
function(ida_kayit_ucuncu_taraf_ayar hedef)
  set_target_properties(${hedef} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  target_compile_definitions(${hedef} PRIVATE "MCAP_PUBLIC=")
  target_link_libraries(${hedef} PRIVATE unofficial::mcap::mcap)
  if(MSVC)
    target_compile_options(${hedef} PRIVATE /W3 /utf-8 /permissive-)
  else()
    target_compile_options(${hedef} PRIVATE -Wall -Wextra)
  endif()
  if(IDA_SANITIZER AND NOT CMAKE_CROSSCOMPILING)
    if(MSVC)
      target_compile_options(${hedef} PRIVATE /fsanitize=address)
    else()
      target_compile_options(${hedef} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    endif()
  endif()
endfunction()

add_library(ida_kayit_mcap STATIC src/mcap_yazim.cpp)
target_include_directories(ida_kayit_mcap PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
ida_kayit_ucuncu_taraf_ayar(ida_kayit_mcap)

add_library(ida_kayit STATIC
  src/coz.cpp
  src/crc32.cpp
  src/dosya.cpp
  src/katalog.cpp
  src/okuyucu.cpp
  src/ust_veri.cpp
  src/yazici.cpp
  "${_ida_kayit_uretim}/yukler.cpp")
target_include_directories(ida_kayit
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${_ida_kayit_uretim}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_kayit PUBLIC ida_a3_yukler Threads::Threads PRIVATE ida_kayit_mcap zstd::libzstd)
ida_kayit_ayar(ida_kayit)

add_executable(ida_kayit_dok dok/main.cpp)
target_link_libraries(ida_kayit_dok PRIVATE ida_kayit)
ida_kayit_ayar(ida_kayit_dok)

add_subdirectory(test)
