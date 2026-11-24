# ida_link üst katmanı birim testleri (W3-oturum; Unity, CTest). PC'de koşar (host-debug,
# host-asan); firmware kökünde aynı kaynaklar arm-none-eabi ile bağlanabilir ELF olarak
# derlenir (kartta koşturma bu işte yok). Bağımsız kabul testleri tests/kabul/oturum/
# altındadır (ayrı iş).

ida_unity_kur()

add_library(ida_link_test_oturum_destek STATIC destek_oturum.c)
target_include_directories(ida_link_test_oturum_destek PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
target_link_libraries(ida_link_test_oturum_destek PUBLIC
  ida_link_ust ida_link ida_link_altin_veri ida_mavlink_uretilen ida_unity)
ida_ayar_uygula(ida_link_test_oturum_destek CEKIRDEK)

set(_oturum_testleri oturum parti komut cikti esleme dayaniklilik uctan_uca sol crc)
foreach(_ad IN LISTS _oturum_testleri)
  set(_hedef ida_link_test_ust_${_ad})
  add_executable(${_hedef} test_${_ad}.c)
  target_link_libraries(${_hedef} PRIVATE ida_link_test_oturum_destek)
  if(_ad STREQUAL "uctan_uca" OR _ad STREQUAL "sol")
    target_link_libraries(${_hedef} PRIVATE ida_core)
  endif()
  ida_ayar_uygula(${_hedef} CEKIRDEK)
  if(CMAKE_CROSSCOMPILING)
    ida_fw_elf_kur(${_hedef})
    # Unity'nin çıkış işlevi ida_fw_test_destek'te: bağlama sırası Unity'den sonra.
    target_link_libraries(${_hedef} PRIVATE ida_unity ida_fw_test_destek)
  else()
    add_test(NAME protokol.birim.ust_${_ad} COMMAND ${_hedef})
  endif()
endforeach()

# W4-param-hiz: ust_crc32'nin tablosu çekirdeğin ida_crc32 tablosuyla aynı mı (yalnız bu test
# çekirdeğe ve onun iç başlığına bağlanır; ida_link_ust bağlanmaz).
target_link_libraries(ida_link_test_ust_crc PRIVATE ida_core)
target_include_directories(ida_link_test_ust_crc PRIVATE "${PROJECT_SOURCE_DIR}/core/src")
