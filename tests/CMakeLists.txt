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

# ---- Çekirdek sınırı (tools/denetim/deneme/cekirdek-stm32) -----------------------------
set(_deneme "${PROJECT_SOURCE_DIR}/tools/denetim/deneme/cekirdek-stm32")
# Yapılandırma katmanı: bozulmuş çekirdek yapılandırılamaz.
foreach(_kip yol kaynak uretilen genex)
  add_test(NAME host.cekirdek_siniri.${_kip}
    COMMAND "${CMAKE_COMMAND}" --fresh
            -S "${_deneme}" -B "${CMAKE_CURRENT_BINARY_DIR}/cekirdek-stm32-${_kip}"
            -G "${CMAKE_GENERATOR}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DIDA_KOK=${PROJECT_SOURCE_DIR}"
            "-DIDA_DENEME_KIPI=${_kip}")
  set_tests_properties(host.cekirdek_siniri.${_kip} PROPERTIES
    PASS_REGULAR_EXPRESSION "İDA çekirdek sınırı ihlali \\(ida_core\\)")
endforeach()
# Derleme sonrası katman: kaynak değişmeden başlığa eklenen yasak içerme (dolaylı) derlenir
# ama gerçek başlık bağımlılıkları denetimi düşer; temiz çekirdekte aynı denetim geçer.
foreach(_kip temiz dolayli_ad dolayli_kok)
  add_test(NAME host.cekirdek_siniri.${_kip}
    COMMAND "${CMAKE_COMMAND}"
            "-DIDA_KOK=${PROJECT_SOURCE_DIR}"
            "-DIKILI=${CMAKE_CURRENT_BINARY_DIR}/cekirdek-stm32-${_kip}"
            "-DKIP=${_kip}"
            "-DURETEC=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DC_DERLEYICI=${CMAKE_C_COMPILER}"
            -P "${_deneme}/calistir.cmake")
endforeach()
set_tests_properties(host.cekirdek_siniri.temiz PROPERTIES
  PASS_REGULAR_EXPRESSION "cekirdek-stm32 denemesi: başlık denetimi GEÇTİ"
  FAIL_REGULAR_EXPRESSION "İDA çekirdek sınırı ihlali")
set_tests_properties(host.cekirdek_siniri.dolayli_ad PROPERTIES
  PASS_REGULAR_EXPRESSION "yasak kimlikli başlık \\(cekirdek kökünde\\): [^\n]*stm32u5xx\\.h")
set_tests_properties(host.cekirdek_siniri.dolayli_kok PROPERTIES
  PASS_REGULAR_EXPRESSION "çekirdek ve sistem kökleri dışından başlık: [^\n]*platform_baslik\\.h")

# ---- cmake/ modülleri başka dizinlerden çağrılınca da çalışır ---------------------------
# (include_guard tuzağı): denetim işlevi ve MAVLink üretimi iki kardeş dizinden; boş betik
# yolu yapılandırma hatasıdır.
ida_pymavlink_kur()
set(_moduller "${PROJECT_SOURCE_DIR}/tools/denetim/deneme/cmake-moduller")
if(MSVC)
  set(_test_sayisi "")   # MSVC'de bellek denetimi testleri bilerek kurulmaz
else()
  set(_test_sayisi 2)    # a/ ve b/ dizinlerinin bellek denetimi testleri
endif()
add_test(NAME host.cmake_moduller.iki_dizin
  COMMAND "${CMAKE_COMMAND}"
          "-DTEST_SAYISI=${_test_sayisi}"
          "-DIDA_KOK=${PROJECT_SOURCE_DIR}"
          "-DIKILI=${CMAKE_CURRENT_BINARY_DIR}/cmake-moduller-iki-dizin"
          "-DURETEC=${CMAKE_GENERATOR}"
          "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
          "-DC_DERLEYICI=${CMAKE_C_COMPILER}"
          "-DPYMAVLINK_KAYNAK=${IDA_PYMAVLINK_KOK}"
          -P "${_moduller}/calistir.cmake")
set_tests_properties(host.cmake_moduller.iki_dizin PROPERTIES
  PASS_REGULAR_EXPRESSION "cmake-moduller denemesi: iki dizin geçti")
add_test(NAME host.cmake_moduller.bos_yol
  COMMAND "${CMAKE_COMMAND}" --fresh
          -S "${_moduller}" -B "${CMAKE_CURRENT_BINARY_DIR}/cmake-moduller-bos-yol"
          -G "${CMAKE_GENERATOR}"
          "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
          "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
          "-DIDA_KOK=${PROJECT_SOURCE_DIR}"
          "-DCPM_pymavlink_SOURCE=${IDA_PYMAVLINK_KOK}"
          -DIDA_DENEME_KIPI=bos_yol)
set_tests_properties(host.cmake_moduller.bos_yol PROPERTIES
  PASS_REGULAR_EXPRESSION "bulunamadı: ''")

# ---- vcpkg bağımlılık düzeneğinin dumanı (Eigen) ----------------------------------------
add_subdirectory(duman)
add_subdirectory(kabul/cekirdek)
add_subdirectory(kabul/protokol)  # ida_link tel katmanı bağımsız kabul testleri (W2-protokol)
add_subdirectory(kabul/gorev)     # W3-gorev bağımsız kabul testleri (kipler, silahlandırma, arıza güvenliği)
