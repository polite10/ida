# core/test/atis — atış kontrol emniyet zinciri birim testleri (W5-atis-emniyet).
# core/test/CMakeLists.txt bu dosyayı ida_core_birim_testi tanımlandıktan sonra içerir.
#
#   atis_port     port yolu (ida_core_step/tick): silahlandırma gerektirmeyen G-Ç maddeleri
#   atis_durum    düzenek yolu (atis_duzenek.h): silahlı yollar
#   atis_ozellik  düzenek ve port yolu: yedi emniyet değişmezi, rastgele diziler (sabit tohum)

add_library(ida_core_test_atis_duzenek STATIC "${CMAKE_CURRENT_LIST_DIR}/atis_duzenek.c")
target_link_libraries(ida_core_test_atis_duzenek PUBLIC ida_core_test_destek ida_core ida_unity)
ida_ayar_uygula(ida_core_test_atis_duzenek CEKIRDEK)

foreach(_ad IN ITEMS port durum ozellik)
  ida_core_birim_testi(atis_${_ad} "${CMAKE_CURRENT_LIST_DIR}/test_atis_${_ad}.c")
  target_link_libraries(ida_core_test_atis_${_ad} PRIVATE ida_core_test_atis_duzenek)
endforeach()
