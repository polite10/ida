# Kart yazılımının donanımdan bağımsız modüllerinin birim testleri (W3-kart; Unity, CTest).
# Yalnız PC kökünde derlenir (tests/CMakeLists.txt içerir); firmware/ dizininin kendisi yalnız
# fw kökünde yapılandırılır. Aynı kaynaklar kartta ida_fw_saf kitaplığındadır.

ida_unity_kur()

set(_fw "${PROJECT_SOURCE_DIR}/firmware")
add_library(ida_fw_saf_host STATIC
  "${_fw}/link/alim.c"
  "${_fw}/bsp/acilis_kaydi.c"
  "${_fw}/app/istatistik.c"
  "${_fw}/app/tanilama.c"
  "${_fw}/app/saglik.c")
target_include_directories(ida_fw_saf_host PUBLIC "${_fw}/link" "${_fw}/bsp" "${_fw}/app")
target_link_libraries(ida_fw_saf_host PUBLIC ida_link ida_core)
ida_ayar_uygula(ida_fw_saf_host CEKIRDEK)

function(_ida_fw_birim ad)
  add_executable(ida_fw_test_${ad} test_${ad}.c)
  target_link_libraries(ida_fw_test_${ad} PRIVATE ida_fw_saf_host ida_link ida_mavlink_uretilen
                        ida_unity)
  ida_ayar_uygula(ida_fw_test_${ad} CEKIRDEK)
  add_test(NAME firmware.birim.${ad} COMMAND ida_fw_test_${ad})
endfunction()

_ida_fw_birim(alim)
_ida_fw_birim(acilis)
_ida_fw_birim(istatistik)
_ida_fw_birim(tanilama)
_ida_fw_birim(saglik)

# Ölçüm bloğu okuyucusunun tutarlılık denetimi (Python, kart gerekmez).
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME firmware.araclar.olcum_oku
  COMMAND "${Python3_EXECUTABLE}" -I "${_fw}/araclar/test_olcum_oku.py")
