# PC kökündeki bütünleme, duman ve depo denetimi testleri (CTest).
# Kabul testleri (tests/kabul/) ilgili işlerde test yazarınca eklenir.

include(IdaDenetim)

# ---- Bellek ayırma yasağı (tools/denetim/bellek-ayirma.sh) ------------------------------
ida_bellek_denetimi_ekle(host.bellek.cekirdek ida_core)

# Deneme: malloc çağıran nesne denetimce yakalanır.
add_library(ida_host_deneme_malloc_nesne STATIC
  "${PROJECT_SOURCE_DIR}/firmware/deneme/malloc_cagiran.c")
if(NOT MSVC)
  target_compile_options(ida_host_deneme_malloc_nesne PRIVATE -fno-builtin)
endif()
ida_bellek_denetimi_ekle(host.bellek.deneme_yakalanir ida_host_deneme_malloc_nesne
  BULGU_BEKLENIR malloc)

# ---- Çekirdek sınırı: stm32 başlığı eklemeye çalışan deneme hedefi derlenmez ------------
set(_deneme "${PROJECT_SOURCE_DIR}/tools/denetim/deneme/cekirdek-stm32")
foreach(_kip yol kaynak temiz)
  add_test(NAME host.cekirdek_siniri.${_kip}
    COMMAND "${CMAKE_COMMAND}" --fresh
            -S "${_deneme}" -B "${CMAKE_CURRENT_BINARY_DIR}/cekirdek-stm32-${_kip}"
            -G "${CMAKE_GENERATOR}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DIDA_KOK=${PROJECT_SOURCE_DIR}"
            "-DIDA_DENEME_KIPI=${_kip}")
endforeach()
set_tests_properties(host.cekirdek_siniri.yol host.cekirdek_siniri.kaynak PROPERTIES
  PASS_REGULAR_EXPRESSION "İDA çekirdek sınırı ihlali \\(ida_core\\)")
# Denetimin her şeyi reddetmediğini gösteren karşı örnek: temiz çekirdek yapılandırılır.
set_tests_properties(host.cekirdek_siniri.temiz PROPERTIES
  FAIL_REGULAR_EXPRESSION "İDA çekirdek sınırı ihlali")

# ---- vcpkg bağımlılık düzeneğinin dumanı (Eigen) ----------------------------------------
add_subdirectory(duman)
add_subdirectory(kabul/protokol)  # ida_link tel katmanı bağımsız kabul testleri (W2-protokol)
