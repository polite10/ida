# Derleme denetimlerini CTest'e bağlar (W2-depo §4).
#
#   ida_bellek_denetimi_ekle(<test-adı> <hedef> [BULGU_BEKLENIR <sembol>])
#     tools/denetim/bellek-ayirma.sh betiğini <hedef>'in çıktısı (.a) üzerinde koşturur.
#     BULGU_BEKLENIR verilirse test, betiğin o sembol için bulgu bildirmesini bekler
#     (bilerek bozulmuş deneme nesnesi); verilmezse betiğin 0 ile çıkmasını bekler.
#
# nm seçimi: fw kökünde arm-none-eabi-nm (ELF); macOS'ta sistem nm (Mach-O). Betik
# değiştirilmez; NM ortam değişkeniyle seçilir.
#
# MSVC (Windows host) bu sürümde denetlenmez: /MD ile derlenen nesnelerde CRT işlevlerine
# başvuru `__imp_malloc` biçimindedir ve betik bu öneki tanımaz (bulgu kaçırılır). Aynı
# çekirdek kaynakları Windows'ta da fw kökünde ELF olarak denetlenir. Betikte COFF/`__imp_`
# desteği gerekir; değişiklik W2-depo raporunda önerildi (betik W0-SOL1'indir).

include_guard(GLOBAL)

set(IDA_BELLEK_DENETIMI "${PROJECT_SOURCE_DIR}/tools/denetim/bellek-ayirma.sh")

function(ida_bellek_denetimi_ekle ad hedef)
  cmake_parse_arguments(PARSE_ARGV 2 A "" "BULGU_BEKLENIR" "")
  if(MSVC)
    message(STATUS "MSVC nesneleri bellek ayırma betiğince denetlenemiyor; ${ad} eklenmedi")
    return()
  endif()
  if(NOT IDA_BASH)
    message(WARNING "bash bulunamadı; ${ad} denetimi eklenmedi")
    return()
  endif()
  if(NOT CMAKE_NM)
    message(WARNING "nm bulunamadı; ${ad} denetimi eklenmedi")
    return()
  endif()
  add_test(NAME ${ad}
    COMMAND "${IDA_BASH}" "${IDA_BELLEK_DENETIMI}" "$<TARGET_FILE:${hedef}>")
  set_tests_properties(${ad} PROPERTIES ENVIRONMENT "NM=${CMAKE_NM}")
  if(A_BULGU_BEKLENIR)
    # Çıkış kodu 1 (bulgu) ve tam bulgu satırı; 64/66/70 araç hataları bu deseni üretmez.
    set_tests_properties(${ad} PROPERTIES
      PASS_REGULAR_EXPRESSION ": yasak tanımsız başvuru: _?${A_BULGU_BEKLENIR}(\n|$)"
      FAIL_REGULAR_EXPRESSION "bulunamadı|çalıştırılamadı|tanınmadı|okunamıyor|okunamadı")
  endif()
endfunction()
