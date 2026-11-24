# tests/kabul/atis — atış kontrolün emniyet zincirinin bağımsız kabul testleri (W5-atis-emniyet;
# yazan ida-test-yazari).
#
# Testler yalnız şunlardan türetildi: mimari Bölüm 03 sürüm 0.3'ün "Ek: uygulama gereksinimleri
# (emniyet zinciri)" (G-Ç01–G-Ç53) ve §7 arıza tablosu (F1, F3, F7, F8, F10–F15),
# docs/gorevler/W5-ortak.md (W5-K1…K11), A4 sözleşmesi (docs/sozlesmeler/A4-port.md 1.0.1,
# core/include/ida/port.h) ve tests/kabul/cekirdek düzeneği. Çekirdeğin uygulama kaynağı (core/src)
# ve birim testleri (core/test) kullanılmadı. Çekirdeğe yalnız A4 portu üzerinden erişilir.
#
# Kapsam sınırı (kullanıcı kararı): yalnız emniyet zinciri. Hedefleme, atış ölçütü, hedef
# algılama/takip ve torpido başarımı için test, senaryo ya da veri yoktur.
#
# CTest adları: kabul.atis.<madde>.<kısa ad> (madde: G-Çnn, Fnn ya da "ozellik").
#
# "Bekleniyor" testleri: bugünkü yer tutucu atış kontrol modülüne (durum hep SAFE) karşı düşen
# testler LABELS kabul_atis ile işaretlidir. IDA_KABUL_ATIS_BEKLENEN kapalıyken (bu dalın
# varsayılanı; CI'ı kırmamak için) bu testler DISABLED olarak kaydedilir ve varsayılan test
# turunda koşmaz; uygulama dalı (W5-atis-emniyet) birleşince seçenek açılır (varsayılanı ON
# yapılır) ve hepsi varsayılan tura girer. Tek komutla koşturma:
#   cmake --preset host-debug -DIDA_KABUL_ATIS_BEKLENEN=ON
#   cmake --build build/host-debug
#   ctest --preset host-debug -R '^kabul\.atis\.'
# Yalnız bekleniyor listesi: ctest --preset host-debug -L kabul_atis

option(IDA_KABUL_ATIS_BEKLENEN
  "Yer tutucu atış kontrole karşı düşen (bekleniyor) atış kabul testlerini varsayılan turda koştur" OFF)

ida_unity_kur()

add_library(kabul_atis_ortak STATIC kabul_atis.c)
target_include_directories(kabul_atis_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(kabul_atis_ortak PUBLIC kabul_cekirdek_ortak ida_core ida_unity)
ida_ayar_uygula(kabul_atis_ortak CEKIRDEK)

foreach(_dosya temel komut emniyet kilit izin ozellik)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kabul_atis_${_dosya}.c")
    continue()
  endif()
  add_executable(kabul_atis_${_dosya} "kabul_atis_${_dosya}.c")
  target_link_libraries(kabul_atis_${_dosya} PRIVATE kabul_atis_ortak)
  ida_ayar_uygula(kabul_atis_${_dosya} CEKIRDEK)
endforeach()

# kabul_atis_testi(<dosya> <madde> <kısa ad> [BEKLENIYOR])
function(kabul_atis_testi dosya madde kisa)
  cmake_parse_arguments(PARSE_ARGV 3 A "BEKLENIYOR" "" "")
  set(_ad "kabul.atis.${madde}.${kisa}")
  add_test(NAME "${_ad}" COMMAND kabul_atis_${dosya} "${kisa}")
  if(A_BEKLENIYOR)
    set_tests_properties("${_ad}" PROPERTIES LABELS kabul_atis)
    if(NOT IDA_KABUL_ATIS_BEKLENEN)
      set_tests_properties("${_ad}" PROPERTIES DISABLED TRUE)
    endif()
  endif()
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/testler.cmake")
