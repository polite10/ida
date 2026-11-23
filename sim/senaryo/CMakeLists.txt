# İDA senaryo kitaplığı (W4-senaryo): A5 senaryo ve araç dosyalarının çözümleyicisi ve
# doğrulayıcısı `ida_senaryo`, komut satırı aracı `ida_senaryo_dogrula` ve CTest testleri
# (sim.senaryo.*).
#
# Bağımlılıklar (kökteki vcpkg.json, sürüm sabit): toml++ 3.4.0 (TOML 1.0; başlık-yalnız
# kipte kullanılır, vcpkg'nin derlediği kitaplığa bağlanılmaz), nlohmann-json 3.12.0 (JSON
# okuma), Eigen (atalet ve itici matrisi denetimleri). vcpkg etkin değilse dizin atlanır
# (sim/fizik ile aynı politika).

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  if(IDA_BAGIMLILIK_ZORUNLU)
    message(FATAL_ERROR "sim/senaryo: vcpkg etkin değil")
  endif()
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): sim/senaryo atlandı")
  return()
endif()

find_package(tomlplusplus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

function(ida_senaryo_ayar hedef)
  ida_ayar_uygula(${hedef})
  target_compile_definitions(${hedef} PRIVATE EIGEN_FAST_MATH=0)
  if(NOT MSVC)
    # MSVC /W4'ün gölgeleme (C4456–C4459) uyarılarının karşılığı.
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

# ---- gömülü veri: A5 şeması ve Ek E ------------------------------------------------------
set(_sema "${PROJECT_SOURCE_DIR}/scenarios/sema/a5.schema.json")
set(_ek_e "${CMAKE_CURRENT_SOURCE_DIR}/veri/ek_e.toml")
set(_gom "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gom.cmake")
set(_gomulu_sema "${CMAKE_CURRENT_BINARY_DIR}/gomulu_sema.cpp")
set(_gomulu_ek_e "${CMAKE_CURRENT_BINARY_DIR}/gomulu_ek_e.cpp")
add_custom_command(
  OUTPUT "${_gomulu_sema}"
  COMMAND "${CMAKE_COMMAND}" "-DGIRDI=${_sema}" "-DCIKTI=${_gomulu_sema}" -DAD=a5_sema -P "${_gom}"
  DEPENDS "${_sema}" "${_gom}"
  COMMENT "A5 şeması gömülüyor"
  VERBATIM)
add_custom_command(
  OUTPUT "${_gomulu_ek_e}"
  COMMAND "${CMAKE_COMMAND}" "-DGIRDI=${_ek_e}" "-DCIKTI=${_gomulu_ek_e}" -DAD=ek_e -P "${_gom}"
  DEPENDS "${_ek_e}" "${_gom}"
  COMMENT "A5 Ek E gömülüyor"
  VERBATIM)

add_library(ida_senaryo STATIC
  src/cozumleme.cpp
  src/deger.cpp
  src/dogrulayici.cpp
  src/fizik_yukleyici.cpp
  src/json_okuma.cpp
  src/kanonik.cpp
  src/kurallar.cpp
  src/sayi.cpp
  src/sema.cpp
  src/sha256.cpp
  src/toml_okuma.cpp
  "${_gomulu_sema}"
  "${_gomulu_ek_e}")
target_include_directories(ida_senaryo
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# toml++ başlık-yalnız: vcpkg hedefi derlenmiş kitaplığı ve TOML_HEADER_ONLY=0'ı getirir;
# yalnız başlık dizini alınır (üçüncü taraf: sistem başlığı).
get_target_property(_toml_baslik tomlplusplus::tomlplusplus INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(ida_senaryo SYSTEM PRIVATE ${_toml_baslik})
target_compile_definitions(ida_senaryo PRIVATE TOML_HEADER_ONLY=1 TOML_ENABLE_FORMATTERS=0)
# A5-K52 parametre kataloğu: çekirdeğin A4-K85 genel arayüzü (ida/port.h, ida_core_param_*);
# yalnız okunur, çekirdeğe sim'den bağımlılık eklenmez.
target_link_libraries(ida_senaryo PUBLIC ida_fizik PRIVATE ida_core nlohmann_json::nlohmann_json Eigen3::Eigen)
ida_senaryo_ayar(ida_senaryo)
# Üretilen gömülü kaynaklar İDA uyarı politikasına girer ama yalnız sabit dizilerdir.

add_executable(ida_senaryo_dogrula arac/ida_senaryo_dogrula.cpp)
target_link_libraries(ida_senaryo_dogrula PRIVATE ida_senaryo)
ida_senaryo_ayar(ida_senaryo_dogrula)

# ---- testler -----------------------------------------------------------------------------
add_executable(ida_senaryo_test
  test/test_ana.cpp
  test/test_altin.cpp
  test/test_bozuk.cpp
  test/test_cozumleme.cpp
  test/test_dayaniklilik.cpp
  test/test_ozet.cpp
  test/test_sol.cpp
  test/test_yukleyici.cpp)
target_include_directories(ida_senaryo_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ida_senaryo_test PRIVATE ida_senaryo nlohmann_json::nlohmann_json)
ida_senaryo_ayar(ida_senaryo_test)

set(_ida_senaryo_testleri
  sha256_vektorleri
  sayi_dizilimi            # A5-K96 JCS sütunu, ECMAScript biçimi
  k96_vektorleri           # A5-K96 sınır vektörleri (TOML'dan)
  altin_dosya              # A5-K16, K17: ornek-havuz → altın baytlar ve özet
  ek_e_suruklenme          # sim/senaryo/veri/ek_e.toml = A5 Ek E bloğu (bayt bayt)
  belge_tutarliligi        # Ek E [kosulsuz] = şema default; [profil.*] = A5 §3.6 tablosu
  sema_sozcukleri          # şemadaki her anahtar sözcük yorumlayıcıca tanınıyor
  bozuk_dosyalar           # A5-K01–K11, K96, K97, K102, §5 (tablo güdümlü)
  anlamsal_kurallar        # §4.3–§4.13 yükleme anı kuralları
  kosullu_varsayilanlar    # A5-K98 her satırın iki tarafı
  a7_atiflari              # A5-K18, K19, K36, K115
  ozet_degismezligi        # A5-K17
  yeniden_yukleme          # A5-K02
  rapor_bicimi             # A5-K90–K95
  dayaniklilik             # 10 000 bozulmuş dosya, sabit tohum
  cozumleme_suresi         # ölçüm (eşik yok)
  # dilim 2: fizik yükleyici
  fizik_yukleyici          # çözümlenmiş senaryo → ida::fizik yapıları; Dunya::kur
  ek_a_vektorleri          # A5-K23, K24: Ek A (trimli dahil), 1e-4 bağıl
  fizik_kurallari          # A5-K22, K34
  fizik_karsilastirma      # madde 11: dosyadan kurulan ↔ ida0_arac(), havuz_ortami() (rapor)
  # sol incelemesinin karşı örnekleri (docs/gorevler/inceleme/W4-senaryo-sol.md)
  sol_1_yeniden_yukleme_anlamsal   # EMNİYET
  sol_1_tur_zorlamasi
  sol_2_baslik_yorumu
  sol_3_bozuk_sayi
  sol_4_buyuk_tamsayi
  sol_5_hedefler
  sol_6_acil_durdurma_uyarisi
  sol_7_parametre_katalogu
  sol_8_yol_siniri
  sol_9_routh_olcegi
  sol_10_bulgu_toplama
  sol_11_tek_gecis
  sol_12_arac_dosyasi)
foreach(_ad IN LISTS _ida_senaryo_testleri)
  add_test(NAME sim.senaryo.${_ad} COMMAND ida_senaryo_test ${_ad} "${PROJECT_SOURCE_DIR}")
endforeach()

# ---- sensör yükleyici (dilim 2): yalnız sim/sensor depoda varken (W4-sensor) ------------
if(EXISTS "${PROJECT_SOURCE_DIR}/sim/sensor/CMakeLists.txt")
  add_library(ida_senaryo_sensor STATIC src/sensor_yukleyici.cpp)
  target_link_libraries(ida_senaryo_sensor PUBLIC ida_senaryo ida_sensor)
  ida_senaryo_ayar(ida_senaryo_sensor)
  add_executable(ida_senaryo_sensor_test test/test_sensor_yukleyici.cpp)
  target_link_libraries(ida_senaryo_sensor_test PRIVATE ida_senaryo_sensor)
  ida_senaryo_ayar(ida_senaryo_sensor_test)
  foreach(_ad sensor_yukleyici sensor_karsilastirma)
    add_test(NAME sim.senaryo.${_ad} COMMAND ida_senaryo_sensor_test ${_ad} "${PROJECT_SOURCE_DIR}")
  endforeach()
endif()

# Komut satırı aracının çıkış kodları (0 geçerli, 1 geçersiz, 2 kullanım, 3 G/Ç).
set(_cikis "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cikis_kodu.cmake")
set(_arac_cikti "${CMAKE_CURRENT_BINARY_DIR}/arac_cikti")
add_test(NAME sim.senaryo.arac_gecerli
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:ida_senaryo_dogrula>" -DBEKLENEN=0
          "-DARGUMANLAR=${PROJECT_SOURCE_DIR}/scenarios/ornek-havuz.toml;--cozumlenmis;${_arac_cikti}/cozumlenmis.json;--rapor;${_arac_cikti}/rapor.json"
          "-DCIKTI_DIZINI=${_arac_cikti}"
          "-DKARSILASTIR=${PROJECT_SOURCE_DIR}/scenarios/ornek-havuz.cozumlenmis.json"
          -P "${_cikis}")
add_test(NAME sim.senaryo.arac_arac_dosyasi
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:ida_senaryo_dogrula>" -DBEKLENEN=0
          "-DARGUMANLAR=${PROJECT_SOURCE_DIR}/scenarios/ida0.toml" -P "${_cikis}")
add_test(NAME sim.senaryo.arac_gecersiz
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:ida_senaryo_dogrula>" -DBEKLENEN=1
          "-DARGUMANLAR=${CMAKE_CURRENT_SOURCE_DIR}/test/veri/bozuk.toml" -P "${_cikis}")
add_test(NAME sim.senaryo.arac_kullanim
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:ida_senaryo_dogrula>" -DBEKLENEN=2
          "-DARGUMANLAR=--bilinmeyen" -P "${_cikis}")
add_test(NAME sim.senaryo.arac_gc
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:ida_senaryo_dogrula>" -DBEKLENEN=3
          "-DARGUMANLAR=${PROJECT_SOURCE_DIR}/scenarios/yok.toml" -P "${_cikis}")
