# Ortak çekirdeğin sınırı (09 §2.6; W2-depo §2): `ida_core` hedefinin include yoluna
# ST/CMSIS/RTOS başlığı giremez ve çekirdek kaynakları böyle bir başlığı içeremez.
#
#   ida_cekirdek_siniri_kur(<hedef>)
#
# Denetim yapılandırmanın sonunda (kök dizinin sonunda, DEFER) yapılır; böylece çekirdek
# tanımlandıktan sonra başka bir dizinden eklenen include yolu ve kaynak da yakalanır.
# İhlal FATAL_ERROR'dur: yapılandırma ve dolayısıyla derleme durur.
#
# Denetlenen:
#   1. Hedefin INCLUDE_DIRECTORIES, INTERFACE_INCLUDE_DIRECTORIES ve
#      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES değerleri.
#   2. Hedefin LINK_LIBRARIES listesi: çekirdek hiçbir kitaplığa bağlanmaz (izin listesi
#      IDA_CEKIRDEK_IZINLI_BAGLANTI bu sürümde boştur); böylece başka bir hedeften geçişli
#      include yolu da gelemez.
#   3. Hedefin kaynak dosyalarındaki #include yönergeleri (yol gerekmeden aynı dizindeki
#      başlığı içermeye karşı).
# Yasak desen (küçük harfe çevrilmiş metinde): stm32, cmsis, core_cm, freertos, threadx,
# tx_api, zephyr, cmsis_os, rtos.

include_guard(GLOBAL)

set(IDA_CEKIRDEK_YASAK_DESEN "(stm32|cmsis|core_cm[0-9]|freertos|threadx|tx_api|zephyr|rtos)")
set(IDA_CEKIRDEK_IZINLI_BAGLANTI "")

function(_ida_cekirdek_ihlal hedef ayrinti)
  message(FATAL_ERROR
    "İDA çekirdek sınırı ihlali (${hedef}): ${ayrinti}\n"
    "Çekirdek (ISO C17) ST, CMSIS ya da RTOS başlığına bağımlı olamaz (09 §2.6).")
endfunction()

function(_ida_yollari_denetle hedef kaynak_adi yollar)
  foreach(_y IN LISTS yollar)
    string(TOLOWER "${_y}" _k)
    if(_k MATCHES "${IDA_CEKIRDEK_YASAK_DESEN}")
      _ida_cekirdek_ihlal(${hedef} "${kaynak_adi} içinde yasak include yolu: ${_y}")
    endif()
  endforeach()
endfunction()

function(ida_cekirdek_sinirini_denetle hedef)
  get_target_property(_kaynak_dizini ${hedef} SOURCE_DIR)

  foreach(_ozellik INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES
                   INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    get_target_property(_yollar ${hedef} ${_ozellik})
    if(_yollar)
      _ida_yollari_denetle(${hedef} "${_ozellik}" "${_yollar}")
    endif()
  endforeach()

  get_target_property(_baglar ${hedef} LINK_LIBRARIES)
  if(_baglar)
    foreach(_b IN LISTS _baglar)
      if(NOT _b IN_LIST IDA_CEKIRDEK_IZINLI_BAGLANTI)
        _ida_cekirdek_ihlal(${hedef} "izinsiz bağlantı: ${_b} (çekirdek başka kitaplığa bağlanmaz)")
      endif()
    endforeach()
  endif()

  get_target_property(_kaynaklar ${hedef} SOURCES)
  foreach(_s IN LISTS _kaynaklar)
    if(_s MATCHES "^\\$<")
      continue()
    endif()
    if(NOT IS_ABSOLUTE "${_s}")
      set(_s "${_kaynak_dizini}/${_s}")
    endif()
    if(NOT EXISTS "${_s}")
      continue()
    endif()
    file(STRINGS "${_s}" _satirlar REGEX "^[ \t]*#[ \t]*include")
    foreach(_satir IN LISTS _satirlar)
      string(TOLOWER "${_satir}" _k)
      if(_k MATCHES "#[ \t]*include[ \t]*[<\"][^>\"]*${IDA_CEKIRDEK_YASAK_DESEN}")
        _ida_cekirdek_ihlal(${hedef} "${_s}: yasak başlık: ${_satir}")
      endif()
    endforeach()
  endforeach()
endfunction()

function(ida_cekirdek_siniri_kur hedef)
  # DEFER'in argümanlarındaki değişkenler çağrı anında açılır; hedef adı şimdi gömülür.
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]] CALL ida_cekirdek_sinirini_denetle [[${hedef}]])")
endfunction()
