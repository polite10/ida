# Kart yazılımı (fw kökü; W3-kart dilim 1): kart destek katmanı (bsp/), A1 bağlantı sürücüsü
# (link/), ana döngü ve çağırıcı (app/). ST ve Arm kaynakları third_party/st.cmake ile alınır.
#
# Hedefler:
#   ida_fw_baslangic   başlangıç kodu ve vektör tablosu (nesne kitaplığı; bütün kart imajları,
#                      kartta bağlanan birim test imajları dahil)
#   ida_fw_saf         donanımdan bağımsız modüller (ISO C17; PC'de firmware/test/ sınar)
#   ida_fw_uygulama    BSP, sürücü, uygulama (gnu17; bellek ayırma denetimi bu arşivi tarar)
#   ida_fw             kart imajı (ida_fw.elf; harita, boyut ve yığın raporu)
#
# Ön ayarlar: fw-debug (-Og, GCC -fanalyzer, IWDG hata ayıklamada donar), fw-release (-O2), fw-measure (-O2,
# IDA_OLCUM=1: SWD ile tetiklenen gözetim ve hata sınamaları).

include(IdaDenetim)
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/st.cmake")

option(IDA_OLCUM "Ölçüm derlemesi (fw-measure): IDA_OLCUM=1 tanımı" OFF)

ida_fw_st_kur()

# Kaynak deposu commit özetinin ilk 32 biti (IDA_BOARD_HELLO.fw_git); yapılandırma anındaki
# HEAD. Git yoksa 0.
set(IDA_FW_GIT "0")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse --short=8 HEAD
    OUTPUT_VARIABLE _ida_git OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _ida_git_rc
    ERROR_QUIET)
  if(_ida_git_rc EQUAL 0 AND _ida_git MATCHES "^[0-9a-f]+$")
    set(IDA_FW_GIT "0x${_ida_git}")
  endif()
endif()

# Firmware hedeflerinin ortak ayarları (İDA uyarı politikası + gölgeleme ve yığın sınırı).
function(_ida_fw_ayar hedef)
  ida_ayar_uygula(${hedef})
  set_target_properties(${hedef} PROPERTIES C_EXTENSIONS ON)
  target_compile_options(${hedef} PRIVATE -fstack-usage -Wshadow -Wstack-usage=1024)
endfunction()

# ---- Başlangıç kodu ----------------------------------------------------------------------
add_library(ida_fw_baslangic OBJECT bsp/baslangic.c)
_ida_fw_ayar(ida_fw_baslangic)
# Aygıt başlıkları yalnız derleme için (bağlama bağımlılığı eklenmez: test imajları da bağlar).
target_include_directories(ida_fw_baslangic SYSTEM PRIVATE
  "$<TARGET_PROPERTY:ida_fw_hal,INTERFACE_INCLUDE_DIRECTORIES>")
target_compile_definitions(ida_fw_baslangic PRIVATE
  "$<TARGET_PROPERTY:ida_fw_hal,INTERFACE_COMPILE_DEFINITIONS>")

# ---- Donanımdan bağımsız modüller --------------------------------------------------------
add_library(ida_fw_saf STATIC
  link/alim.c
  bsp/acilis_kaydi.c
  app/istatistik.c
  app/saglik.c
  app/gonderim.c
  app/kalici.c)
ida_ayar_uygula(ida_fw_saf CEKIRDEK)
target_compile_options(ida_fw_saf PRIVATE -fstack-usage -Wshadow)
target_include_directories(ida_fw_saf PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/link" "${CMAKE_CURRENT_SOURCE_DIR}/bsp"
  "${CMAKE_CURRENT_SOURCE_DIR}/app")
target_link_libraries(ida_fw_saf PUBLIC ida_link ida_core)
target_compile_options(ida_fw_saf PRIVATE -Wimplicit-fallthrough)

# ---- BSP, bağlantı sürücüsü, uygulama ----------------------------------------------------
add_library(ida_fw_uygulama STATIC
  bsp/bsp.c
  bsp/zaman.c
  bsp/hata.c
  bsp/flash.c
  link/seri.c
  app/cagirici.c
  app/main.c)
_ida_fw_ayar(ida_fw_uygulama)
target_link_libraries(ida_fw_uygulama PRIVATE ida_fw_saf ida_core ida_link_ust ida_link ida_fw_hal)
target_compile_definitions(ida_fw_uygulama PRIVATE "IDA_FW_GIT=${IDA_FW_GIT}u")
if(IDA_OLCUM)
  target_compile_definitions(ida_fw_uygulama PRIVATE IDA_OLCUM=1)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # Hata ayıklama derlemesi: çekirdek hata ayıklayıcıda durunca IWDG de durur (01 §2.2 md.4).
  target_compile_definitions(ida_fw_uygulama PRIVATE IDA_FW_IWDG_DONDUR=1)
endif()

# ---- Kart imajı --------------------------------------------------------------------------
add_executable(ida_fw)
# Uygulama arşivinin bütün nesneleri bağlanır: kesme ve hata işleyicilerinin güçlü tanımları
# başlangıç kodundaki zayıf tanımların yerini almalıdır (arşivden yalnız zayıf simge için nesne
# çekilmez).
target_link_options(ida_fw PRIVATE
  "LINKER:--whole-archive" "$<TARGET_FILE:ida_fw_uygulama>" "LINKER:--no-whole-archive")
target_link_libraries(ida_fw PRIVATE ida_fw_uygulama ida_fw_saf ida_link_ust ida_link ida_core ida_fw_hal)
set(IDA_FW_BAGLAYICI_BETIGI "${CMAKE_CURRENT_SOURCE_DIR}/bsp/stm32u5a9_ida.ld")
ida_fw_elf_kur(ida_fw)

# Birim testlerinin kartta bağlanması için test çıkışı (Unity -> firmware/test_destek).
add_library(ida_fw_test_destek STATIC test_destek/test_cikis.c)
ida_ayar_uygula(ida_fw_test_destek)
set_target_properties(ida_fw_test_destek PROPERTIES C_EXTENSIONS ON)

# ---- Denetimler (CTest; kartsız, PC'de koşar) -------------------------------------------

# Çekirdek ve firmware nesnelerinde ayırma simgesine başvuru yok.
ida_bellek_denetimi_ekle(fw.bellek.cekirdek ida_core)
ida_bellek_denetimi_ekle(fw.bellek.uygulama ida_fw_uygulama)
ida_bellek_denetimi_ekle(fw.bellek.saf ida_fw_saf)
ida_bellek_denetimi_ekle(fw.bellek.hal ida_fw_hal)

# Deneme: malloc çağıran nesne denetimce yakalanır.
add_library(ida_fw_deneme_malloc_nesne STATIC deneme/malloc_cagiran.c)
target_compile_options(ida_fw_deneme_malloc_nesne PRIVATE -fno-builtin)
ida_bellek_denetimi_ekle(fw.bellek.deneme_yakalanir ida_fw_deneme_malloc_nesne
  BULGU_BEKLENIR malloc)

# Deneme: malloc çağıran imaj bağlanmaz (--wrap ile __wrap_malloc tanımsız). W2'nin yer tutucu
# bağlayıcı betiğiyle bağlanır (ida_fw_elf_kur'un varsayılanı).
unset(IDA_FW_BAGLAYICI_BETIGI)
add_executable(ida_fw_deneme_malloc EXCLUDE_FROM_ALL deneme/malloc_main.c)
target_link_libraries(ida_fw_deneme_malloc PRIVATE ida_fw_deneme_malloc_nesne)
ida_fw_elf_kur(ida_fw_deneme_malloc)
add_test(NAME fw.bellek.deneme_baglanmaz
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ida_fw_deneme_malloc)
set_tests_properties(fw.bellek.deneme_baglanmaz PROPERTIES
  PASS_REGULAR_EXPRESSION "undefined reference to `__wrap_malloc'")

# Bağlanmış imajda ayırma simgesi (tanımlı ya da tanımsız) yok.
add_test(NAME fw.bellek.imaj
  COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DELF=$<TARGET_FILE:ida_fw>"
          -P "${PROJECT_SOURCE_DIR}/cmake/imaj_ayirma_denetimi.cmake")
# Karşı örnek: aynı denetim malloc'a başvuran deneme arşivini reddeder.
add_test(NAME fw.bellek.imaj_denetimi_yakalar
  COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}"
          "-DELF=$<TARGET_FILE:ida_fw_deneme_malloc_nesne>"
          -P "${PROJECT_SOURCE_DIR}/cmake/imaj_ayirma_denetimi.cmake")
set_tests_properties(fw.bellek.imaj_denetimi_yakalar PROPERTIES
  PASS_REGULAR_EXPRESSION "İmajda bellek ayırma simgesi var")

# Yığın kullanımı raporu (.su dosyalarının birleşimi). Sınır: firmware hedeflerinde işlev başına
# -Wstack-usage=1024; ana yığın 16 KB (bağlayıcı betiği), MSPLIM ile korunur.
add_custom_target(ida_fw_yigin_raporu ALL
  COMMAND "${CMAKE_COMMAND}" "-DKOK=${CMAKE_BINARY_DIR}" "-DCIKTI=${CMAKE_BINARY_DIR}/yigin-kullanimi.txt"
          -P "${PROJECT_SOURCE_DIR}/cmake/yigin_raporu.cmake"
  DEPENDS ida_fw
  COMMENT "Yığın kullanımı raporu: yigin-kullanimi.txt"
  VERBATIM)
