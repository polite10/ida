# sim/a3 testleri (sim.a3.*). Beyaz kutu: iç başlıkları (src/) görür. Soket testleri yalnız
# 127.0.0.1'de ve port 0 ile (işletim sistemi atar) dinler.

add_executable(ida_a3_test
  test_akis.cpp
  test_ana.cpp
  test_birim.cpp
  test_dayaniklilik.cpp
  test_destek.cpp
  test_erisim.cpp
  test_komut.cpp
  test_saglam.cpp
  test_uyum.cpp
  test_yavas.cpp)
target_include_directories(ida_a3_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(ida_a3_test PRIVATE ida_a3_sahte_dunya Threads::Threads)
target_compile_definitions(ida_a3_test PRIVATE
  "IDA_A3_KATALOG_YOLU=\"${PROJECT_SOURCE_DIR}/docs/sozlesmeler/sema/a3-kanallar.json\""
  "IDA_A3_KOMUTLAR_YOLU=\"${PROJECT_SOURCE_DIR}/protocol/komutlar.json\"")
ida_a3_ayar(ida_a3_test)

set(_ida_a3_testleri
  # birim
  json desen sema ozet http_ayristirma ws_cerceve halka katalog_tablosu kodlayici
  # erişim (EMNİYET; kabul 1)
  dongu_disi_adres erisim_http erisim_ws erisim_olayi_hiz_siniri statik_simgesel_bag
  # akış (A3-K17…K49)
  merhaba_ve_zorunlu abonelik_ve_ozet seyreltme_hizi olay_gecmisi ws_kapanis_kodlari canlilik
  olay_kuyrugu_tasmasi kapanis_1001
  # komut yolu ve emniyet girdileri (EMNİYET; kabul 5)
  calistirma_akisi yineleme govde_sinirlari emniyet_kisitlayan emniyet_gevseten sonuc_aynen
  kuyruk_dolu_kisitlayan yer_istasyonu kisitlayan_bayat_run_id  # W4-K19
  kisitlayan_mandal_siniri jeton_erken_ret  # W4-K22, A3-K94 (sol #2, #3)
  # ölçüm ve dayanıklılık (kabul 4, 6)
  yavas_istemci yavas_istemci_ret_duser rastgele_cekirdek rastgele_ayristirici cok_baglanti
  # sol #4-#11, #13
  hz_aralik_disi govde_okuma_butcesi pipeline_baski head_sonra_get chunked_bozuk ws_en_kisa_uzunluk baslatma_hatasi uyandirma_eslesmesi olay_gecmisi_cok_bayt
  # sol W4-a3-2 #1-#3
  baglanti_siniri_emniyet baski_altinda_emniyet baski_altinda_emniyet_soket jeton_erken_ret_tablo statik_harf_duyarli
  # W4-K23 (sol-3)
  baglanti_siniri_dogrulanmis baglanti_siniri_503 baski_uzun_kisitlayan baski_chunked_eof baski_sira_tasmasi jeton_okunamayan statik_kok_tutulur)
foreach(_ad IN LISTS _ida_a3_testleri)
  add_test(NAME sim.a3.${_ad} COMMAND ida_a3_test ${_ad})
endforeach()
# Ağır testler aynı anda koşmaz (soket ve zamanlama ölçümü).
set_tests_properties(sim.a3.yavas_istemci sim.a3.yavas_istemci_ret_duser sim.a3.cok_baglanti sim.a3.erisim_http
  sim.a3.baski_altinda_emniyet_soket PROPERTIES RUN_SERIAL TRUE)

# A3-K02: döngü arabirimi dışı adres verilince program dinlemez ve hata koduyla çıkar.
add_test(NAME sim.a3.sahte_dongu_disi COMMAND ida_a3_sahte --adres 0.0.0.0 --port 0 --sure 1)
set_tests_properties(sim.a3.sahte_dongu_disi PROPERTIES WILL_FAIL TRUE)

# Node ile koşan uyum testleri (web/src/a3'ün çözücüsü ve bağlantı kitaplığı; web/'e dosya eklenmez).
# Node yoksa kaydedilmez; web/node_modules (esbuild) yoksa test 77 döner ve atlanır.
find_program(IDA_NODE node)
if(IDA_NODE)
  add_test(NAME sim.a3.uyum_web_cozucu
    COMMAND "${IDA_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/uyum.mjs"
            "$<TARGET_FILE:ida_a3_test>" "${PROJECT_SOURCE_DIR}/web" "${CMAKE_CURRENT_BINARY_DIR}/node")
  add_test(NAME sim.a3.uctan_uca_web
    COMMAND "${IDA_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/uctan_uca.mjs"
            "$<TARGET_FILE:ida_a3_sahte>" "${PROJECT_SOURCE_DIR}/web" "${CMAKE_CURRENT_BINARY_DIR}/node")
  set_tests_properties(sim.a3.uyum_web_cozucu sim.a3.uctan_uca_web PROPERTIES SKIP_RETURN_CODE 77)
endif()
