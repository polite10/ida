# ida_link birim testleri (W2-protokol §4; Unity, CTest). PC'de koşar (host-debug,
# host-asan); firmware kökünde aynı kaynak arm-none-eabi ile bağlanabilir ELF olarak
# derlenir, kartta koşturma bu işte yok. Bağımsız kabul testleri tests/kabul/protokol/
# altındadır (ayrı iş).

ida_unity_kur()

add_executable(ida_link_test_tel test_tel.c)
target_link_libraries(ida_link_test_tel PRIVATE
  ida_link ida_link_altin_veri ida_mavlink_uretilen ida_unity)
add_dependencies(ida_link_test_tel ida_link_uret)
ida_ayar_uygula(ida_link_test_tel CEKIRDEK)

if(CMAKE_CROSSCOMPILING)
  ida_fw_elf_kur(ida_link_test_tel)
  target_link_libraries(ida_link_test_tel PRIVATE ida_fw_test_destek)
  # Bağlanmış test imajında bellek ayırma simgesi yok (kabul md. 1; --wrap ayrıca bağlamayı
  # durdururdu).
  add_test(NAME protokol.fw.bellek.test_imaji
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DELF=$<TARGET_FILE:ida_link_test_tel>"
            -P "${PROJECT_SOURCE_DIR}/cmake/imaj_ayirma_denetimi.cmake")
else()
  add_test(NAME protokol.birim.tel COMMAND ida_link_test_tel)
endif()
