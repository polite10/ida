# tests/kabul/kopru — sim sunucusunun A1 köprüsünün bağımsız kabul testleri (W4-kopru, kabul
# md.3 ve md.4: fırlatıcı olaylarının uygulanması, sanal emniyet girdileri, güvenli çıktı,
# aktüatör zamanlaması, acil durdurma engeli; yazan ida-test-yazari).
#
# Testler yalnız A1 sözleşmesinden (docs/sozlesmeler/A1-kart-baglantisi.md 1.0), iskeletten
# (§3.2, §4, §5), görev belgelerinden (W4-kopru, W3-ortak) ve açık başlıklardan
# (sim/kopru/include, sim/sunucu/include, sim/sensor/include, protocol/ida_link/include)
# türetildi; köprünün ve sunucunun uygulama kaynağı ve uygulayıcının testleri kullanılmaz.
# Karşı taraf sahte karttır (sahte_kart.*): kart çerçeveleri tel katmanının kart rolündeki
# kodlayıcısıyla üretilir; gerçek çekirdek ve ida_link üst katmanı kullanılmaz.
#
# CTest adları kabul.kopru.<kural>.<vaka>. Fizik Eigen'e (vcpkg) bağlı olduğundan sunucu
# düzeyindeki vakalar (acil durdurma engeli, A1 üzerinden kos()) yalnız vcpkg etkinken eklenir;
# hedef adları (ida_kopru, ida_sunucu) sim/ altında, bu dizinden sonra tanımlanır ve üretim
# anında çözülür.

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  # ida_kopru ida_sensor'a, o da ida_fizik'e (Eigen) bağlıdır.
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): tests/kabul/kopru atlandı")
  return()
endif()

find_package(Threads REQUIRED)

function(kabul_kopru_ayar hedef)
  ida_ayar_uygula(${hedef})
  if(NOT MSVC)
    target_compile_options(${hedef} PRIVATE -Wshadow)
  endif()
endfunction()

add_library(kabul_kopru_ortak STATIC sahte_kart.cpp duzenek.cpp)
target_include_directories(kabul_kopru_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kabul_kopru_ortak PUBLIC ida_kopru ida_link ida_mavlink_uretilen ida_core)
add_dependencies(kabul_kopru_ortak ida_link_uret)
kabul_kopru_ayar(kabul_kopru_ortak)

# Köprü düzeyi (Kopru + sahte kart, bellek içi taşıma).
add_executable(kabul_kopru kabul_kopru.cpp kabul_olay.cpp kabul_zaman.cpp kabul_saglamlik.cpp
  kabul_geri_okuma.cpp kabul_w4k15.cpp)
target_link_libraries(kabul_kopru PRIVATE kabul_kopru_ortak)
kabul_kopru_ayar(kabul_kopru)

set(_kabul_kopru_vakalari
  # Vaka 1: yetkili durum izin vermiyor; her girdi tek başına engel (EMNİYET)
  A1-K86.acil_durdurma_tek_engel
  A1-K86.fis_cikik_tek_engel
  A1-K86.pim_takili_tek_engel
  A1-K88.izin_kapali_tek_engel
  A1-K86.izin_suresi_doldu
  A1-K88.tup_bos
  # Vaka 2: uygun durumda zincir
  A1-K88.zincir_fire_yalniz_hedef_tup
  A1-K118.fire_tutarsiz_yanit
  A1-K118.yanit_gelmedi
  # Vaka 3: tekillik
  A1-K81.yinelenen_olay
  A1-K81.oturum_sonrasi_ayni_kimlik
  A1-K82.farkli_boot_ayni_sayac
  # Vaka 4: son geçerlilik ve revizyon
  A1-K84.suresi_gecmis_olay
  A1-K84.son_gecerlilik_siniri
  A1-K106.bayat_revizyon
  A1-K106.sifir_revizyon
  # Vaka 5: fizik sınırındaki sıra
  A1-K85.close_open_ayni_sinir
  A1-K85.acil_durdurma_ve_olay_ayni_sinir
  A1-K85.close_fire_ayni_sinir
  A1-K85.gevseme_olaya_yaramaz
  A1-K85.kisitlama_varken_gevseme_reddi
  # Vaka 7: aktüatör zamanlaması
  A1-K74.uygulama_ani
  A1-K72.gec_yanit
  A1-K72.yinelenen_yanit
  A1-K77.gecerlilik_siniri
  A1-K74.thr_valid_sifir
  A1-K78.eszamansiz_guvenli
  A1-K23.oturum_kapaninca_notr
  A1-K23.t_bag_asimi
  # Vaka 8: emniyet örneği
  A1-K56.kisitlama_ertelenmez
  A1-K56.ayni_pencerede_kapanip_acilma
  A1-K56.geri_okuma_fis_takilmasi      # gerçek sensör kümesi (W4-K4, W4-K5)
  A1-K56.geri_okuma_pim_cikarilmasi
  # Vaka 9: dayanıklılık
  A1-K109.rastgele_cerceveler_acil
  A1-K109.rastgele_cerceveler_fis
  # W4-K15: olay kümesi sayıyla kapanır (EMNİYET)
  W4-K15.3.tam_yuvaya_yeni_open
  W4-K15.3.tam_yuvaya_yeni_fire
  W4-K15.3.sayiyi_asan_genisletici
  W4-K15.1.eksik_kume_open_uygulanmaz
  W4-K15.1.eksik_kume_close_beklemez
  W4-K15.3.is_safe_yanit_sonra_fire
  W4-K15.3.ayni_kimlik_yineleme_ihlal_degil
  W4-K15.3.sayiyi_asan_safety_close
  W4-K15.2.bekleyen_yanit_t_bag_yenilemez
  W4-K15.4.bad_input_olay_beklemez)
foreach(_ad IN LISTS _kabul_kopru_vakalari)
  add_test(NAME kabul.kopru.${_ad} COMMAND kabul_kopru ${_ad})
endforeach()

# Sunucu düzeyi (ida_sunucu::kos, SIL, TCP; karşı taraf sahte kart): acil durdurma engeli.
add_executable(kabul_kopru_sunucu kabul_sunucu.cpp)
target_link_libraries(kabul_kopru_sunucu PRIVATE kabul_kopru_ortak ida_sunucu Threads::Threads)
kabul_kopru_ayar(kabul_kopru_sunucu)

set(_kabul_sunucu_vakalari
  W3-K1.acil_durdurmada_itki_notr
  W3-K1.acil_kalkinca_eski_komut_donmez
  A1-K56.sunucu_acil_darbe
  W3-K1.rastgele_cerceveler_acil)
foreach(_ad IN LISTS _kabul_sunucu_vakalari)
  add_test(NAME kabul.kopru.${_ad} COMMAND kabul_kopru_sunucu ${_ad})
  set_tests_properties(kabul.kopru.${_ad} PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
endforeach()
