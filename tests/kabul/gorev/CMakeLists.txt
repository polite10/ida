# tests/kabul/gorev — W3-gorev işinin (kipler, araç silahlandırması, arıza güvenliği)
# bağımsız kabul testleri (görev belgesi docs/gorevler/W3-gorev.md, kabul md.2–4; yazan
# ida-test-yazari).
#
# Testler yalnız A4 sözleşmesinden (docs/sozlesmeler/A4-port.md, 1.0), açık başlıktan
# (core/include/ida/port.h), protocol/komutlar.json'dan ve görev belgesinden türetildi;
# çekirdeğin uygulama kaynağı (core/src), uygulayıcının testleri ve tasarım notu
# (core/TASARIM.md) kullanılmaz. Çekirdeğe yalnız port.h'nin işlevleriyle erişilir.
#
# CTest adları görev belgesinin kabul maddesini taşır:
#   kabul.gorev.md2.arm-onkosul   ARM ön koşulları: her biri için ret ve neden, kabul
#   kabul.gorev.md3.ariza         arıza eylemleri armed ya da mode'u genişletmez
#   kabul.gorev.md4.dizi          DISARM ile aynı adımda ARM, yinelenen komut, emniyet
#                                 kısıtı; sabit tohumlu rastgele diziler ve kapsam
#
# Yalnız PC kökünde derlenir (tests/ yalnız host'ta eklenir).

ida_unity_kur()

add_library(kabul_gorev_ortak STATIC kabul_gorev.c)
target_include_directories(kabul_gorev_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kabul_gorev_ortak PUBLIC ida_core ida_unity)
ida_ayar_uygula(kabul_gorev_ortak CEKIRDEK)

function(kabul_gorev_testi ad kaynak)
  set(_hedef "kabul_gorev_${kaynak}")
  add_executable(${_hedef} "${kaynak}.c")
  target_link_libraries(${_hedef} PRIVATE kabul_gorev_ortak)
  ida_ayar_uygula(${_hedef} CEKIRDEK)
  add_test(NAME "kabul.gorev.${ad}" COMMAND ${_hedef})
endfunction()

kabul_gorev_testi(md2.arm-onkosul  kabul_arm)
kabul_gorev_testi(md3.ariza        kabul_ariza)
kabul_gorev_testi(md4.dizi         kabul_dizi)
