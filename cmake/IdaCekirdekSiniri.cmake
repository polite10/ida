# Ortak çekirdeğin sınırı (09 §2.6; W2-depo §2): `ida_core` ST, CMSIS, RTOS ya da işletim
# sistemi başlığına ve çekirdek dışındaki hiçbir başlığa bağımlı olamaz.
#
#   ida_cekirdek_siniri_kur(<hedef> KOKLER <dizin>...)
#
# İki katman:
#
# A. Yapılandırma sırasında (kök dizinin sonunda, DEFER; ihlal FATAL_ERROR'dur, derleme
#    başlamaz). Erken uyarı katmanıdır:
#    1. Hedefin INCLUDE_DIRECTORIES, INTERFACE_INCLUDE_DIRECTORIES ve
#       INTERFACE_SYSTEM_INCLUDE_DIRECTORIES değerlerinde yasak desen.
#    2. LINK_LIBRARIES: çekirdek hiçbir kitaplığa bağlanmaz (izin listesi boş).
#    3. Kaynak dosyalarındaki doğrudan #include yönergelerinde yasak desen. Yapılandırma
#       sırasında bulunmayan ya da generator expression ile verilen kaynak sessizce
#       atlanmaz: yapılandırma hatasıdır (W2-depo sol S1).
#
# B. Derlemeden sonra, derlemenin GERÇEK başlık bağımlılıkları üzerinden (CTest:
#    cekirdek.siniri.basliklar; cmake/cekirdek_baslik_denetimi.cmake). Dolaylı içermeyi de
#    yakalar (ör. kaynak değişmeden iskelet.h'ye yasak içerme eklenmesi). Her başlık ya
#    çekirdek köklerinin (KOKLER) ya da derleyicinin/standart C kitaplığının sistem
#    köklerinin altında olmalı; ayrıca kimliği (ST/CMSIS/RTOS deseni, işletim sistemi API
#    başlığı) yasak olmamalı. Bağımlılık bilgisini Ninja saklar (GCC/Clang depfile, MSVC
#    /showIncludes); bu yüzden Ninja üreteci gerekir.
#
# Sistem kökleri: GCC/Clang'de CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES (/usr/local/include ve
# /opt/homebrew/include hariç: oradaki başlıklar derleyicinin değil, kurulmuş kitaplıklarındır);
# MSVC'de yalnız araç takımının include dizini ve UCRT (Windows SDK'nın um/shared/winrt
# dizinleri sistem kökü sayılmaz).

include_guard(GLOBAL)

# Sabitler işlevin içindedir (modül düzeyindeki değişkenler include_guard yüzünden yalnız
# modülü ilk içeren dizinde görünür; denetim kök dizinin sonunda koşar).

function(_ida_cekirdek_ihlal hedef ayrinti)
  message(FATAL_ERROR
    "İDA çekirdek sınırı ihlali (${hedef}): ${ayrinti}\n"
    "Çekirdek (ISO C17) ST, CMSIS ya da RTOS başlığına bağımlı olamaz (09 §2.6).")
endfunction()

function(_ida_yollari_denetle hedef kaynak_adi yollar)
  # Desen yalnız bilinen köklerden (depo, kaynak ve derleme dizini) sonraki kuyrukta aranır:
  # deponun bulunduğu dizinin adı yanlış pozitif üretmez.
  get_filename_component(_depo "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  set(_onekler "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}" "${_depo}")
  foreach(_y IN LISTS yollar)
    set(_kuyruk "${_y}")
    string(REGEX REPLACE "^\\$<BUILD_INTERFACE:(.*)>$" "\\1" _duz "${_y}")
    set(_en_uzun 0)
    foreach(_o IN LISTS _onekler)
      string(LENGTH "${_o}" _u)
      string(FIND "${_duz}" "${_o}/" _i)
      if(_i EQUAL 0 AND _u GREATER _en_uzun)
        set(_en_uzun ${_u})
        math(EXPR _b "${_u} + 1")
        string(SUBSTRING "${_duz}" ${_b} -1 _kuyruk)
      endif()
    endforeach()
    string(TOLOWER "${_kuyruk}" _k)
    if(_k MATCHES "${IDA_CEKIRDEK_YASAK_DESEN}")
      _ida_cekirdek_ihlal(${hedef} "${kaynak_adi} içinde yasak include yolu: ${_y}")
    endif()
  endforeach()
endfunction()

function(ida_cekirdek_sinirini_denetle hedef)
  set(IDA_CEKIRDEK_YASAK_DESEN "(stm32|cmsis|core_cm[0-9]|freertos|threadx|tx_api|zephyr|rtos)")
  set(IDA_CEKIRDEK_IZINLI_BAGLANTI "")
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
    if(_s MATCHES "\\$<")
      _ida_cekirdek_ihlal(${hedef}
        "kaynak generator expression ile verilmiş, yapılandırmada denetlenemez: ${_s}")
    endif()
    if(NOT IS_ABSOLUTE "${_s}")
      set(_s "${_kaynak_dizini}/${_s}")
    endif()
    if(NOT EXISTS "${_s}")
      _ida_cekirdek_ihlal(${hedef}
        "kaynak yapılandırma sırasında yok (üretilmiş kaynak çekirdeğe giremez): ${_s}")
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

# Derleyicinin sistem başlık köklerini döndürür (açıklama dosyanın başında).
function(_ida_sistem_kokleri cikti)
  set(_l "")
  if(MSVC)
    if("$ENV{VCToolsInstallDir}" STREQUAL "" OR "$ENV{UniversalCRTSdkDir}" STREQUAL ""
       OR "$ENV{UCRTVersion}" STREQUAL "")
      message(FATAL_ERROR
        "Çekirdek başlık denetimi: MSVC geliştirici ortamı yok (VCToolsInstallDir, "
        "UniversalCRTSdkDir, UCRTVersion); sistem kökleri belirlenemiyor")
    endif()
    list(APPEND _l "$ENV{VCToolsInstallDir}/include"
                   "$ENV{UniversalCRTSdkDir}/Include/$ENV{UCRTVersion}/ucrt")
  else()
    foreach(_d IN LISTS CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
      if(_d MATCHES "^(/usr/local|/opt/homebrew)/include/?$")
        continue()
      endif()
      list(APPEND _l "${_d}")
    endforeach()
  endif()
  if(NOT _l)
    message(FATAL_ERROR "Çekirdek başlık denetimi: derleyicinin sistem başlık kökleri bulunamadı")
  endif()
  set(${cikti} "${_l}" PARENT_SCOPE)
endfunction()

function(ida_cekirdek_siniri_kur hedef)
  cmake_parse_arguments(PARSE_ARGV 1 A "" "" "KOKLER")
  if(NOT A_KOKLER)
    message(FATAL_ERROR "ida_cekirdek_siniri_kur(${hedef}): KOKLER verilmeli")
  endif()
  # A katmanı. DEFER'in argümanlarındaki değişkenler çağrı anında açılır; hedef adı şimdi
  # gömülür.
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]] CALL ida_cekirdek_sinirini_denetle [[${hedef}]])")

  # B katmanı.
  if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR
      "Çekirdek başlık denetimi Ninja üreteci ister (bağımlılık bilgisi ninja -t deps ile okunur); "
      "üreteç: ${CMAKE_GENERATOR}")
  endif()
  _ida_sistem_kokleri(_sistem)
  if(CMAKE_HOST_WIN32)
    set(_buyuk_kucuk ON)
  else()
    set(_buyuk_kucuk OFF)
  endif()
  add_test(NAME cekirdek.siniri.basliklar
    COMMAND "${CMAKE_COMMAND}"
            "-DNINJA=${CMAKE_MAKE_PROGRAM}"
            "-DIKILI=${CMAKE_BINARY_DIR}"
            "-DNESNELER=$<TARGET_OBJECTS:${hedef}>"
            "-DCEKIRDEK_KOKLERI=${A_KOKLER}"
            "-DSISTEM_KOKLERI=${_sistem}"
            "-DBUYUK_KUCUK=${_buyuk_kucuk}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cekirdek_baslik_denetimi.cmake")
endfunction()
