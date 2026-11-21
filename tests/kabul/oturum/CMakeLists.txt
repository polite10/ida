# tests/kabul/oturum — ida_link üst katmanının (oturum, parti, komut sırası, olay) bağımsız
# kabul testleri (W3-oturum, kabul md.2 ve md.4 ile oturum değişimindeki temizlik; yazan
# ida-test-yazari).
#
# Testler yalnız A1 ve A4 sözleşmelerinden, port.h'den, görev belgelerinden (W3-oturum,
# W3-ortak mimar kararları W3-K10/K11, W1-uyum D-17) ve modülün dışa açık başlığından
# (protocol/ida_link/include/ida_link/ust.h) türetildi; modülün uygulama kaynağı, iç başlığı
# ve uygulayıcının testleri kullanılmaz. Modüle sahte platform işlev tablosu ve tel
# katmanının kodlayıcısıyla üretilen A1 bayt akışıyla erişilir; çekirdek sahtedir.
#
# CTest adları kural numarasını taşır:
#   kabul.oturum.A1-K63.komut_sirasi      genişletici → daraltıcı (açık, bekleyen, ertelenmiş),
#                                         nihailik, mandal, A1-K64, W3-K10 ihlal vakası, W3-K11
#   kabul.oturum.A1-K80.olay              olay yinelemesi, onay, son geçerlilik, oturum sonu
#   kabul.oturum.A1-K22.oturum_degisimi   oturum sonunda temizlik, A1-K107, eski sess
#   kabul.oturum.A1-K63.rastgele          sabit tohumlu rastgele komut dizileri
#
# Yalnız PC kökünde derlenir (tests/ yalnız host'ta eklenir).

ida_unity_kur()

add_library(kabul_oturum_ortak STATIC kabul_oturum.c)
target_include_directories(kabul_oturum_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# ida_link_ust, ida_link ve ida_mavlink_uretilen protocol/ida_link'te (bu dizinden sonra)
# tanımlanır; ad üretim anında çözülür.
target_link_libraries(kabul_oturum_ortak PUBLIC ida_link_ust ida_link ida_mavlink_uretilen ida_unity)
add_dependencies(kabul_oturum_ortak ida_link_uret)
ida_ayar_uygula(kabul_oturum_ortak CEKIRDEK)

function(kabul_oturum_testi ad kaynak)
  set(_hedef "kabul_oturum_${kaynak}")
  add_executable(${_hedef} "${kaynak}.c")
  target_link_libraries(${_hedef} PRIVATE kabul_oturum_ortak)
  ida_ayar_uygula(${_hedef} CEKIRDEK)
  add_test(NAME "kabul.oturum.${ad}" COMMAND ${_hedef})
endfunction()

kabul_oturum_testi(A1-K63.komut_sirasi     kabul_komut_sirasi)
kabul_oturum_testi(A1-K80.olay             kabul_olay)
kabul_oturum_testi(A1-K22.oturum_degisimi  kabul_oturum_degisimi)
kabul_oturum_testi(A1-K63.rastgele         kabul_rastgele)
