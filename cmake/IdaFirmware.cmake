# Firmware ELF kuralları (yalnız fw kökü; W2-depo §2, §4).
#
#   ida_fw_elf_kur(<hedef>)
#     - firmware başlangıç kitaplığına ve bağlayıcı betiğine bağlar,
#     - harita dosyası (<hedef>.map) ve boyut raporu (<hedef>.boyut.txt) üretir,
#     - bellek ayırma simgelerini --wrap ile tanımsız bırakır: malloc ailesine giden her
#       başvuru __wrap_<ad> tanımsız başvurusuna dönüşür ve bağlama hatası verir
#       (boş tanım değil; 00 08.13, 09 §2.8).

include_guard(GLOBAL)

# Not: işlevler modül düzeyindeki normal değişkenlere dayanmaz (include_guard yüzünden
# yalnız modülü ilk içeren dizinde görünürler); sabitler işlevin içinde, yollar işlevin
# kendi dosyasının yerinden (CMAKE_CURRENT_FUNCTION_LIST_DIR) çözülür. Önbellek
# değişkenleri her dizinde görünür.
get_filename_component(_ida_ld_varsayilan
  "${CMAKE_CURRENT_LIST_DIR}/../firmware/yer_tutucu/stm32u5a9_yer_tutucu.ld" ABSOLUTE)
set(IDA_FW_BAGLAYICI_BETIGI "${_ida_ld_varsayilan}"
  CACHE FILEPATH "Firmware bağlayıcı betiği (W3'te firmware/bsp/ altındaki betik gelir)")

function(ida_fw_elf_kur hedef)
  # 09 §2.8'deki küme (malloc/calloc/realloc/free/_malloc_r) + newlib'in öteki yeniden
  # girilebilir karşılıkları ve hizalı ayırma.
  set(IDA_FW_YASAK_AYIRMA
    malloc calloc realloc free memalign aligned_alloc posix_memalign valloc
    _malloc_r _calloc_r _realloc_r _free_r _memalign_r)
  set_target_properties(${hedef} PROPERTIES SUFFIX ".elf")
  target_link_libraries(${hedef} PRIVATE ida_fw_baslangic)
  set(_harita "$<TARGET_FILE_DIR:${hedef}>/${hedef}.map")
  set(_sar "")
  foreach(_s IN LISTS IDA_FW_YASAK_AYIRMA)
    list(APPEND _sar "LINKER:--wrap=${_s}")
  endforeach()
  target_link_options(${hedef} PRIVATE
    "LINKER:-T,${IDA_FW_BAGLAYICI_BETIGI}"
    "LINKER:-Map=${_harita},--cref"
    "LINKER:--print-memory-usage"
    "LINKER:--fatal-warnings"
    ${_sar})
  set_property(TARGET ${hedef} APPEND PROPERTY LINK_DEPENDS "${IDA_FW_BAGLAYICI_BETIGI}")
  add_custom_command(TARGET ${hedef} POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            "-DSIZE=${CMAKE_SIZE}"
            "-DELF=$<TARGET_FILE:${hedef}>"
            "-DCIKTI=$<TARGET_FILE_DIR:${hedef}>/${hedef}.boyut.txt"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boyut_raporu.cmake"
    COMMENT "Boyut raporu: ${hedef}.boyut.txt"
    VERBATIM)
endfunction()
