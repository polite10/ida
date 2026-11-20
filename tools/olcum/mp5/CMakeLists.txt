# W0-MP5a ölçüm prototipi (atılabilir; ürün koduna taşınmaz, 09 §2.1).
#   MP5_HEDEF=pc : akış üreteci ve host referansı (mp5_uret, C17) ve seri kayıt aracı (mp5_kayit, C++20)
#   MP5_HEDEF=fw : kart ölçüm yazılımı
#
# Ürün kitaplığı (protocol/ida_link) bu prototipte DERLENMEZ: depo kökünün fw-release ön ayarıyla
# derlenmiş libida_link.a'sı bağlanır (IDA_FW_DERLEME). Üretilen MAVLink ve ida_link başlıkları da
# aynı derleme dizinindendir. MP5_KAYNAK=kopya yalnız iyileştirme denemeleri içindir (kopya/).
cmake_minimum_required(VERSION 3.20)

set(MP5_HEDEF "fw" CACHE STRING "fw, pc ya da test")
# Ürün kaynağının kökü: varsayılan bu çalışma kopyası; ortam değişkeni MP5_URUN_KOK ile başka bir
# çalışma kopyası (ör. ürünün güncel dalının geçici çalışma ağacı) verilebilir.
if(DEFINED ENV{MP5_URUN_KOK} AND NOT "$ENV{MP5_URUN_KOK}" STREQUAL "")
  set(_kok_varsayilan "$ENV{MP5_URUN_KOK}")
else()
  get_filename_component(_kok_varsayilan "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()
set(IDA_KOK "${_kok_varsayilan}" CACHE PATH "ürün kaynağının kökü (protocol/ida_link)")
set(IDA_FW_DERLEME "${IDA_KOK}/build/fw-release" CACHE PATH "ürün kökünün fw-release derleme dizini")
set(IDA_HOST_DERLEME "${IDA_KOK}/build/host-debug" CACHE PATH "ürün kökünün host-debug derleme dizini (test kipi)")
set(MP5_URETILEN "${CMAKE_CURRENT_SOURCE_DIR}/derleme/uretilen" CACHE PATH "mp5_uret çıktı dizini")

# Üretilmiş başlıkların yeri iki düzende de desteklenir: 5b34de8 (uretilen/acik, uretilen/ozel)
# ve 0fa569e (protocol/uretilen/ida_link/...). Var olan dizinler içerme yoluna girer.
set(IDA_MAVLINK_URETILEN "${IDA_FW_DERLEME}/protocol/uretilen")
set(IDA_LINK_OZEL "${IDA_FW_DERLEME}/protocol/ida_link/uretilen/ozel")
set(IDA_LINK_ICERME "${IDA_KOK}/protocol/ida_link/include")
foreach(_d "${IDA_FW_DERLEME}/protocol/ida_link/uretilen/acik" "${IDA_LINK_OZEL}")
  if(EXISTS "${_d}")
    list(APPEND IDA_LINK_ICERME "${_d}")
  endif()
endforeach()
foreach(_d "${IDA_LINK_OZEL}/ida_link_altin_veri.c" "${IDA_MAVLINK_URETILEN}/mavlink/ida/mavlink.h")
  if(NOT EXISTS "${_d}")
    message(FATAL_ERROR "Yok: ${_d} (önce ürün kökünde: cmake --preset fw-release && cmake --build --preset fw-release)")
  endif()
endforeach()

set(MP5_KOPYA_DOSYA "kopya/tel_kopya.c" CACHE STRING "MP5_KAYNAK=kopya iken derlenecek tel.c kopyası")
set(MP5_KOPYA_TANIM "" CACHE STRING "kopyanın derleme tanımları (ör. MP5_K=1;MP5_CRC=256)")

if(MP5_HEDEF STREQUAL "pc")
  project(mp5_pc LANGUAGES C CXX)
  # Host referansı: ürün kitaplığının kaynağı host derleyicisiyle (ürün kaynağı değişmeden).
  add_executable(mp5_uret pc/mp5_uret.c "${IDA_KOK}/protocol/ida_link/src/tel.c"
                 "${IDA_LINK_OZEL}/ida_link_altin_veri.c")
  set_target_properties(mp5_uret PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  target_include_directories(mp5_uret PRIVATE ${IDA_LINK_ICERME})
  target_include_directories(mp5_uret SYSTEM PRIVATE "${IDA_MAVLINK_URETILEN}")
  target_compile_options(mp5_uret PRIVATE -Wall -Wextra -Wconversion -Werror)
  add_executable(mp5_kayit pc/mp5_kayit.cpp)
  set_target_properties(mp5_kayit PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  target_compile_options(mp5_kayit PRIVATE -Wall -Wextra -Wconversion -Werror)
  return()
endif()

if(MP5_HEDEF STREQUAL "test")
  # Davranış eşitliği: ürünün birim testleri (protocol/ida_link/test/test_tel.c) ve bağımsız kabul
  # testleri (tests/kabul/protokol/) ürün kaynağı yerine kopya bağlanarak host'ta derlenir ve koşar.
  # Test dosyalarına dokunulmaz; üretilmiş girdileri ürün kökünün host-debug derlemesinden alınır.
  # MP5_TEST_CESITLERI: "ad=kaynak|tanımlar" listesi; ad "urun" ise ürün kaynağının kendisi.
  project(mp5_test LANGUAGES C)
  enable_testing()
  set(MP5_TEST_CESITLERI "urun=${IDA_KOK}/protocol/ida_link/src/tel.c|" CACHE STRING "test çeşitleri")
  set(_hu "${IDA_HOST_DERLEME}/protocol/uretilen")
  set(_ho "${IDA_HOST_DERLEME}/protocol/ida_link/uretilen/ozel")
  set(_ku "${IDA_HOST_DERLEME}/tests/kabul/protokol/uretilen")
  set(_unity "${IDA_HOST_DERLEME}/_deps/unity-src/src")
  set(_kd "${IDA_KOK}/tests/kabul/protokol")
  foreach(_d "${_hu}/ida_link/ozel/tablo_uretilen.h" "${_ho}/ida_link_altin_veri.c" "${_ku}/kabul_ek_b.c"
             "${_unity}/unity.c")
    if(NOT EXISTS "${_d}")
      message(FATAL_ERROR "Yok: ${_d} (önce ürün kökünde: cmake --preset host-debug && cmake --build --preset host-debug)")
    endif()
  endforeach()
  set(_uyari -std=c17 -pedantic-errors -Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Werror
      -ffp-contract=off -fno-fast-math)
  add_library(mp5t_unity STATIC "${_unity}/unity.c")
  target_include_directories(mp5t_unity SYSTEM PUBLIC "${_unity}")
  target_compile_options(mp5t_unity PRIVATE -w)
  set(_kabuller k02 k04 k05 k06 k07 k08 sayac ek_b saglamlik k101 belirsiz)
  foreach(_c IN LISTS MP5_TEST_CESITLERI)
    string(REGEX MATCH "^([^=]+)=([^|]+)\\|(.*)$" _ok "${_c}")
    if(NOT _ok)
      message(FATAL_ERROR "test çeşidi biçimi: ad=kaynak|tanımlar (${_c})")
    endif()
    set(_ad "${CMAKE_MATCH_1}")
    set(_kaynak "${CMAKE_MATCH_2}")
    string(REPLACE "," ";" _tanim "${CMAKE_MATCH_3}")
    if(NOT IS_ABSOLUTE "${_kaynak}")
      set(_kaynak "${CMAKE_CURRENT_SOURCE_DIR}/${_kaynak}")
    endif()
    add_library(mp5t_tel_${_ad} STATIC "${_kaynak}")
    target_include_directories(mp5t_tel_${_ad} PUBLIC "${IDA_KOK}/protocol/ida_link/include")
    target_include_directories(mp5t_tel_${_ad} SYSTEM PUBLIC "${_hu}")
    target_compile_definitions(mp5t_tel_${_ad} PRIVATE ${_tanim})
    target_compile_options(mp5t_tel_${_ad} PRIVATE ${_uyari})
    add_executable(mp5t_birim_${_ad} "${IDA_KOK}/protocol/ida_link/test/test_tel.c" "${_ho}/ida_link_altin_veri.c")
    target_include_directories(mp5t_birim_${_ad} PRIVATE "${_ho}")
    target_link_libraries(mp5t_birim_${_ad} PRIVATE mp5t_tel_${_ad} mp5t_unity)
    target_compile_options(mp5t_birim_${_ad} PRIVATE ${_uyari})
    add_test(NAME ${_ad}.protokol.birim.tel COMMAND mp5t_birim_${_ad})
    add_library(mp5t_kabul_ortak_${_ad} STATIC "${_kd}/kabul_ortak.c" "${_ku}/kabul_ek_b.c")
    target_include_directories(mp5t_kabul_ortak_${_ad} PUBLIC "${_kd}" "${_ku}")
    target_link_libraries(mp5t_kabul_ortak_${_ad} PUBLIC mp5t_tel_${_ad} mp5t_unity)
    target_compile_options(mp5t_kabul_ortak_${_ad} PRIVATE ${_uyari})
    foreach(_k IN LISTS _kabuller)
      add_executable(mp5t_kabul_${_k}_${_ad} "${_kd}/kabul_${_k}.c")
      target_link_libraries(mp5t_kabul_${_k}_${_ad} PRIVATE mp5t_kabul_ortak_${_ad})
      target_compile_options(mp5t_kabul_${_k}_${_ad} PRIVATE ${_uyari})
      add_test(NAME ${_ad}.kabul.protokol.${_k} COMMAND mp5t_kabul_${_k}_${_ad})
    endforeach()
    # Farksal sınama: ürün kaynağı ile bu çeşit aynı programda (çeşidin açık işlevleri v_ önekli).
    if(NOT _ad STREQUAL "urun")
      set(_yeniden ida_link_alici_kur ida_link_al ida_link_alici_tasma ida_link_gonderici_kur ida_link_gonder
          ida_link_coz ida_link_mesaj_bilgisi)
      add_library(mp5t_vtel_${_ad} STATIC "${_kaynak}")
      target_include_directories(mp5t_vtel_${_ad} PUBLIC "${IDA_KOK}/protocol/ida_link/include")
      target_include_directories(mp5t_vtel_${_ad} SYSTEM PUBLIC "${_hu}")
      set(_ad_tanim ${_tanim})
      foreach(_f IN LISTS _yeniden)
        list(APPEND _ad_tanim "${_f}=v_${_f}")
      endforeach()
      target_compile_definitions(mp5t_vtel_${_ad} PRIVATE ${_ad_tanim})
      target_compile_options(mp5t_vtel_${_ad} PRIVATE ${_uyari})
      set(_fark_k 0)
      if(_tanim MATCHES "MP5_K=1")
        set(_fark_k 1)
      endif()
      add_executable(mp5t_fark_${_ad} pc/mp5_fark_test.c "${IDA_KOK}/protocol/ida_link/src/tel.c")
      target_include_directories(mp5t_fark_${_ad} PRIVATE "${IDA_KOK}/protocol/ida_link/include")
      target_include_directories(mp5t_fark_${_ad} SYSTEM PRIVATE "${_hu}")
      target_compile_definitions(mp5t_fark_${_ad} PRIVATE FARK_K=${_fark_k})
      target_compile_options(mp5t_fark_${_ad} PRIVATE ${_uyari})
      target_link_libraries(mp5t_fark_${_ad} PRIVATE mp5t_vtel_${_ad})
      add_test(NAME ${_ad}.fark COMMAND mp5t_fark_${_ad} 20000)
      if(_fark_k)
        # Duyarlılık denemesi: aynı çeşit yükün tamamı (255 B) karşılaştırılarak sınanınca farkı
        # bulmalıdır (sınamanın bilinen tek farkı yakaladığının kanıtı).
        add_executable(mp5t_fark_tam_${_ad} pc/mp5_fark_test.c "${IDA_KOK}/protocol/ida_link/src/tel.c")
        target_include_directories(mp5t_fark_tam_${_ad} PRIVATE "${IDA_KOK}/protocol/ida_link/include")
        target_include_directories(mp5t_fark_tam_${_ad} SYSTEM PRIVATE "${_hu}")
        target_compile_definitions(mp5t_fark_tam_${_ad} PRIVATE FARK_K=0)
        target_compile_options(mp5t_fark_tam_${_ad} PRIVATE ${_uyari})
        target_link_libraries(mp5t_fark_tam_${_ad} PRIVATE mp5t_vtel_${_ad})
        add_test(NAME ${_ad}.fark.deneme_yuk_tamami_farkli COMMAND mp5t_fark_tam_${_ad} 2000)
        set_tests_properties(${_ad}.fark.deneme_yuk_tamami_farkli PROPERTIES WILL_FAIL TRUE)
      endif()
    endif()
  endforeach()
  return()
endif()

project(mp5_fw LANGUAGES C ASM)

if(NOT MP5_SRC)
  if(DEFINED ENV{IDA_CACHE} AND NOT "$ENV{IDA_CACHE}" STREQUAL "")
    set(MP5_SRC "$ENV{IDA_CACHE}/src")
  else()
    set(MP5_SRC "$ENV{HOME}/.cache/ida/src")
  endif()
endif()
set(MP5_SRC "${MP5_SRC}" CACHE PATH "ST kaynaklarının kökü (MP1 kaynak-indir.sh)")
set(MP5_DERLEME "?" CACHE STRING "derleme etiketi")
set(MP5_KIMLIK "yok" CACHE STRING "derleme kimliği (derle.sh üretir; seri çıktıyı imaja bağlar)")
set(MP5_YINELEME 10000 CACHE STRING "seri başına yineleme")
set(MP5_ONBELLEK 7 CACHE STRING "önbellek kipleri maskesi: 1 sicak, 2 soguk, 4 kapali")
set(MP5_KAYNAK "urun" CACHE STRING "urun: depo derlemesinin libida_link.a'sı; kopya: kopya/ altındaki deneme kaynağı")
set(MP5_KOPYA_OPT "-O2" CACHE STRING "MP5_KAYNAK=kopya iken kopyanın eniyileme düzeyi")

if(NOT EXISTS "${MP5_URETILEN}/mp5_akislar.c")
  message(FATAL_ERROR "Yok: ${MP5_URETILEN}/mp5_akislar.c (derle.sh önce mp5_uret'i koşar)")
endif()

set(HAL_DIR  "${MP5_SRC}/stm32u5xx-hal-driver")
set(DEV_DIR  "${MP5_SRC}/cmsis-device-u5")
set(CORE_INC "${MP5_SRC}/STM32CubeU5/Drivers/CMSIS/Core/Include")
foreach(_d "${HAL_DIR}/Src" "${DEV_DIR}/Include" "${CORE_INC}")
  if(NOT EXISTS "${_d}")
    message(FATAL_ERROR "Kaynak yok: ${_d}  (MP1 kaynak-indir.sh; tools/olcum/mp1, main)")
  endif()
endforeach()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(MP5_FP -ffp-contract=off -fno-fast-math)

set(MP5_HAL_MODULLER HAL CORTEX RCC GPIO PWR FLASH UART DMA ICACHE EXTI)
file(READ "${HAL_DIR}/Inc/stm32u5xx_hal_conf_template.h" _conf)
string(REGEX REPLACE "#define (HAL_[A-Z0-9]+_MODULE_ENABLED)" "/* #define \\1 */" _conf "${_conf}")
string(REPLACE "/* #define HAL_MODULE_ENABLED */" "#define HAL_MODULE_ENABLED" _conf "${_conf}")
foreach(_m ${MP5_HAL_MODULLER})
  string(REPLACE "/* #define HAL_${_m}_MODULE_ENABLED */" "#define HAL_${_m}_MODULE_ENABLED" _conf "${_conf}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h.yeni" "${_conf}")
file(COPY_FILE "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h.yeni"
               "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h" ONLY_IF_DIFFERENT)

set(HAL_KAYNAK hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_pwr hal_pwr_ex hal_flash hal_flash_ex
               hal_uart hal_uart_ex hal_dma hal_dma_ex hal_icache hal_exti)
set(HAL_C "")
foreach(_s ${HAL_KAYNAK})
  list(APPEND HAL_C "${HAL_DIR}/Src/stm32u5xx_${_s}.c")
endforeach()
add_library(mp5_hal STATIC ${HAL_C} "${DEV_DIR}/Source/Templates/system_stm32u5xx.c")
target_include_directories(mp5_hal PUBLIC
  "${CMAKE_BINARY_DIR}/uretilen" "${HAL_DIR}/Inc" "${DEV_DIR}/Include" "${CORE_INC}")
target_compile_definitions(mp5_hal PUBLIC STM32U5A9xx USE_HAL_DRIVER)
target_compile_options(mp5_hal PRIVATE -w ${MP5_FP})

# Ölçülen kitaplık.
if(MP5_KAYNAK STREQUAL "urun")
  set(MP5_IDA_LINK_A "${IDA_FW_DERLEME}/protocol/ida_link/libida_link.a")
  if(NOT EXISTS "${MP5_IDA_LINK_A}")
    message(FATAL_ERROR "Yok: ${MP5_IDA_LINK_A}")
  endif()
  add_library(mp5_ida_link STATIC IMPORTED)
  set_target_properties(mp5_ida_link PROPERTIES IMPORTED_LOCATION "${MP5_IDA_LINK_A}")
else()
  # Deneme: ürün kaynağının prototip içindeki kopyası, ürünün fw-release bayraklarıyla (eniyileme
  # düzeyi MP5_KOPYA_OPT). Ürün kitaplığına dokunulmaz.
  add_library(mp5_ida_link STATIC "${MP5_KOPYA_DOSYA}")
  target_include_directories(mp5_ida_link PRIVATE ${IDA_LINK_ICERME})
  target_include_directories(mp5_ida_link SYSTEM PRIVATE "${IDA_MAVLINK_URETILEN}")
  string(REPLACE "," ";" _kopya_tanim "${MP5_KOPYA_TANIM}")
  target_compile_definitions(mp5_ida_link PRIVATE ${_kopya_tanim})
  set_target_properties(mp5_ida_link PROPERTIES C_EXTENSIONS OFF)
  target_compile_options(mp5_ida_link PRIVATE --specs=nano.specs ${MP5_KOPYA_OPT} -g -DNDEBUG -std=c17
    -Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Werror -ffp-contract=off -fno-fast-math
    -pedantic-errors -fstack-usage)
endif()

set(MP5_LD "${DEV_DIR}/Source/Templates/gcc/linker/STM32U5A9xx_FLASH.ld")
set(MP5_STARTUP "${DEV_DIR}/Source/Templates/gcc/startup_stm32u5a9xx.s")

add_executable(mp5 fw/main.c fw/kart.c fw/kesmeler.c "${MP5_URETILEN}/mp5_akislar.c" "${MP5_STARTUP}")
target_include_directories(mp5 PRIVATE fw ${IDA_LINK_ICERME})
target_include_directories(mp5 SYSTEM PRIVATE "${IDA_MAVLINK_URETILEN}")
target_link_libraries(mp5 PRIVATE mp5_ida_link mp5_hal)
target_compile_definitions(mp5 PRIVATE MP5_DERLEME="${MP5_DERLEME}" MP5_KIMLIK="${MP5_KIMLIK}"
  MP5_YINELEME=${MP5_YINELEME}u MP5_ONBELLEK=${MP5_ONBELLEK}u)
target_compile_options(mp5 PRIVATE $<$<COMPILE_LANGUAGE:C>:${MP5_FP} -Wall -Wextra -Wconversion -fstack-usage>)
set_target_properties(mp5 PROPERTIES SUFFIX ".elf")
target_link_options(mp5 PRIVATE
  "-T${MP5_LD}" "-Wl,-Map=${CMAKE_BINARY_DIR}/mp5.map" "-Wl,--print-memory-usage"
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free")
add_custom_command(TARGET mp5 POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:mp5>" "${CMAKE_BINARY_DIR}/mp5.bin"
  COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:mp5>"
  VERBATIM)
