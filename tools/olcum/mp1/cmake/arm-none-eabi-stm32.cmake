# W0-MP1a: Cortex-M33 araç zinciri dosyası (atılabilir prototip).
# Derleyici: GNU Tools for STM32 14.3.rel1 (STM32CubeIDE paketinden). Homebrew arm-none-eabi kullanılmaz (09 §2.6).
# Yol ARM_GCC_BIN ortam değişkeninden ya da -DARM_GCC_BIN= ile gelir; yoksa CubeIDE'nin varsayılan yerinde aranır.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m33)

if(NOT ARM_GCC_BIN)
  if(DEFINED ENV{ARM_GCC_BIN} AND NOT "$ENV{ARM_GCC_BIN}" STREQUAL "")
    set(ARM_GCC_BIN "$ENV{ARM_GCC_BIN}")
  else()
    file(GLOB _mp1_gcc_adaylar
      "/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.macosaarch64_*/tools/bin")
    list(SORT _mp1_gcc_adaylar)
    list(GET _mp1_gcc_adaylar -1 ARM_GCC_BIN)
  endif()
endif()
set(ARM_GCC_BIN "${ARM_GCC_BIN}" CACHE PATH "GNU Tools for STM32 bin dizini")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GCC_BIN)

set(CMAKE_C_COMPILER   "${ARM_GCC_BIN}/arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "${ARM_GCC_BIN}/arm-none-eabi-gcc")
set(CMAKE_OBJCOPY      "${ARM_GCC_BIN}/arm-none-eabi-objcopy" CACHE FILEPATH "")
set(CMAKE_SIZE         "${ARM_GCC_BIN}/arm-none-eabi-size" CACHE FILEPATH "")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MP1_CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${MP1_CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${MP1_CPU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MP1_CPU_FLAGS} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
