# tests/kabul/kontrol — acil durdurmada çekirdeğin aktüatör çıktısının bağımsız kabul testleri
# (W5-kontrol-acil; EMNİYET; yazan ida-test-yazari).
#
# Testler yalnız şunlardan türetildi: docs/gorevler/W5-kontrol-acil.md ("Kural", "Kabul",
# "Bağımsız kabul testleri"), A4 sözleşmesi (docs/sozlesmeler/A4-port.md: sürekli komut alanları,
# nötr güvenli çıktı A4-K55, emniyet örneği ve etkin değerler A4-K116, parametre komutları A4-K81),
# core/include/ida/port.h, docs/gorevler/W5-ortak.md (W5-K4 ve eki, W5-K20, W5-K26) ve var olan
# tests/kabul/cekirdek ile tests/kabul/atis düzenekleri. Çekirdeğin uygulama kaynağı (core/src) ve
# birim testleri (core/test) kullanılmadı; çekirdeğe yalnız A4 portu üzerinden erişilir.
#
# CTest adları: kabul.kontrol.acil.<kısa ad>.
#
# Etiket ve seçenek:
#   - kabul_kontrol_beklenen: bugünkü (acil durdurma kuralı uygulanmamış) çekirdeğe karşı düşen
#     ("bekleniyor") testler. IDA_KABUL_KONTROL_BEKLENEN varsayılanı OFF: kapalıyken bu testler
#     DISABLED kaydedilir. Uygulama dalıyla (is/w5-kontrol-acil) birleşince seçenek ON yapılır ve
#     testler varsayılan turda koşar.
# Koşturma:
#   ctest --preset host-debug -R '^kabul\.kontrol\.'                       (bugün geçenler)
#   cmake --preset host-debug -DIDA_KABUL_KONTROL_BEKLENEN=ON            (beklenenler dahil)
#   ctest --preset host-debug -L kabul_kontrol_beklenen                         (beklenenler)

option(IDA_KABUL_KONTROL_BEKLENEN
  "Bugünkü çekirdeğe karşı düşen (bekleniyor) acil durdurma kontrol kabul testlerini varsayılan turda koştur" OFF)

ida_unity_kur()

add_library(kabul_kontrol_ortak STATIC kabul_kontrol.c)
target_include_directories(kabul_kontrol_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kabul_kontrol_ortak PUBLIC kabul_atis_ortak)
ida_ayar_uygula(kabul_kontrol_ortak CEKIRDEK)

foreach(_dosya acil ozellik)
  add_executable(kabul_kontrol_${_dosya} "kabul_kontrol_${_dosya}.c")
  target_link_libraries(kabul_kontrol_${_dosya} PRIVATE kabul_kontrol_ortak)
  ida_ayar_uygula(kabul_kontrol_${_dosya} CEKIRDEK)
endforeach()

# kabul_kontrol_testi(<dosya> <kısa ad> [BEKLENIYOR])
function(kabul_kontrol_testi dosya kisa)
  cmake_parse_arguments(PARSE_ARGV 2 A "BEKLENIYOR" "" "")
  set(_ad "kabul.kontrol.acil.${kisa}")
  add_test(NAME "${_ad}" COMMAND kabul_kontrol_${dosya} "${kisa}")
  if(A_BEKLENIYOR)
    set_tests_properties("${_ad}" PROPERTIES LABELS kabul_kontrol_beklenen)
    if(NOT IDA_KABUL_KONTROL_BEKLENEN)
      set_tests_properties("${_ad}" PROPERTIES DISABLED TRUE)
    endif()
  endif()
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/testler.cmake")
