# Kart yazılımı (fw kökü). Bu sürümde yalnız bağlanabilir en küçük yer tutucu vardır:
# başlangıç dosyası + boş main + bağlayıcı betiği (W2-depo §2). Kart destek katmanı, saat
# kurulumu ve sürücüler W3 kart yazılımı işinindir; ST kütüphaneleri firmware/third_party/
# altına o işte alınır.

include(IdaDenetim)

option(IDA_OLCUM "Ölçüm derlemesi (fw-measure): IDA_OLCUM=1 tanımı" OFF)

# Başlangıç: vektör tablosu, Reset_Handler. Platform kodu gnu17 (uzantı yalnız firmware/).
# Nesne kitaplığıdır: nesnesi bağlamaya her zaman girer (arşiv sırası sorunu olmaz).
add_library(ida_fw_baslangic OBJECT yer_tutucu/baslangic.c)
ida_ayar_uygula(ida_fw_baslangic)
set_target_properties(ida_fw_baslangic PROPERTIES C_EXTENSIONS ON)
target_compile_options(ida_fw_baslangic PRIVATE -fstack-usage)

# Uygulama nesneleri ayrı kitaplıkta: bellek ayırma denetimi bu arşivi tarar.
add_library(ida_fw_uygulama STATIC yer_tutucu/main.c)
ida_ayar_uygula(ida_fw_uygulama)
set_target_properties(ida_fw_uygulama PROPERTIES C_EXTENSIONS ON)
target_compile_options(ida_fw_uygulama PRIVATE -fstack-usage)
target_include_directories(ida_fw_uygulama PRIVATE "${PROJECT_SOURCE_DIR}/core/src")
target_link_libraries(ida_fw_uygulama PRIVATE ida_core)
if(IDA_OLCUM)
  target_compile_definitions(ida_fw_uygulama PRIVATE IDA_OLCUM=1)
endif()

# Kart imajı (yer tutucu).
add_executable(ida_fw)
target_link_libraries(ida_fw PRIVATE ida_fw_uygulama ida_core)
ida_fw_elf_kur(ida_fw)

# Birim testlerinin kartta bağlanması için test çıkışı (Unity -> firmware/test_destek).
add_library(ida_fw_test_destek STATIC test_destek/test_cikis.c)
ida_ayar_uygula(ida_fw_test_destek)
set_target_properties(ida_fw_test_destek PROPERTIES C_EXTENSIONS ON)

# ---- Denetimler (CTest; kartsız, PC'de koşar) -------------------------------------------

# Çekirdek ve uygulama nesnelerinde ayırma simgesine başvuru yok.
ida_bellek_denetimi_ekle(fw.bellek.cekirdek ida_core)
ida_bellek_denetimi_ekle(fw.bellek.uygulama ida_fw_uygulama)

# Deneme: malloc çağıran nesne denetimce yakalanır.
add_library(ida_fw_deneme_malloc_nesne STATIC deneme/malloc_cagiran.c)
target_compile_options(ida_fw_deneme_malloc_nesne PRIVATE -fno-builtin)
ida_bellek_denetimi_ekle(fw.bellek.deneme_yakalanir ida_fw_deneme_malloc_nesne
  BULGU_BEKLENIR malloc)

# Deneme: malloc çağıran imaj bağlanmaz (--wrap ile __wrap_malloc tanımsız).
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

# Yığın kullanımı raporu (.su dosyalarının birleşimi; sınır W3'te konur).
add_custom_target(ida_fw_yigin_raporu ALL
  COMMAND "${CMAKE_COMMAND}" "-DKOK=${CMAKE_BINARY_DIR}" "-DCIKTI=${CMAKE_BINARY_DIR}/yigin-kullanimi.txt"
          -P "${PROJECT_SOURCE_DIR}/cmake/yigin_raporu.cmake"
  DEPENDS ida_fw
  COMMENT "Yığın kullanımı raporu: yigin-kullanimi.txt"
  VERBATIM)
