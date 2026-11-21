# Derleme denetimlerini CTest'e bağlar (W2-depo §4).
#
#   ida_bellek_denetimi_ekle(<test-adı> <hedef> [BULGU_BEKLENIR <sembol>])
#     tools/denetim/bellek-ayirma.sh betiğini <hedef>'in çıktısı (.a) üzerinde koşturur.
#     BULGU_BEKLENIR verilirse test, betiğin o sembol için bulgu bildirmesini bekler
#     (bilerek bozulmuş deneme nesnesi); verilmezse betiğin 0 ile çıkmasını bekler.
#     Herhangi bir dizinden çağrılabilir: betik yolu işlevin kendi dosyasının yerinden
#     çözülür (modül düzeyindeki değişkenler include_guard yüzünden yalnız modülü ilk
#     içeren dizinde görünür). Betik yolu boş ya da dosya yoksa yapılandırma hatasıdır.
#     IDA_BELLEK_DENETIMI_BETIGI tanımlıysa (yalnız deneme için) onun değeri kullanılır.
#
# nm seçimi: fw kökünde arm-none-eabi-nm (ELF); macOS'ta sistem nm (Mach-O). Betik
# değiştirilmez; NM ortam değişkeniyle seçilir.
#
# MSVC (Windows host) bu sürümde denetlenmez: /MD ile derlenen nesnelerde CRT işlevlerine
# başvuru `__imp_malloc` biçimindedir ve betik bu öneki tanımaz (bulgu kaçırılır). Aynı
# çekirdek kaynakları Windows'ta da fw kökünde ELF olarak denetlenir. Betikte COFF/`__imp_`
# desteği gerekir (ayrı iş; betik W0-SOL1'indir).

include_guard(GLOBAL)

# bash: önbellek değişkeni (her dizinde görünür). Windows'ta Git Bash aranır; System32'deki
# bash (WSL) kullanılmaz.
if(CMAKE_HOST_WIN32)
  file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _ida_pf)
  file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _ida_pf64)
  find_program(IDA_BASH bash HINTS "${_ida_pf}/Git/bin" "${_ida_pf64}/Git/bin" NO_DEFAULT_PATH)
endif()
find_program(IDA_BASH bash)

function(ida_bellek_denetimi_ekle ad hedef)
  cmake_parse_arguments(PARSE_ARGV 2 A "" "BULGU_BEKLENIR" "")
  if(DEFINED IDA_BELLEK_DENETIMI_BETIGI)
    set(_betik "${IDA_BELLEK_DENETIMI_BETIGI}")
  else()
    get_filename_component(_betik
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/denetim/bellek-ayirma.sh" ABSOLUTE)
  endif()
  if(_betik STREQUAL "" OR NOT EXISTS "${_betik}")
    message(FATAL_ERROR "ida_bellek_denetimi_ekle(${ad}): bellek ayırma betiği bulunamadı: '${_betik}'")
  endif()
  if(MSVC)
    message(STATUS "MSVC nesneleri bellek ayırma betiğince denetlenemiyor; ${ad} eklenmedi")
    return()
  endif()
  if(NOT IDA_BASH)
    message(FATAL_ERROR "ida_bellek_denetimi_ekle(${ad}): bash bulunamadı")
  endif()
  if(NOT CMAKE_NM)
    message(FATAL_ERROR "ida_bellek_denetimi_ekle(${ad}): nm bulunamadı (CMAKE_NM boş)")
  endif()
  add_test(NAME ${ad} COMMAND "${IDA_BASH}" "${_betik}" "$<TARGET_FILE:${hedef}>")
  set_tests_properties(${ad} PROPERTIES ENVIRONMENT "NM=${CMAKE_NM}")
  if(A_BULGU_BEKLENIR)
    # Çıkış kodu 1 (bulgu) ve tam bulgu satırı; 64/66/70 araç hataları bu deseni üretmez.
    set_tests_properties(${ad} PROPERTIES
      PASS_REGULAR_EXPRESSION ": yasak tanımsız başvuru: _?${A_BULGU_BEKLENIR}(\n|$)"
      FAIL_REGULAR_EXPRESSION "bulunamadı|çalıştırılamadı|tanınmadı|okunamıyor|okunamadı|No such file")
  else()
    set_tests_properties(${ad} PROPERTIES FAIL_REGULAR_EXPRESSION "No such file")
  endif()
endfunction()
