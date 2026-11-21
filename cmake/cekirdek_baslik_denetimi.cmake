# Çekirdek sınırının derleme sonrası denetimi (cmake -P; CTest): ida_core nesnelerinin
# GERÇEK başlık bağımlılıkları (derleyicinin ürettiği bağımlılık bilgisi; Ninja'nın
# kaydettiği hâliyle `ninja -t deps`) taranır. GCC/Clang'de bu bilgi depfile'dan
# (-MD), MSVC'de /showIncludes'tan gelir; Ninja ikisini de aynı biçimde saklar.
#
#   -DNINJA=<ninja> -DIKILI=<derleme dizini> -DNESNELER=<nesne;...>
#   -DCEKIRDEK_KOKLERI=<dizin;...>   çekirdeğin kendi başlık kökleri (include/, src/)
#   -DSISTEM_KOKLERI=<dizin;...>     derleyicinin ve standart C kitaplığının başlık kökleri;
#                                    Clang'de derleyicinin kaynak dizini de (sanitizer'ın
#                                    yok sayma listesi gibi başlık olmayan bağımlılıklar
#                                    oradan gelir; IdaCekirdekSiniri.cmake)
#   -DBUYUK_KUCUK=ON|OFF             yolları büyük/küçük harf duyarsız karşılaştır (Windows)
#
# Her başlık için:
#   1. Kök: çekirdek köklerinden ya da sistem köklerinden birinin altında olmalı; değilse
#      ihlal (firmware/, ST kütüphaneleri, derleme dizini, depo içindeki başka bir dizin ya da
#      dış bir kök).
#   2. Kimlik (kökten sonraki yol kuyruğunda): ST, CMSIS ya da RTOS desenine uyan başlık
#      (dizin ve dosya adı ne olursa olsun; ör. core/src/stm32u5xx.h) ve işletim sistemi API
#      başlığı (windows.h, unistd.h, pthread.h, ...) ihlal.
# Nesnenin bağımlılık kaydı yoksa ya da eskiyse (VALID değil) denetim başarısızdır: derleme
# yapılmamış ya da güncel değil demektir; sessizce geçilmez.

foreach(_d NINJA IKILI NESNELER CEKIRDEK_KOKLERI SISTEM_KOKLERI)
  if(NOT DEFINED ${_d} OR "${${_d}}" STREQUAL "")
    message(FATAL_ERROR "cekirdek_baslik_denetimi.cmake: ${_d} verilmedi")
  endif()
endforeach()

set(_yasak_kimlik "(stm32|cmsis|core_cm[0-9]|freertos|threadx|tx_api|zephyr|rtos)")
set(_yasak_os "(^|/)(windows\\.h|winbase\\.h|winsock2?\\.h|unistd\\.h|pthread\\.h|dlfcn\\.h|sys/mman\\.h|sys/socket\\.h|dispatch/dispatch\\.h|mach/mach\\.h)$")

function(_ida_yol_normal girdi cikti)
  file(TO_CMAKE_PATH "${girdi}" _y)
  if(NOT IS_ABSOLUTE "${_y}")
    set(_y "${IKILI}/${_y}")
  endif()
  get_filename_component(_y "${_y}" REALPATH)
  if(BUYUK_KUCUK)
    string(TOLOWER "${_y}" _y)
  endif()
  set(${cikti} "${_y}" PARENT_SCOPE)
endfunction()

function(_ida_kokler girdi cikti)
  set(_l "")
  foreach(_k IN LISTS girdi)
    if(_k STREQUAL "")
      continue()
    endif()
    _ida_yol_normal("${_k}" _n)
    list(APPEND _l "${_n}/")
  endforeach()
  set(${cikti} "${_l}" PARENT_SCOPE)
endfunction()

_ida_kokler("${CEKIRDEK_KOKLERI}" _cekirdek)
_ida_kokler("${SISTEM_KOKLERI}" _sistem)

# Yol köklerden birinin altındaysa kökten sonraki kuyruğu, değilse boş döndürür.
function(_ida_kuyruk yol kokler cikti)
  set(_q "")
  foreach(_k IN LISTS kokler)
    string(FIND "${yol}" "${_k}" _i)
    if(_i EQUAL 0)
      string(LENGTH "${_k}" _u)
      string(SUBSTRING "${yol}" ${_u} -1 _q)
      break()
    endif()
  endforeach()
  set(${cikti} "${_q}" PARENT_SCOPE)
endfunction()

set(_ihlaller "")
set(_toplam 0)
foreach(_nesne IN LISTS NESNELER)
  file(TO_CMAKE_PATH "${_nesne}" _nesne)
  file(RELATIVE_PATH _goreli "${IKILI}" "${_nesne}")
  execute_process(COMMAND "${NINJA}" -C "${IKILI}" -t deps "${_goreli}"
    OUTPUT_VARIABLE _cikti ERROR_VARIABLE _hata RESULT_VARIABLE _rc)
  string(REPLACE "\r" "" _cikti "${_cikti}")
  if(NOT _rc EQUAL 0 OR NOT _cikti MATCHES "\\(VALID\\)")
    message(FATAL_ERROR
      "İDA çekirdek sınırı denetimi çalışmadı: ${_goreli} için geçerli bağımlılık kaydı yok "
      "(derlenmemiş ya da güncel değil; önce derleyin).\n${_cikti}${_hata}")
  endif()
  string(REPLACE "\n" ";" _satirlar "${_cikti}")
  set(_basliklar 0)
  foreach(_s IN LISTS _satirlar)
    if(NOT _s MATCHES "^[ \t]+(.+)$")
      continue()
    endif()
    string(STRIP "${CMAKE_MATCH_1}" _ham)
    if(_ham MATCHES "\\.(c|cc|cpp|cxx)$")
      continue()   # kaynağın kendisi
    endif()
    math(EXPR _basliklar "${_basliklar} + 1")
    _ida_yol_normal("${_ham}" _yol)
    # Kimlik denetimi yalnız kökten sonraki kuyrukta yapılır (deponun ya da SDK'nın
    # bulunduğu dizinin adı yanlış pozitif üretmesin); kök dışı başlık zaten ihlaldir.
    _ida_kuyruk("${_yol}" "${_cekirdek}" _kuyruk)
    set(_tur cekirdek)
    if(_kuyruk STREQUAL "")
      _ida_kuyruk("${_yol}" "${_sistem}" _kuyruk)
      set(_tur sistem)
    endif()
    if(_kuyruk STREQUAL "")
      list(APPEND _ihlaller "${_goreli}: çekirdek ve sistem kökleri dışından başlık: ${_ham}")
      continue()
    endif()
    string(TOLOWER "${_kuyruk}" _kucuk)
    if(_kucuk MATCHES "${_yasak_kimlik}")
      list(APPEND _ihlaller "${_goreli}: yasak kimlikli başlık (${_tur} kökünde): ${_ham}")
    elseif(_kucuk MATCHES "${_yasak_os}")
      list(APPEND _ihlaller "${_goreli}: işletim sistemi API başlığı (${_tur} kökünde): ${_ham}")
    endif()
  endforeach()
  if(_basliklar EQUAL 0)
    message(FATAL_ERROR
      "İDA çekirdek sınırı denetimi çalışmadı: ${_goreli} için hiç başlık kaydı yok.\n${_cikti}")
  endif()
  math(EXPR _toplam "${_toplam} + ${_basliklar}")
endforeach()

if(_ihlaller)
  # Her ihlal kendi satırında, sarılmadan basılır (FATAL_ERROR iletisi satır kaydırır).
  foreach(_i IN LISTS _ihlaller)
    message(NOTICE "İHLAL ${_i}")
  endforeach()
  list(LENGTH _ihlaller _ni)
  message(FATAL_ERROR "İDA çekirdek sınırı ihlali (başlık bağımlılığı): ${_ni} ihlal (yukarıda)")
endif()
list(LENGTH NESNELER _n)
message(STATUS "Çekirdek sınırı (başlık bağımlılıkları): ${_n} nesne, ${_toplam} başlık; ihlal yok")
