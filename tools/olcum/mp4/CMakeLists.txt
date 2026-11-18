# W0-MP4a ölçüm prototipi (atılabilir; ürün koduna taşınmaz, 09 §2.1).
#   MP4_HEDEF=fw : kart aygıt yazılımı; optimizasyon düzeyi ön ayardan (-O2 birincil, -Og)
#   MP4_HEDEF=pc : PC seri kayıt/çözümleme ve çağrı grafiği denetim aracı (C++20)
cmake_minimum_required(VERSION 3.20)

set(MP4_HEDEF "fw" CACHE STRING "fw ya da pc")

if(MP4_HEDEF STREQUAL "pc")
  project(mp4_pc LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  add_executable(mp4_seri pc/mp4_seri.cpp)
  add_executable(mp4_cagri pc/mp4_cagri.cpp)
  foreach(_t mp4_seri mp4_cagri)
    target_compile_options(${_t} PRIVATE -Wall -Wextra -Wconversion)
  endforeach()
  return()
endif()

project(mp4_fw LANGUAGES C ASM)

if(NOT MP4_SRC)
  if(DEFINED ENV{IDA_CACHE} AND NOT "$ENV{IDA_CACHE}" STREQUAL "")
    set(MP4_SRC "$ENV{IDA_CACHE}/src")
  else()
    set(MP4_SRC "$ENV{HOME}/.cache/ida/src")
  endif()
endif()
set(MP4_SRC "${MP4_SRC}" CACHE PATH "ST kaynaklarının kökü")
set(MP4_DERLEME "?" CACHE STRING "derleme etiketi (O2 / Og)")
set(MP4_KIMLIK "yok" CACHE STRING "derleme kimliği (derle.sh üretir; seri çıktıyı imaja bağlar)")
set(MP4_HZ 400 CACHE STRING "tetik hızı (Hz); 160 MHz'i tam bölmeli")
set(MP4_FAZLAR 7 CACHE STRING "koşulacak fazların bit maskesi: 1 A, 2 B, 4 C")
set(MP4_FAZ_ADIM 241000 CACHE STRING "faz başına kayıtlı adım (kısa düzenek denemesi için küçültülür)")

set(HAL_DIR  "${MP4_SRC}/stm32u5xx-hal-driver")
set(DEV_DIR  "${MP4_SRC}/cmsis-device-u5")
set(CORE_INC "${MP4_SRC}/STM32CubeU5/Drivers/CMSIS/Core/Include")
foreach(_d "${HAL_DIR}/Src" "${DEV_DIR}/Include" "${CORE_INC}")
  if(NOT EXISTS "${_d}")
    message(FATAL_ERROR "Kaynak yok: ${_d}  (tools/olcum/mp1/kaynak-indir.sh)")
  endif()
endforeach()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)   # platform kodu gnu17; adım kodu aşağıda uzantısız

# Kayan nokta bayrakları (09 §2.6), bütün C kodu için
set(MP4_FP -ffp-contract=off -fno-fast-math)

set(MP4_HAL_MODULLER HAL CORTEX RCC GPIO PWR FLASH UART DMA ICACHE EXTI TIM)
file(READ "${HAL_DIR}/Inc/stm32u5xx_hal_conf_template.h" _conf)
string(REGEX REPLACE "#define (HAL_[A-Z0-9]+_MODULE_ENABLED)" "/* #define \\1 */" _conf "${_conf}")
string(REPLACE "/* #define HAL_MODULE_ENABLED */" "#define HAL_MODULE_ENABLED" _conf "${_conf}")
foreach(_m ${MP4_HAL_MODULLER})
  string(REPLACE "/* #define HAL_${_m}_MODULE_ENABLED */" "#define HAL_${_m}_MODULE_ENABLED" _conf "${_conf}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h.yeni" "${_conf}")
file(COPY_FILE "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h.yeni"
               "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h" ONLY_IF_DIFFERENT)

set(HAL_KAYNAK hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_pwr hal_pwr_ex hal_flash hal_flash_ex
               hal_uart hal_uart_ex hal_dma hal_dma_ex hal_icache hal_exti hal_tim hal_tim_ex)
set(HAL_C "")
foreach(_s ${HAL_KAYNAK})
  list(APPEND HAL_C "${HAL_DIR}/Src/stm32u5xx_${_s}.c")
endforeach()

add_library(mp4_hal STATIC ${HAL_C} "${DEV_DIR}/Source/Templates/system_stm32u5xx.c")
target_include_directories(mp4_hal PUBLIC
  "${CMAKE_BINARY_DIR}/uretilen" "${HAL_DIR}/Inc" "${DEV_DIR}/Include" "${CORE_INC}")
target_compile_definitions(mp4_hal PUBLIC STM32U5A9xx USE_HAL_DRIVER)
target_compile_options(mp4_hal PRIVATE -w ${MP4_FP})

# Yer tutucu çekirdek adımı: ISO C17, uzantısız, tek duyarlıklı; CMSIS/HAL içerme yolu yok.
add_library(mp4_adim STATIC adim/yt_adim.c adim/yt_ekf.c adim/yt_kontrol.c adim/yt_mat.c)
set_target_properties(mp4_adim PROPERTIES C_EXTENSIONS OFF)
target_include_directories(mp4_adim PUBLIC adim)
target_compile_options(mp4_adim PRIVATE -std=c17 -pedantic-errors ${MP4_FP}
  -Wall -Wextra -Wconversion -Wdouble-promotion -Wfloat-conversion -Wshadow -Wvla
  -fstack-usage -Wstack-usage=4096)

set(MP4_LD "${DEV_DIR}/Source/Templates/gcc/linker/STM32U5A9xx_FLASH.ld")
set(MP4_STARTUP "${DEV_DIR}/Source/Templates/gcc/startup_stm32u5a9xx.s")

add_executable(mp4 fw/platform.c fw/kesmeler.c "${MP4_STARTUP}")
target_link_libraries(mp4 PRIVATE mp4_adim mp4_hal m)
target_compile_definitions(mp4 PRIVATE MP4_DERLEME="${MP4_DERLEME}" MP4_KIMLIK="${MP4_KIMLIK}"
  MP4_HZ=${MP4_HZ}u MP4_FAZLAR=${MP4_FAZLAR}u MP4_FAZ_ADIM=${MP4_FAZ_ADIM}u)
target_compile_options(mp4 PRIVATE $<$<COMPILE_LANGUAGE:C>:${MP4_FP} -Wall -Wextra -Wconversion -Wdouble-promotion -fstack-usage>)
set_target_properties(mp4 PROPERTIES SUFFIX ".elf")
target_link_options(mp4 PRIVATE
  "-T${MP4_LD}" "-Wl,-Map=${CMAKE_BINARY_DIR}/mp4.map" "-Wl,--print-memory-usage"
  "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free")
add_custom_command(TARGET mp4 POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:mp4>" "${CMAKE_BINARY_DIR}/mp4.bin"
  COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:mp4>"
  VERBATIM)
