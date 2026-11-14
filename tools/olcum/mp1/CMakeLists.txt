# W0-MP1a ölçüm prototipi (atılabilir; ürün koduna taşınmaz, 09 §2.1).
#   MP1_HEDEF=fw : kart aygıt yazılımı (SMPS ve LDO varyantları), araç zinciri dosyasıyla
#   MP1_HEDEF=pc : PC seri kayıt ve çözümleme aracı (C++20)
cmake_minimum_required(VERSION 3.20)

set(MP1_HEDEF "fw" CACHE STRING "fw ya da pc")

if(MP1_HEDEF STREQUAL "pc")
  project(mp1_pc LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  add_executable(mp1_seri pc/mp1_seri.cpp)
  target_compile_options(mp1_seri PRIVATE -Wall -Wextra -Wconversion)
  return()
endif()

project(mp1_fw LANGUAGES C ASM)

# Kaynaklar depoda değil: tools/olcum/mp1/kaynak-indir.sh ile $IDA_CACHE/src altına iner.
if(NOT MP1_SRC)
  if(DEFINED ENV{MP1_SRC} AND NOT "$ENV{MP1_SRC}" STREQUAL "")
    set(MP1_SRC "$ENV{MP1_SRC}")
  elseif(DEFINED ENV{IDA_CACHE} AND NOT "$ENV{IDA_CACHE}" STREQUAL "")
    set(MP1_SRC "$ENV{IDA_CACHE}/src")
  else()
    set(MP1_SRC "$ENV{HOME}/.cache/ida/src")
  endif()
endif()
set(MP1_SRC "${MP1_SRC}" CACHE PATH "ST kaynaklarının kökü")

set(HAL_DIR  "${MP1_SRC}/stm32u5xx-hal-driver")
set(DEV_DIR  "${MP1_SRC}/cmsis-device-u5")
set(CORE_INC "${MP1_SRC}/STM32CubeU5/Drivers/CMSIS/Core/Include")
foreach(_d "${HAL_DIR}/Src" "${DEV_DIR}/Include" "${CORE_INC}")
  if(NOT EXISTS "${_d}")
    message(FATAL_ERROR "Kaynak yok: ${_d}  (önce tools/olcum/mp1/kaynak-indir.sh)")
  endif()
endforeach()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)   # platform kodu gnu17 (09 §2.6)

# HAL yapılandırması: ST'nin şablonundan yalnız gereken modüller açık bırakılarak üretilir.
set(MP1_HAL_MODULLER HAL CORTEX RCC GPIO PWR FLASH UART DMA ICACHE IWDG EXTI)
file(READ "${HAL_DIR}/Inc/stm32u5xx_hal_conf_template.h" _conf)
string(REGEX REPLACE "#define (HAL_[A-Z0-9]+_MODULE_ENABLED)" "/* #define \\1 */" _conf "${_conf}")
string(REPLACE "/* #define HAL_MODULE_ENABLED */" "#define HAL_MODULE_ENABLED" _conf "${_conf}")
foreach(_m ${MP1_HAL_MODULLER})
  string(REPLACE "/* #define HAL_${_m}_MODULE_ENABLED */" "#define HAL_${_m}_MODULE_ENABLED" _conf "${_conf}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h.yeni" "${_conf}")
file(COPY_FILE "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h.yeni"
               "${CMAKE_BINARY_DIR}/uretilen/stm32u5xx_hal_conf.h" ONLY_IF_DIFFERENT)

set(HAL_KAYNAK hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_pwr hal_pwr_ex hal_flash hal_flash_ex
               hal_uart hal_uart_ex hal_dma hal_dma_ex hal_icache hal_iwdg hal_exti)
set(HAL_C "")
foreach(_s ${HAL_KAYNAK})
  list(APPEND HAL_C "${HAL_DIR}/Src/stm32u5xx_${_s}.c")
endforeach()

add_library(mp1_hal STATIC
  ${HAL_C}
  "${DEV_DIR}/Source/Templates/system_stm32u5xx.c")
target_include_directories(mp1_hal PUBLIC
  "${CMAKE_BINARY_DIR}/uretilen" "${HAL_DIR}/Inc" "${DEV_DIR}/Include" "${CORE_INC}")
target_compile_definitions(mp1_hal PUBLIC STM32U5A9xx USE_HAL_DRIVER)
target_compile_options(mp1_hal PRIVATE -w)   # ST kodunun uyarıları bu prototipin konusu değil

set(MP1_LD "${DEV_DIR}/Source/Templates/gcc/linker/STM32U5A9xx_FLASH.ld")
set(MP1_STARTUP "${DEV_DIR}/Source/Templates/gcc/startup_stm32u5a9xx.s")

# İki varyant: regülatör SMPS ve LDO (B01 K2). Aynı kaynak, tek tanım farkı.
foreach(_v smps ldo)
  set(_t mp1_${_v})
  add_executable(${_t} fw/main.c fw/kesmeler.c "${MP1_STARTUP}")
  target_link_libraries(${_t} PRIVATE mp1_hal)
  if(_v STREQUAL "smps")
    target_compile_definitions(${_t} PRIVATE MP1_REG_SMPS=1)
  else()
    target_compile_definitions(${_t} PRIVATE MP1_REG_SMPS=0)
  endif()
  target_compile_options(${_t} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wconversion -Wdouble-promotion>)
  set_target_properties(${_t} PROPERTIES SUFFIX ".elf")
  target_link_options(${_t} PRIVATE
    "-T${MP1_LD}" "-Wl,-Map=${CMAKE_BINARY_DIR}/${_t}.map" "-Wl,--print-memory-usage")
  add_custom_command(TARGET ${_t} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${_t}>" "${CMAKE_BINARY_DIR}/${_t}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${_t}>"
    VERBATIM)
endforeach()
