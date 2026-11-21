# tests/kabul/cekirdek — ortak çekirdeğin iskeletinin bağımsız kabul testleri (W2-cekirdek,
# kabul bölümünün son paragrafı; yazan ida-test-yazari).
#
# Testler yalnız A4 sözleşmesinden (docs/sozlesmeler/A4-port.md; 130 kurallı son metin),
# açık başlıklardan (core/include/ida/port.h, algilama_kaydi.h) ve görev belgesinden
# türetildi; çekirdeğin uygulama kaynağı (core/src), uygulayıcının testleri ve test desteği
# (core/test) ile tasarım notu (core/TASARIM.md) kullanılmaz. Çekirdeğe yalnız port.h'nin
# işlevleri üzerinden erişilir; örnek belleği ida_core_info'nun bildirdiği boy ve hizayla
# düzenekte ayrılır (A4-K98).
#
# CTest adları kural numarasını taşır:
#   kabul.cekirdek.A4-3.11           değerlendirme sırası ve öncelik tablosu (init, step,
#                                    tick, background)
#   kabul.cekirdek.A4-4.14           dönüş koduna göre çıktının her alanı; A4-K27, A4-K126
#   kabul.cekirdek.A4-4.15           işlevlerin işaretçi önkoşulları
#   kabul.cekirdek.A4-K113.daraltma  (b) adımı ve emniyet değişmezleri; A4-K124, 3.17
#   kabul.cekirdek.A4-K24            belirlenimcilik: sabit tohumlu karışık uzun diziler
#   kabul.cekirdek.A4-K80.param      parametre işlevleri, kodlanmış blok, A4-K78
#
# Önceki "belirsiz" grubunun bütün vakaları A4'ün fark turunda kesinleşti ve yukarıdaki
# gruplara kesin test olarak taşındı; grup kaldırıldı.
#
# Yalnız PC kökünde derlenir (tests/ yalnız host'ta eklenir).

ida_unity_kur()

add_library(kabul_cekirdek_ortak STATIC kabul_cekirdek.c)
target_include_directories(kabul_cekirdek_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kabul_cekirdek_ortak PUBLIC ida_core ida_unity)
ida_ayar_uygula(kabul_cekirdek_ortak CEKIRDEK)

function(kabul_cekirdek_testi ad kaynak)
  set(_hedef "kabul_cekirdek_${kaynak}")
  add_executable(${_hedef} "${kaynak}.c")
  target_link_libraries(${_hedef} PRIVATE kabul_cekirdek_ortak)
  ida_ayar_uygula(${_hedef} CEKIRDEK)
  add_test(NAME "kabul.cekirdek.${ad}" COMMAND ${_hedef})
endfunction()

kabul_cekirdek_testi(A4-3.11            kabul_sira)
kabul_cekirdek_testi(A4-4.14            kabul_cikti)
kabul_cekirdek_testi(A4-4.15            kabul_onkosul)
kabul_cekirdek_testi(A4-K113.daraltma   kabul_daraltma)
kabul_cekirdek_testi(A4-K24             kabul_belirlenim)
kabul_cekirdek_testi(A4-K80.param       kabul_param)
