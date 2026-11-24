# tests/butunleme — sim sunucusunun bütünleme testleri (W4-kopru): Z1'in A1 üzerinden koşusu
# (süreç içi karşı taraf ve iki süreç: ida_sim kitaplığı + ida_sil), belirlenimcilik, acil
# durdurma engeli ve HIL 1x'in sözde uçbirimle kartsız sınaması.
#
# Fizik Eigen'e (vcpkg) bağlıdır; vcpkg etkin değilse dizin atlanır (tests/zincir ile aynı).
# ida_sunucu ve ida_sil sim/ altında, tests/'ten sonra tanımlanır; hedef adları üretimde çözülür.

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): tests/butunleme atlandı")
  return()
endif()

add_executable(ida_butunleme_test test_butunleme.cpp test_senaryo.cpp test_sil_belirlenim.cpp test_sil_alim.cpp
  test_a3.cpp a3_istemci.cpp)   # W4-bagla-a3
target_sources(ida_butunleme_test PRIVATE test_sonar.cpp)   # W4-bagla-sonar
target_compile_definitions(ida_butunleme_test PRIVATE "IDA_DEPO_KOKU=\"${PROJECT_SOURCE_DIR}\"")
target_link_libraries(ida_butunleme_test PRIVATE ida_sunucu ida_zincir ida_sil_kosucu ida_core)
ida_ayar_uygula(ida_butunleme_test)
if(NOT MSVC)
  target_compile_options(ida_butunleme_test PRIVATE -Wshadow)
endif()

add_test(NAME butunleme.z1_sil_ic COMMAND ida_butunleme_test z1_sil_ic)
add_test(NAME butunleme.z1_belirlenim COMMAND ida_butunleme_test z1_belirlenim)
add_test(NAME butunleme.acil_durdurma COMMAND ida_butunleme_test acil_durdurma)
# W4-sim-senaryo: A5 senaryo dosyasıyla kurulum ve zaman çizelgesi.
add_test(NAME butunleme.senaryo_z1_esitligi COMMAND ida_butunleme_test senaryo_z1_esitligi $<TARGET_FILE:ida_sil>)
set_tests_properties(butunleme.senaryo_z1_esitligi PROPERTIES TIMEOUT 600)
foreach(_ad senaryo_gecersiz senaryo_dogrulama_uyarilari senaryo_zaman_cizelgesi senaryo_emniyet senaryo_belirlenim senaryo_gorev)
  add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_test ${_ad})
  set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 600)
endforeach()
# ida_sim'in çıkış kodları: geçersiz senaryo 3 (rapor standart hataya), bilinmeyen seçenek 1.
add_test(NAME butunleme.ida_sim_gecersiz_senaryo
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=3
          "-DARGUMANLAR=--senaryo;tests/butunleme/veri/gecersiz.toml;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
# EMNİYET (sol #1): yerleşik plana özgü seçenekler dosyayla birlikte başlamadan reddedilir.
foreach(_secenek acil acil-kalk ariza)
  if(_secenek STREQUAL "ariza")
    set(_deger "4,0")
  else()
    set(_deger "12")
  endif()
  add_test(NAME butunleme.ida_sim_${_secenek}_ve_senaryo
    COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1
            "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--${_secenek};${_deger};--kok;${PROJECT_SOURCE_DIR}"
            "-DILETI=--${_secenek} senaryo dosyasıyla birlikte kullanılamaz"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
endforeach()
# EMNİYET (sol #1 dar denetimi): değerden ve yinelemeden bağımsız; geçersiz değerler yerleşikte de.
function(ida_sim_kullanim_testi ad ileti)
  add_test(NAME butunleme.ida_sim_${ad}
    COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1
            "-DARGUMANLAR=${ARGN};--kok;${PROJECT_SOURCE_DIR};--sure;1"
            "-DILETI=${ileti}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
endfunction()
ida_sim_kullanim_testi(dosya_acil_yinelenen "--acil bir kez verilebilir" --senaryo scenarios/z1.toml --acil 12 --acil -1)
ida_sim_kullanim_testi(dosya_acil_nan "--acil sonlu" --senaryo scenarios/z1.toml --acil nan)
ida_sim_kullanim_testi(dosya_acil_kalk_nan "--acil-kalk sonlu" --senaryo scenarios/z1.toml --acil-kalk nan)
ida_sim_kullanim_testi(dosya_acil_negatif "--acil sonlu" --acil -1 --senaryo scenarios/z1.toml)
ida_sim_kullanim_testi(yerlesik_acil_nan "--acil sonlu" --acil nan)
ida_sim_kullanim_testi(yerlesik_acil_negatif "--acil sonlu" --acil -1)
ida_sim_kullanim_testi(yerlesik_acil_sayi_degil "--acil sonlu" --acil 12x)
ida_sim_kullanim_testi(yerlesik_acil_sonsuz "--acil sonlu" --acil inf)
ida_sim_kullanim_testi(yerlesik_kalk_once "--acil-kalk --acil'den sonra" --acil 12 --acil-kalk 10)
ida_sim_kullanim_testi(yerlesik_acil_yinelenen "--acil bir kez verilebilir" --acil 12 --acil 13)
# Üçüncü dar denetim: strtod alt taşması (−1e−999 → 0, ERANGE) ve taşma; aynı ayrıştırıcı.
ida_sim_kullanim_testi(yerlesik_acil_alt_tasma "--acil sonlu" --acil -1e-999)
ida_sim_kullanim_testi(yerlesik_acil_alt_tasma_pozitif "--acil sonlu" --acil 1e-999)
ida_sim_kullanim_testi(yerlesik_acil_kalk_tasma "--acil-kalk sonlu" --acil 1 --acil-kalk 1e999)
ida_sim_kullanim_testi(sure_alt_tasma "--sure pozitif" --sure 1e-999)
ida_sim_kullanim_testi(tohum_kesirli "--tohum" --tohum 1.5)
ida_sim_kullanim_testi(gunluk_sayi_degil "--gunluk" --gunluk x)
ida_sim_kullanim_testi(ariza_kanal_sayi_degil "kanal 0–7" --ariza 4,x)
add_test(NAME butunleme.ida_sim_tohum_ve_senaryo
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--tohum;2;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
add_test(NAME butunleme.a4_girdi_esitligi COMMAND ida_butunleme_test a4_girdi_esitligi)
add_test(NAME butunleme.sil_gecikmeli_tcp COMMAND ida_butunleme_test sil_gecikmeli_tcp)
set_tests_properties(butunleme.sil_gecikmeli_tcp PROPERTIES TIMEOUT 300)
# A1-K102, K121 (iv): koşu ortasında protokol ihlaliyle düşen oturum; aynı tohumda 10 yüksüz +
# 10 yüklü (6 meşgul döngü) koşunun iki yöndeki bayt akışı aynı.
add_test(NAME butunleme.sil_oturum_dusme_belirlenim COMMAND ida_butunleme_test sil_oturum_dusme_belirlenim 10)
set_tests_properties(butunleme.sil_oturum_dusme_belirlenim PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
# W4-K26 (EMNİYET): SIL'de alım anı (kısıtlayan çerçeve yuva n + 1'in sınırında), akışın sonu
# (FIN ve yazma hatası akıştaki yerinde) ve alım sınırları (kesintisiz 0x00).
# sil_hazirlik_suresi: el sıkışma süre sınırı hazırlık oturumunun süresini kapsamaz; hazırlığın
# kendi sınırı ayrı (W4-K26 mimar kararı).
# Sol 3. denetim: sil_eof_konumu (#1), sil_hazirlik_tekrar (#2), sil_hata_kapanisi (#3).
foreach(_ad sil_kisitlayan_alim_ani sil_akis_sonu_belirlenim sil_sifir_seli sil_hazirlik_suresi sil_butce_konumu
            sil_eof_konumu sil_hazirlik_tekrar sil_hata_kapanisi sil_istisna_kapanisi sil_kurma_siniri)
  add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_test ${_ad})
  set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
endforeach()
add_test(NAME butunleme.sil_iki_surec COMMAND ida_butunleme_test sil_iki_surec $<TARGET_FILE:ida_sil>)
set_tests_properties(butunleme.sil_iki_surec PROPERTIES TIMEOUT 300)
set_tests_properties(butunleme.a4_girdi_esitligi PROPERTIES TIMEOUT 300)
add_test(NAME butunleme.z1_sil_tcp COMMAND ida_butunleme_test z1_sil_tcp $<TARGET_FILE:ida_sil>)
set_tests_properties(butunleme.z1_sil_tcp PROPERTIES TIMEOUT 300)
if(NOT WIN32)
  # Sözde uçbirim yalnız POSIX'te (Windows'ta bu testler tanımlanmaz); gerçek zamanlı 60 s
  # (zamanlamaya duyarlı: ağır yük altında koşturulmaz). Duvar saati eşikleri W4-K24'e göre
  # `GITHUB_ACTIONS` ve sanitizer'lı derlemede yalnız yazdırılır (sim/kopru/test/zamanlama_olcutu.hpp).
  add_test(NAME butunleme.hil_pty COMMAND ida_butunleme_test hil_pty)
  set_tests_properties(butunleme.hil_pty PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  add_test(NAME butunleme.hil_cikarma_takma COMMAND ida_butunleme_test hil_cikarma_takma)
  add_test(NAME butunleme.senaryo_hil_pty COMMAND ida_butunleme_test senaryo_hil_pty)
  # sol #5: --kip olmadan dosyadan seçilen HIL kipinin ölçüm özeti (2 s, sözde uçbirim).
  add_test(NAME butunleme.ida_sim_hil_dosya
    COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> "-DKOK=${PROJECT_SOURCE_DIR}"
            "-DGECICI=${CMAKE_CURRENT_BINARY_DIR}/hil_dosya_kok" -P "${CMAKE_CURRENT_SOURCE_DIR}/hil_dosya.cmake")
  set_tests_properties(butunleme.ida_sim_hil_dosya PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
  set_tests_properties(butunleme.senaryo_hil_pty PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  set_tests_properties(butunleme.hil_cikarma_takma PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endif()

# W4-bagla-a3: A3 sunucusunun ida_sim'e bağlanması (kabul 2, 4, 5; sim/sunucu/BENIOKU.md "A3").
foreach(_ad a3_belirlenim a3_emniyet_yolu a3_emniyet_ret a3_calistirma a3_hazirlik_durdur a3_hazirlik_kapanis a3_ayirma a3_durdur_dolu_depo)
  add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_test ${_ad})
  set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 600)
endforeach()
if(NOT WIN32)
  # Kabul 5 (ölçüm): HIL 1x 60 s, sözde uçbirim (POSIX), A3 açık; zamanlamaya duyarlı.
  add_test(NAME butunleme.a3_hil_olcum COMMAND ida_butunleme_test a3_hil_olcum)
  set_tests_properties(butunleme.a3_hil_olcum PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  # W4-K28 sol-3 #2 EMNİYET: sürekli veri dönen, yazımı ilerlemeyen taşımada kapanış sınırlı sürede döner.
  add_test(NAME butunleme.a3_kapanis_sel COMMAND ida_butunleme_test a3_kapanis_sel)
  set_tests_properties(butunleme.a3_kapanis_sel PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
  # W4-K28 E EMNİYET: DISARM → DISARM → acil bas gerçek kitaplık yolundan (koşarken, hazırlıkta, yuklendi).
  add_test(NAME butunleme.a3_kisitlayan_ardisik COMMAND ida_butunleme_test a3_kisitlayan_ardisik)
  set_tests_properties(butunleme.a3_kisitlayan_ardisik PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
  # W4-K28 (3) EMNİYET: hazırlığın son el sıkışma pompasında kabul edilen durdur kazanır (SIL ve HIL).
  add_test(NAME butunleme.a3_hazirlik_son_durdur COMMAND ida_butunleme_test a3_hazirlik_son_durdur)
  set_tests_properties(butunleme.a3_hazirlik_son_durdur PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
  # W4-K28 (1): HIL'de duraklatma (onaylı dahil) reddedilir; 4 s, sözde uçbirim.
  add_test(NAME butunleme.a3_hil_duraklat_ret COMMAND ida_butunleme_test a3_hil_duraklat_ret)
  set_tests_properties(butunleme.a3_hil_duraklat_ret PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endif()
# Kabul 3: ida_sim + panelin A3 istemcisi (Node; web/src/a3, web/'e dosya eklenmez). Node yoksa
# kaydedilmez; web/node_modules (esbuild) yoksa 77 döner ve atlanır.
find_program(IDA_NODE_BUTUNLEME node)
if(IDA_NODE_BUTUNLEME)
  add_test(NAME butunleme.a3_uctan_uca_web
    COMMAND "${IDA_NODE_BUTUNLEME}" "${CMAKE_CURRENT_SOURCE_DIR}/a3_uctan_uca.mjs" "$<TARGET_FILE:ida_sim>"
            "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/web" "${CMAKE_CURRENT_BINARY_DIR}/node")
  set_tests_properties(butunleme.a3_uctan_uca_web PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 180)
  # W4-bagla-sonar kabul 5: sonar kanallarının panel istemcisiyle çözülen değerleri sonar CSV'siyle.
  add_test(NAME butunleme.a3_sonar_web
    COMMAND "${IDA_NODE_BUTUNLEME}" "${CMAKE_CURRENT_SOURCE_DIR}/a3_sonar_web.mjs" "$<TARGET_FILE:ida_sim>"
            "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/web" "${CMAKE_CURRENT_BINARY_DIR}/node_sonar")
  set_tests_properties(butunleme.a3_sonar_web PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 240)
endif()

# W4-bagla-sonar (docs/gorevler/W4-bagla-sonar.md kabul 1–6; sim/sunucu/BENIOKU.md "Sonar").
foreach(_ad sonar_kapali_esitlik sonar_belirlenim sonar_ping1d sonar_ping360 sonar_red sonar_a3)
  add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_test ${_ad})
  set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 600)
endforeach()
if(NOT WIN32)
  # Kabul 6 (ölçüm): HIL 1x 60 s, sözde uçbirim (POSIX), Ping360 1 200 ve 600 örnek; zamanlamaya duyarlı.
  foreach(_n 1200 600)
    add_test(NAME butunleme.sonar_hil_olcum_${_n} COMMAND ida_butunleme_test sonar_hil_olcum ${_n})
    set_tests_properties(butunleme.sonar_hil_olcum_${_n} PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  endforeach()
endif()
