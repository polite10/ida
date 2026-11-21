# İDA firmware araç zinciri: STM32U5A9J-DK (Cortex-M33, tek duyarlıklı donanım FPU), newlib-nano.
#
# Derleyici: GNU Tools for STM32 14.3.rel1 (geliştirme Mac'i, STM32CubeIDE paketi) ya da aynı
# üst akış sürümü Arm GNU Toolchain 14.3.rel1 (CI). İkisi de `arm-none-eabi-gcc -dumpversion`
# için 14.3.1 verir; başka sürüm reddedilir (Homebrew'deki daha yeni sürüm dahil; 09 §2.6).
#
# Yol: ARM_GCC_BIN (önbellek değişkeni ya da ortam değişkeni) derleyicinin bin dizinidir.
# Verilmezse arm-none-eabi-gcc PATH'te aranır. Kişisel makine yolu bu dosyaya yazılmaz.
#
# libc: newlib-nano (--specs=nano.specs). Picolibc bu derleyici paketinde yok (MP1 bulgu 3).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m33)

# Çalıştırılabilir dosya soneki (CMAKE_HOST_WIN32 cmake'in kendisince en başta tanımlanır).
if(CMAKE_HOST_WIN32)
  set(_ida_son ".exe")
else()
  set(_ida_son "")
endif()

if(NOT ARM_GCC_BIN AND DEFINED ENV{ARM_GCC_BIN} AND NOT "$ENV{ARM_GCC_BIN}" STREQUAL "")
  set(ARM_GCC_BIN "$ENV{ARM_GCC_BIN}")
endif()
if(ARM_GCC_BIN)
  file(TO_CMAKE_PATH "${ARM_GCC_BIN}" ARM_GCC_BIN)
  set(_ida_gcc "${ARM_GCC_BIN}/arm-none-eabi-gcc${_ida_son}")
else()
  find_program(_ida_gcc arm-none-eabi-gcc)
  if(NOT _ida_gcc)
    message(FATAL_ERROR
      "arm-none-eabi-gcc bulunamadı. ARM_GCC_BIN ortam değişkenine GNU Tools for STM32 / "
      "Arm GNU Toolchain 14.3.rel1'in bin dizinini verin.")
  endif()
  get_filename_component(ARM_GCC_BIN "${_ida_gcc}" DIRECTORY)
endif()
set(ARM_GCC_BIN "${ARM_GCC_BIN}" CACHE PATH "arm-none-eabi araç zincirinin bin dizini")
# try_compile projeleri de aynı yolu görür.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GCC_BIN)

if(NOT EXISTS "${_ida_gcc}")
  message(FATAL_ERROR "Derleyici yok: ${_ida_gcc}")
endif()

# Sürüm sabit: 14.3.1 (14.3.rel1). Başka sürümle derleme reddedilir.
set(IDA_ARM_GCC_SURUMU "14.3.1")
execute_process(COMMAND "${_ida_gcc}" -dumpversion
  OUTPUT_VARIABLE _ida_gcc_surum OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _ida_rc)
if(NOT _ida_rc EQUAL 0 OR NOT _ida_gcc_surum STREQUAL IDA_ARM_GCC_SURUMU)
  message(FATAL_ERROR
    "arm-none-eabi-gcc sürümü '${_ida_gcc_surum}'; beklenen ${IDA_ARM_GCC_SURUMU} (14.3.rel1). "
    "Yol: ${_ida_gcc}")
endif()

set(_ida_on "${ARM_GCC_BIN}/arm-none-eabi-")
set(CMAKE_C_COMPILER   "${_ida_on}gcc${_ida_son}")
set(CMAKE_CXX_COMPILER "${_ida_on}g++${_ida_son}")
set(CMAKE_ASM_COMPILER "${_ida_on}gcc${_ida_son}")
set(CMAKE_AR           "${_ida_on}gcc-ar${_ida_son}" CACHE FILEPATH "")
set(CMAKE_RANLIB       "${_ida_on}gcc-ranlib${_ida_son}" CACHE FILEPATH "")
set(CMAKE_NM           "${_ida_on}nm${_ida_son}" CACHE FILEPATH "")
set(CMAKE_OBJCOPY      "${_ida_on}objcopy${_ida_son}" CACHE FILEPATH "")
set(CMAKE_OBJDUMP      "${_ida_on}objdump${_ida_son}" CACHE FILEPATH "")
set(CMAKE_SIZE         "${_ida_on}size${_ida_son}" CACHE FILEPATH "")

# Derleyici sınaması bağlama yapmaz (başlangıç dosyası ve bağlayıcı betiği projenindir).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M33 + FPv5 tek duyarlıklı, donanım kayan nokta ABI'si; newlib-nano.
set(IDA_FW_CPU_BAYRAKLARI "-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${IDA_FW_CPU_BAYRAKLARI} --specs=nano.specs -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${IDA_FW_CPU_BAYRAKLARI} --specs=nano.specs -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${IDA_FW_CPU_BAYRAKLARI} -x assembler-with-cpp")
# Bağlama komutu dilin derleme bayraklarını (CPU bayrakları, --specs=nano.specs) zaten
# içerir; nano.specs'in ikinci kez verilmesi GCC'de hatadır.
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nosys.specs -nostartfiles -Wl,--gc-sections")

# Yapı türlerinin eniyileme düzeyleri CMakePresets.json'daki fw-* ön ayarlarındadır
# (CMAKE_<DİL>_FLAGS_<TÜR>): *_INIT değerlerine CMake kendi -g / -O3 bayrağını ekler.

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
