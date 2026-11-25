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
target_sources(ida_butunleme_test PRIVATE test_duraklat.cpp)   # W4-duraklat
target_sources(ida_butunleme_test PRIVATE test_gorev_sil.cpp)   # W5-gorev-yurutucu
target_sources(ida_butunleme_test PRIVATE test_sunucu_kucuk.cpp)   # W4-sunucu-kucuk
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
            sil_eof_konumu sil_hazirlik_tekrar sil_hata_kapanisi sil_istisna_kapanisi sil_kurma_siniri
            sil_sinyal_kapanisi sil_hazirlik_kurma_siniri)
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
foreach(_ad a3_belirlenim a3_emniyet_yolu a3_emniyet_ret a3_calistirma a3_hazirlik_durdur a3_hazirlik_kapanis a3_ayirma a3_durdur_dolu_depo a3_sinyal)
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
endif()
# W4-duraklat (docs/gorevler/W4-duraklat.md kabul 2–5; sim/sunucu/BENIOKU.md "Duraklatma").
foreach(_ad duraklat_sil_esitlik duraklat_senaryo_operator duraklat_ayni_tur_cikis duraklat_bas_birak duraklat_ayirma
           duraklat_taban_ozet)
  add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_test ${_ad})
  set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 600)
endforeach()
if(NOT WIN32)
  # HIL (sözde uçbirim, POSIX): kısıtlayan ardışık istekler (SIL de), kartın oturum kapanışı ve
  # eşzamansız çıktıları, ilk TF öncesi duraklatma, tel (yalnız IDA_KEEPALIVE).
  foreach(_ad duraklat_kisitlayan_ardisik duraklat_hil_oturum_sonu duraklat_hil_esz duraklat_hil_ilk_tf duraklat_hil_tel)
    add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_test ${_ad})
    set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
  endforeach()
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
foreach(_ad sonar_kapali_tutarlilik sonar_belirlenim sonar_ping1d sonar_ping360 sonar_red sonar_kurulum sonar_a3
           sonar_ayirma)
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

# W5-gorev-yurutucu kabul 6: görev senaryosu (yüzeyde iki yol noktası, dalış, su altında yol
# noktası, yüzeye çıkış) süreç içi SIL'de COMPLETE olur; yer gerçeğine göre ölçümler ve iki koşunun
# bayt eşitliği. Ayrıca aynı senaryo `ida_sim --kip sil` ile çıkış kodu 0.
add_test(NAME butunleme.gorev_sil COMMAND ida_butunleme_test gorev_sil)
set_tests_properties(butunleme.gorev_sil PROPERTIES TIMEOUT 600)
add_test(NAME butunleme.ida_sim_gorev_senaryosu
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--kip;sil;--senaryo;scenarios/gorev-havuz.toml;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
set_tests_properties(butunleme.ida_sim_gorev_senaryosu PROPERTIES TIMEOUT 600)

# W4-sunucu-kucuk (docs/gorevler/W4-sunucu-kucuk.md kabul 1, 3–5; sim/sunucu/BENIOKU.md).
# Kapsam 6 / kabul 5: açık su sonar örneği dosyası SIL'de koşar, iki koşu bayt bayt aynı.
add_test(NAME butunleme.sonar_acik_su_dosyasi COMMAND ida_butunleme_test sonar_acik_su_dosyasi)
set_tests_properties(butunleme.sonar_acik_su_dosyasi PROPERTIES TIMEOUT 600)
# `--sonar=acik|kapali` ayrıştırması (kapsam 5): geçersiz değer 1, kapali ile --ping360-ornek 1 (sonar ister).
add_test(NAME butunleme.ida_sim_sonar_degeri_gecersiz
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1 -DILETI=--sonar
          "-DARGUMANLAR=--senaryo;scenarios/sonar-acik-su.toml;--sonar=belki;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
add_test(NAME butunleme.ida_sim_sonar_kapali_ping360
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1
          "-DARGUMANLAR=--senaryo;scenarios/sonar-acik-su.toml;--sonar=kapali;--ping360-ornek;600;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
add_test(NAME butunleme.ida_sim_sonar_acik_su
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--senaryo;scenarios/sonar-acik-su.toml;--sonar=acik;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
set_tests_properties(butunleme.ida_sim_sonar_acik_su PROPERTIES TIMEOUT 600)
# W4-sunucu-kucuk kabul 2 (özet satırı): düzgün SIL koşusunda `kapanış: düzgün` ve alanlar sayaçlarla
# tutarlı (kaçırılan parti 0; oturum 2 = hazırlık oturumu + çalıştırma oturumu, hazırlık 1; kapanış
# yazımı eksik 0; son hata yok).
add_test(NAME butunleme.ida_sim_ozet_duzgun
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--sure;3;--gunluk;1"
          "-DILETILER=ida_sim: kapanış: düzgün|kaçırılan parti 0,|yinelenen yanıt reddi 0, yeniden bağlanma 0|, oturum 2, kapanış yazımı eksik 0, son hata: yok|kopma 0, hazırlık 1,"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
# Bilgi satırı (kapsam 5; z1: araç dosyasından 2 etkin sonar, sonarsız koşar): bayraksız koşuda etkin sonar bildirilir, `--sonar=kapali` ile bildirilmez.
add_test(NAME butunleme.ida_sim_sonar_bilgi_satiri
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--gunluk;1;--kok;${PROJECT_SOURCE_DIR}"
          "-DILETI=senaryoda 2 etkin sonar var" -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
add_test(NAME butunleme.ida_sim_sonar_bilgi_satiri_kapali
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--sonar=kapali;--gunluk;1;--kok;${PROJECT_SOURCE_DIR}"
          "-DYOK=etkin sonar var" -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
set_tests_properties(butunleme.ida_sim_sonar_bilgi_satiri butunleme.ida_sim_sonar_bilgi_satiri_kapali PROPERTIES TIMEOUT 600)
# Bağlantı kabulünün son diliminde gelen kapanış isteği sinyal sınıflamasıyla döner.
add_test(NAME butunleme.kabul_son_dilim_sinyali COMMAND ida_butunleme_test kabul_son_dilim_sinyali)
set_tests_properties(butunleme.kabul_son_dilim_sinyali PROPERTIES TIMEOUT 60)
if(NOT WIN32)
  # Bilinen hata çıkışı: var olmayan seri aygıt → çıkış 2, `kapanış: hata` ve son hata alanı.
  add_test(NAME butunleme.ida_sim_ozet_hata
    COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=2
            "-DARGUMANLAR=--kip;hil;--seri;/dev/ida_olmayan_aygit;--sure;1"
            "-DILETILER=ida_sim: kapanış: hata|oturum 0, kapanış yazımı eksik 0, son hata: seri_ac("
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
endif()
if(NOT WIN32)
  # Kapsam 1 / kabul 1 (HIL, sözde uçbirim; gerçek zamanlı ≈ 4–6 s) ve kapsam 4 / kabul 4.
  add_test(NAME butunleme.hil_sinyal_kapanisi COMMAND ida_butunleme_test hil_sinyal_kapanisi)
  set_tests_properties(butunleme.hil_sinyal_kapanisi PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
  add_test(NAME butunleme.senaryo_hil_dosyalari COMMAND ida_butunleme_test senaryo_hil_dosyalari)
  set_tests_properties(butunleme.senaryo_hil_dosyalari PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  # Gerçek süreç: ida_sim'e SIGINT/SIGTERM (kabul 1): çıkış kodu 5, özet satırı, eksiksiz CSV; ikinci
  # sinyal hemen sonlandırır (birim testi sim.sunucu.sinyal_ikinci).
  foreach(_sinyal INT TERM)
    add_test(NAME butunleme.sinyal_surec_${_sinyal}
      COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/sinyal_surec.sh" $<TARGET_FILE:ida_sim> $<TARGET_FILE:ida_sil>
              "${CMAKE_CURRENT_BINARY_DIR}/sinyal_surec_${_sinyal}" ${_sinyal})
    set_tests_properties(butunleme.sinyal_surec_${_sinyal} PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
  endforeach()
endif()

# W5-bagla-a2 (docs/gorevler/W5-bagla-a2.md kabul 1–5; sim/sunucu/BENIOKU.md "A2"): A2 sunucusunun ida_sim'e
# bağlanması. Karşı uç gerçek istemcidir (`ida_viz_a2`: viz/a2'nin oturumu ve kodeği); sahte istemci düzeneği
# (gerçek soket, "damla okuma") sim/a2/test/test_destek.*'ten derlenir (kopyalanmaz). Testler sabit 47210
# portunu ve gerçek ida/a2.kilit dosyasını kullanmaz (işletim sisteminin verdiği port; kilit geçici dizinde).
find_package(Threads REQUIRED)
add_executable(ida_butunleme_a2_test test_a2.cpp "${PROJECT_SOURCE_DIR}/sim/a2/test/test_destek.cpp")
target_include_directories(ida_butunleme_a2_test PRIVATE
  "${PROJECT_SOURCE_DIR}/sim/a2/test" "${PROJECT_SOURCE_DIR}/sim/a2/src")
target_compile_definitions(ida_butunleme_a2_test PRIVATE "IDA_DEPO_KOKU=\"${PROJECT_SOURCE_DIR}\"")
target_link_libraries(ida_butunleme_a2_test PRIVATE ida_sunucu ida_a2_sahte_dunya ida_viz_a2 Threads::Threads)
ida_ayar_uygula(ida_butunleme_a2_test)
if(NOT MSVC)
  target_compile_options(ida_butunleme_a2_test PRIVATE -Wshadow)
endif()
foreach(_ad a2_varsayilan a2_belirlenim a2_uctan_uca a2_duraklat a2_kapanis sil_hiz)
  add_test(NAME butunleme.${_ad} COMMAND ida_butunleme_a2_test ${_ad})
  set_tests_properties(butunleme.${_ad} PROPERTIES TIMEOUT 600)
endforeach()
if(NOT WIN32)
  # Kabul 2 (ölçüm): kartsız HIL 1x (sözde uçbirim, POSIX), iki kez 12 s; zamanlamaya duyarlı.
  add_test(NAME butunleme.a2_hil_donuk COMMAND ida_butunleme_a2_test a2_hil_donuk)
  set_tests_properties(butunleme.a2_hil_donuk PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
# ida_sim süreç olarak: seçenekler ve kapanış özet satırı (kapsam 6).
add_test(NAME butunleme.ida_sim_a2_port_gecersiz
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1 "-DILETI=--a2-port 1024–65535"
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--a2-port;80;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
add_test(NAME butunleme.ida_sim_a2_yerlesik_senaryo
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1 "-DILETI=--a2-port senaryo dosyası ister"
          "-DARGUMANLAR=--a2-port;47999;--sure;1" -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
add_test(NAME butunleme.ida_sim_sil_hiz_hil
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=1 "-DILETI=--sil-hiz yalnız SIL"
          "-DARGUMANLAR=--senaryo;scenarios/z1-hil.toml;--karsi;pty;--sil-hiz;1;--kok;${PROJECT_SOURCE_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
# A2 açık (boş port, geçici kilit), istemci yok: koşu düzgün; özet satırında A2 durumu ve sayaçları `oturum`
# alanından önce; var olan alanlar yerinde.
add_test(NAME butunleme.ida_sim_a2_ozet
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--a2-sinama-bos-port;--a2-sinama-kilit;${CMAKE_CURRENT_BINARY_DIR}/a2_ozet/a2.kilit;--gunluk;1;--kok;${PROJECT_SOURCE_DIR}"
          "-DILETILER=ida_sim: kapanış: düzgün|sonar taşma (kuyruk 0, teslim 0, tampon 0), A2 dinliyor (port |bağlantı 0, biten oturum 0, son kopma yok|yayın: sınır 72000, düşen durum 0, kayıp 0, olay taşması 0, geçersiz çağrı 0), oturum 2, kapanış yazımı eksik 0, son hata: yok|ida_sim: A2 127.0.0.1:|ida_sim: A2: çalıştırma tanımı verildi"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
# A2'siz (kilit dosyası açılamıyor: üst dizin yerine bir dosya): koşu düzgün sürer, özet satırında neden.
add_test(NAME butunleme.ida_sim_a2siz_ozet
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--a2-sinama-bos-port;--a2-sinama-kilit;${CMAKE_CURRENT_LIST_FILE}/a2.kilit;--gunluk;1;--kok;${PROJECT_SOURCE_DIR}"
          "-DILETILER=ida_sim: kapanış: düzgün|A2 yok: kilit_acilamadi (|), oturum 2, kapanış yazımı eksik 0, son hata: yok"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
# W5-K31: `--a2-port` yokken çıktıda A2 izi yok (özet satırı bu iş öncesiyle aynı biçimde).
add_test(NAME butunleme.ida_sim_a2_yok_cikti
  COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:ida_sim> -DBEKLENEN=0 -DYOK=A2
          "-DARGUMANLAR=--senaryo;scenarios/z1.toml;--gunluk;1;--kok;${PROJECT_SOURCE_DIR}"
          "-DILETI=sonar taşma (kuyruk 0, teslim 0, tampon 0), oturum 2, kapanış yazımı eksik 0, son hata: yok"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cikis_kodu.cmake")
set_tests_properties(butunleme.ida_sim_a2_ozet butunleme.ida_sim_a2siz_ozet butunleme.ida_sim_a2_yok_cikti PROPERTIES TIMEOUT 600)
