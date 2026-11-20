# protocol/ida_link — A1 tel katmanı (W2-protokol): çerçeveleme, kodlama, alıcı sayaçları
# ve sözleşmenin makinece denetlenen değerleri (dialect_hash, altın baytlar, ayna eşitlikleri).
#
# İki kökte de (host, fw) aynı kaynakla derlenir. Hedefler:
#   ida_link                 statik kitaplık (ISO C17, uzantısız; ayırma, OS, stdio yok)
#   ida_link_altin_veri      A1 Ek B altın çerçeveleri (yalnız test ve ölçüm)
#   ida_link_ayna_denetimi   A1-K03: 136 IDAW_ = IDA_ eşitliği, derleme zamanında (C17; host'ta C++20 de)
#   ida_link_uret            üretilen başlıklar ve deneme girdileri (özel hedef)
#
# Üretim adımları (Python 3, yalnız standart kitaplık; araclar/ida_link_arac.py):
#   protocol/ida.xml + A1 Tablo 3.3 -> mesaj tablosu ve ida_link/dialect.h
#   protocol/ayna.json (+ ida.xml, A1-K03, port.h) -> ayna eşitlik başlığı
#   veri/altin.json (+ A1 Ek B ile her derlemede karşılaştırma) -> altın test verisi
# Üretilen dosyalar <ikili-dizin>/protocol/ida_link/uretilen/ altındadır; depoya girmez.

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
include(IdaMavlink)
include(IdaDenetim)
# Üretilmiş MAVLink başlıkları sözleşme başlıkları denetimiyle paylaşılır (aynı çıktıya iki
# üretim kuralı yazılamaz); o denetim kapalıysa burada kurulur.
if(NOT TARGET ida_mavlink_uretilen)
  ida_mavlink_uret(ida_mavlink_uretilen)
endif()

set(IDA_LINK_ARAC "${CMAKE_CURRENT_SOURCE_DIR}/araclar/ida_link_arac.py")
set(IDA_LINK_XML "${PROJECT_SOURCE_DIR}/protocol/ida.xml")
set(IDA_LINK_SOZLESME "${PROJECT_SOURCE_DIR}/docs/sozlesmeler/A1-kart-baglantisi.md")
set(IDA_LINK_PORT "${PROJECT_SOURCE_DIR}/core/include/ida/port.h")
set(IDA_LINK_AYNA "${PROJECT_SOURCE_DIR}/protocol/ayna.json")
set(IDA_LINK_ALTIN "${CMAKE_CURRENT_SOURCE_DIR}/veri/altin.json")
set(IDA_LINK_BEKLENEN "${CMAKE_CURRENT_SOURCE_DIR}/veri/dialect_beklenen.json")
set(IDA_LINK_URETILEN "${CMAKE_CURRENT_BINARY_DIR}/uretilen")
set(_u "${IDA_LINK_URETILEN}")
set(_py "${Python3_EXECUTABLE}" -I "${IDA_LINK_ARAC}")

# ---- Üretim ----------------------------------------------------------------------------
add_custom_command(
  OUTPUT "${_u}/ozel/ida_link_tablo_uretilen.h" "${_u}/acik/ida_link/dialect.h"
  COMMAND ${_py} tablo --xml "${IDA_LINK_XML}" --sozlesme "${IDA_LINK_SOZLESME}"
          --cikti-tablo "${_u}/ozel/ida_link_tablo_uretilen.h"
          --cikti-dialect "${_u}/acik/ida_link/dialect.h"
  DEPENDS "${IDA_LINK_ARAC}" "${IDA_LINK_XML}" "${IDA_LINK_SOZLESME}"
  COMMENT "ida_link: mesaj tablosu ve dialect_hash (protocol/ida.xml, A1 Tablo 3.3)"
  VERBATIM)

add_custom_command(
  OUTPUT "${_u}/ozel/ida_link_ayna_esitlik.h"
  COMMAND ${_py} ayna --xml "${IDA_LINK_XML}" --ayna "${IDA_LINK_AYNA}"
          --sozlesme "${IDA_LINK_SOZLESME}" --port "${IDA_LINK_PORT}"
          --cikti "${_u}/ozel/ida_link_ayna_esitlik.h"
  DEPENDS "${IDA_LINK_ARAC}" "${IDA_LINK_XML}" "${IDA_LINK_AYNA}" "${IDA_LINK_SOZLESME}"
          "${IDA_LINK_PORT}"
  COMMENT "ida_link: A1-K03 ayna eşitlikleri (protocol/ayna.json)"
  VERBATIM)

# Altın veri dosyası her derlemede (girdilerden biri değişince) Ek B ile karşılaştırılır;
# farklıysa üretim ve dolayısıyla derleme başarısız olur.
add_custom_command(
  OUTPUT "${_u}/ozel/ida_link_altin_veri.c" "${_u}/ozel/ida_link_altin_veri.h"
  COMMAND ${_py} altin-c --sozlesme "${IDA_LINK_SOZLESME}" --json "${IDA_LINK_ALTIN}"
          --cikti-c "${_u}/ozel/ida_link_altin_veri.c" --cikti-h "${_u}/ozel/ida_link_altin_veri.h"
  DEPENDS "${IDA_LINK_ARAC}" "${IDA_LINK_SOZLESME}" "${IDA_LINK_ALTIN}"
  COMMENT "ida_link: altın çerçeveler (veri/altin.json = A1 Ek B denetimi)"
  VERBATIM)

# Deneme girdileri: denetimlerin bilerek bozulmuş girdiyi yakaladığını gösteren testler için.
add_custom_command(
  OUTPUT "${_u}/deneme/A1-ek-b-bozuk.md"
  COMMAND ${_py} deneme-ek-b --sozlesme "${IDA_LINK_SOZLESME}" --cikti "${_u}/deneme/A1-ek-b-bozuk.md"
  DEPENDS "${IDA_LINK_ARAC}" "${IDA_LINK_SOZLESME}"
  VERBATIM)
add_custom_command(
  OUTPUT "${_u}/deneme/ida-alan-bozuk.xml"
  COMMAND ${_py} deneme-xml --xml "${IDA_LINK_XML}" --cikti "${_u}/deneme/ida-alan-bozuk.xml"
  DEPENDS "${IDA_LINK_ARAC}" "${IDA_LINK_XML}"
  VERBATIM)
add_custom_command(
  OUTPUT "${_u}/deneme/port/ida/port.h"
  COMMAND ${_py} ayna-boz --ayna "${IDA_LINK_AYNA}" --port "${IDA_LINK_PORT}"
          --cikti "${_u}/deneme/port/ida/port.h"
  DEPENDS "${IDA_LINK_ARAC}" "${IDA_LINK_AYNA}" "${IDA_LINK_PORT}"
  VERBATIM)

add_custom_target(ida_link_uret ALL
  DEPENDS "${_u}/ozel/ida_link_tablo_uretilen.h" "${_u}/acik/ida_link/dialect.h"
          "${_u}/ozel/ida_link_ayna_esitlik.h"
          "${_u}/ozel/ida_link_altin_veri.c" "${_u}/ozel/ida_link_altin_veri.h"
          "${_u}/deneme/A1-ek-b-bozuk.md" "${_u}/deneme/ida-alan-bozuk.xml"
          "${_u}/deneme/port/ida/port.h")

# ---- Kitaplık --------------------------------------------------------------------------
add_library(ida_link STATIC src/tel.c)
target_include_directories(ida_link
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${_u}/acik"
  PRIVATE "${_u}/ozel")
target_link_libraries(ida_link PRIVATE ida_mavlink_uretilen)
add_dependencies(ida_link ida_link_uret)
ida_ayar_uygula(ida_link CEKIRDEK)
if(CMAKE_CROSSCOMPILING)
  target_compile_options(ida_link PRIVATE -fstack-usage)
endif()

# Altın çerçeve verisi (test ve ölçüm; ürüne girmez).
add_library(ida_link_altin_veri STATIC "${_u}/ozel/ida_link_altin_veri.c")
target_include_directories(ida_link_altin_veri PUBLIC "${_u}/ozel")
add_dependencies(ida_link_altin_veri ida_link_uret)
ida_ayar_uygula(ida_link_altin_veri CEKIRDEK)

# ---- A1-K03: ayna eşitlikleri derleme zamanında ------------------------------------------
# Üretilmiş MAVLink başlığı + port.h + üretilmiş eşitlik başlığı aynı çeviri biriminde.
# Nesne kitaplığı ALL'dadır: bir eşitlik tutmazsa derlemenin kendisi başarısız olur.
set(_ayna_kaynak denetim/ayna_c17.c)
if(NOT IDA_HEDEF STREQUAL "fw")
  list(APPEND _ayna_kaynak denetim/ayna_cxx20.cpp denetim/tel_cxx20.cpp)
endif()
add_library(ida_link_ayna_denetimi OBJECT ${_ayna_kaynak})
target_include_directories(ida_link_ayna_denetimi PRIVATE
  "${PROJECT_SOURCE_DIR}/core/include" "${_u}/ozel" "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${_u}/acik")
target_link_libraries(ida_link_ayna_denetimi PRIVATE ida_mavlink_uretilen)
add_dependencies(ida_link_ayna_denetimi ida_link_uret)
ida_ayar_uygula(ida_link_ayna_denetimi CEKIRDEK)

# Deneme (kabul md. 4): port.h'nin bir değeri değiştirilmiş kopyasıyla aynı birim derlenmez.
add_library(ida_link_deneme_ayna_bozuk OBJECT EXCLUDE_FROM_ALL ${_ayna_kaynak})
target_include_directories(ida_link_deneme_ayna_bozuk BEFORE PRIVATE "${_u}/deneme/port")
target_include_directories(ida_link_deneme_ayna_bozuk PRIVATE
  "${PROJECT_SOURCE_DIR}/core/include" "${_u}/ozel" "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${_u}/acik")
target_link_libraries(ida_link_deneme_ayna_bozuk PRIVATE ida_mavlink_uretilen)
add_dependencies(ida_link_deneme_ayna_bozuk ida_link_uret)
ida_ayar_uygula(ida_link_deneme_ayna_bozuk CEKIRDEK)

# ---- Sözleşme değerlerinin denetimleri (CTest) ------------------------------------------
add_test(NAME protokol.dialect.beklenen
  COMMAND ${_py} dialect-denetle --xml "${IDA_LINK_XML}" --beklenen "${IDA_LINK_BEKLENEN}"
          --baslik "${_u}/acik/ida_link/dialect.h")
add_test(NAME protokol.dialect.deneme_alan_adi
  COMMAND ${_py} dialect-denetle --xml "${_u}/deneme/ida-alan-bozuk.xml"
          --beklenen "${IDA_LINK_BEKLENEN}")
set_tests_properties(protokol.dialect.deneme_alan_adi PROPERTIES
  PASS_REGULAR_EXPRESSION "\\[DIALECT_FARKI\\]")

add_test(NAME protokol.altin.ek_b_esit
  COMMAND ${_py} altin-denetle --sozlesme "${IDA_LINK_SOZLESME}" --json "${IDA_LINK_ALTIN}")
add_test(NAME protokol.altin.deneme_ek_b_bayt
  COMMAND ${_py} altin-denetle --sozlesme "${_u}/deneme/A1-ek-b-bozuk.md" --json "${IDA_LINK_ALTIN}")
set_tests_properties(protokol.altin.deneme_ek_b_bayt PROPERTIES
  PASS_REGULAR_EXPRESSION "\\[ALTIN_FARKI\\]")

add_test(NAME protokol.ayna.deneme_port_degeri
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ida_link_deneme_ayna_bozuk)
set_tests_properties(protokol.ayna.deneme_port_degeri PROPERTIES
  PASS_REGULAR_EXPRESSION "ayna_esitsizligi")

# Bellek ayırma (tools/denetim/bellek-ayirma.sh; MSVC'de atlanır, IdaDenetim.cmake).
# IdaDenetim.cmake'in betik yolu değişkeni include_guard(GLOBAL) yüzünden yalnız onu ilk
# içeren dizinde tanımlıdır; bu dizinde yoksa aynı değerle kurulur (işlev çağıranın
# kapsamını görür).
if(NOT IDA_BELLEK_DENETIMI)
  set(IDA_BELLEK_DENETIMI "${PROJECT_SOURCE_DIR}/tools/denetim/bellek-ayirma.sh")
endif()
ida_bellek_denetimi_ekle(protokol.bellek.ida_link ida_link)

# Simge tablosu: değişebilir genel durum yok, kanal işlevi ya da dizisi yok, yalnız izinli
# dış başvurular (A1-K101; görev kabul md. 6). Sanitizer'lı derlemede araç simgeleri eklenir,
# MSVC'de nm yoktur: ikisinde atlanır.
if(NOT MSVC AND NOT IDA_SANITIZER AND CMAKE_NM)
  add_test(NAME protokol.simge.ida_link
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DARSIV=$<TARGET_FILE:ida_link>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/simge_denetimi.cmake")
  # Karşı örnek: static sayaç taşıyan nesne denetimce yakalanır.
  add_library(ida_link_deneme_durumlu STATIC denetim/deneme_durumlu.c)
  add_test(NAME protokol.simge.deneme_durumlu
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DARSIV=$<TARGET_FILE:ida_link_deneme_durumlu>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/simge_denetimi.cmake")
  set_tests_properties(protokol.simge.deneme_durumlu PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[SIMGE_DENETIMI\\].*sayac")
endif()

# ---- Birim testleri ve ölçüm ------------------------------------------------------------
if(BUILD_TESTING OR NOT DEFINED BUILD_TESTING)
  add_subdirectory(test)
endif()
add_subdirectory(olcum)
