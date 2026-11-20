# Çekirdek birim testleri (T1; Unity). PC'de CTest ile koşar; firmware kökünde aynı test
# arm-none-eabi ile bağlanabilir bir ELF olarak derlenir, kartta koşturma bu işte yok.

ida_unity_kur()

add_executable(ida_core_test_iskelet test_iskelet.c)
target_include_directories(ida_core_test_iskelet PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(ida_core_test_iskelet PRIVATE ida_core ida_unity)
ida_ayar_uygula(ida_core_test_iskelet CEKIRDEK)

if(CMAKE_CROSSCOMPILING)
  # Kartta: firmware'in başlangıç dosyası, bağlayıcı betiği ve test çıkışı (firmware/).
  ida_fw_elf_kur(ida_core_test_iskelet)
  target_link_libraries(ida_core_test_iskelet PRIVATE ida_fw_test_destek)
else()
  add_test(NAME cekirdek.birim.iskelet COMMAND ida_core_test_iskelet)
endif()
