# sim/kayit testleri (sim.kayit.*). Beyaz kutu: iç başlıkları (src/) görür. Dosyalar işletim
# sisteminin geçici dizininde, süreç kimliğiyle adlandırılmış alt dizinlerde açılır; çalışma dizinine
# `kayitlar/` bırakılmaz.

find_package(nlohmann_json CONFIG REQUIRED)

# Örnek yük doldurucuları ve A6 kataloğundan üretilmiş ortak yük kodlayıcıları (W5-K36 karşılaştırması).
set(_ida_kayit_sinama "${CMAKE_CURRENT_BINARY_DIR}/uretim")
add_custom_command(
  OUTPUT "${_ida_kayit_sinama}/sinama_uretim.hpp" "${_ida_kayit_sinama}/sinama_uretim.cpp"
  COMMAND "${Python3_EXECUTABLE}" -I "${_ida_kayit_uretec}" --sinama
          "${_ida_kayit_a6}" "${_ida_kayit_a3}" "${_ida_kayit_a3_uretec}" "${_ida_kayit_sinama}"
  DEPENDS "${_ida_kayit_uretec}" "${_ida_kayit_a3_uretec}" "${_ida_kayit_a6}" "${_ida_kayit_a3}"
  COMMENT "A6 kanal kataloğundan sınama örnekleri üretiliyor"
  VERBATIM)

# `foxglove/mcap` kitaplığının KENDİ okuyucusu (yalnız testler; kabul 1). Üçüncü taraf başlıkları
# İDA uyarı politikasıyla derlenmez.
add_library(ida_kayit_test_mcap STATIC mcap_kitaplik_okuma.cpp)
ida_kayit_ucuncu_taraf_ayar(ida_kayit_test_mcap)
target_link_libraries(ida_kayit_test_mcap PRIVATE zstd::libzstd)

add_executable(ida_kayit_test
  ayirma_sayaci.cpp
  test_ana.cpp
  test_bicim.cpp
  test_birim.cpp
  test_destek.cpp
  test_katalog.cpp
  test_okuma.cpp
  test_yazim.cpp
  "${_ida_kayit_sinama}/sinama_uretim.cpp")
target_include_directories(ida_kayit_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src"
  "${_ida_kayit_sinama}"
  # Zamanlama ölçütünün ortak yardımcısı (W4-K24, W4-K29): yalnız başlık; bağlama yok.
  "${PROJECT_SOURCE_DIR}/sim/kopru/test")
target_link_libraries(ida_kayit_test PRIVATE ida_kayit ida_kayit_test_mcap nlohmann_json::nlohmann_json Threads::Threads)
target_compile_definitions(ida_kayit_test PRIVATE
  "IDA_A6_KATALOG_YOLU=\"${_ida_kayit_a6}\""
  "IDA_A6_BELGE_YOLU=\"${PROJECT_SOURCE_DIR}/docs/sozlesmeler/A6-kayit.md\"")
ida_kayit_ayar(ida_kayit_test)

# Çökme testinin yazan süreci: dosyayı açar, parçaları yazar ve öldürülmeyi bekler.
add_executable(ida_kayit_test_yazan yazan_surec.cpp test_destek.cpp)
target_include_directories(ida_kayit_test_yazan PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(ida_kayit_test_yazan PRIVATE ida_kayit Threads::Threads)
ida_kayit_ayar(ida_kayit_test_yazan)

set(_ida_kayit_testleri
  # birim
  crc32 halka dusme_yuvasi guvence ust_veri_denetimi coz_surum_kurali
  # katalog ve kodlayıcı (A6-K07, K08; W5-K36)
  katalog_tablosu belge_tablosu kodlayici_ortak coz_gidis_donus
  # kabul 1: biçim
  bicim
  # kabul 2: gidiş-dönüş, zaman alanları, sıra
  gidis_donus zaman_alanlari sira_karisik
  # kabul 3: kademe
  kademe kademe_suzgeci
  # kabul 4: üretici beklemez
  uretici_beklemez uretici_ayirmaz eksik_sayim es_zamanli_ureticiler
  # kabul 5: çökme ve kurtarma
  kurtarma_kesik parca_suresi
  # kabul 6: bozulma
  bozulma_parca bozulma_ek_ust_veri ust_veri_birlestirme rastgele_bozulma
  # kabul 7: sınırlar
  dosya_ezilmez negatif_zaman sira_siniri kademe_degismez
  # kapanış, hata kapanışı, sürüm
  kapanis hata_kapanisi guncelleme surum_kurallari parca_hedefi
  # ölçüm (eşik yok; rapora girer)
  olcum_telemetri)
foreach(_ad IN LISTS _ida_kayit_testleri)
  add_test(NAME sim.kayit.${_ad} COMMAND ida_kayit_test ${_ad})
endforeach()
# Ayrı süreçler: yazan süreç öldürülür (kabul 5); döküm programının çıkış kodları.
add_test(NAME sim.kayit.cokme_kurtarma COMMAND ida_kayit_test cokme_kurtarma "$<TARGET_FILE:ida_kayit_test_yazan>")
add_test(NAME sim.kayit.dok_programi COMMAND ida_kayit_test dok_programi "$<TARGET_FILE:ida_kayit_dok>")

# Uzun testler (Windows debug ve ASan derlemelerinde yavaşlar).
set_tests_properties(sim.kayit.rastgele_bozulma sim.kayit.uretici_beklemez sim.kayit.olcum_telemetri
  sim.kayit.cokme_kurtarma sim.kayit.es_zamanli_ureticiler PROPERTIES TIMEOUT 600)
# Zamanlama ölçen testler başkalarıyla aynı anda koşmaz.
set_tests_properties(sim.kayit.uretici_beklemez sim.kayit.olcum_telemetri PROPERTIES RUN_SERIAL TRUE)
