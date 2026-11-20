# Deneme projesi: cmake/ modüllerinin işlevleri, modülü ilk içeren dizinden başka bir
# dizinden çağrıldığında da çalışır (include_guard(GLOBAL) tuzağı; W2-depo düzeltmesi).
#
# Kök, gerçek kök CMakeLists.txt gibi yalnız IdaBagimliliklar'ı içerir; IdaDenetim ve
# IdaMavlink'i iki kardeş alt dizin (a/, b/) ayrı ayrı içerir. İkinci içerme include_guard
# yüzünden boştur; b/'deki çağrılar modül düzeyindeki değişkenleri görmez.
#
# Kipler (IDA_DENEME_KIPI):
#   iki_dizin : a/ ve b/ ida_bellek_denetimi_ekle ve ida_mavlink_uret çağırır (b/ ayrıca
#               a/'nın hedef adıyla yineler); yapılandırma, derleme ve iç testler geçmeli.
#   bos_yol   : bellek ayırma betiğinin yolu boş verilir; yapılandırma hata vermeli.
# tests/CMakeLists.txt bu projeyi CTest ile iki kipte koşturur.
cmake_minimum_required(VERSION 3.25)
project(ida_modul_denemesi LANGUAGES C)

if(NOT IDA_KOK OR NOT IDA_DENEME_KIPI)
  message(FATAL_ERROR "IDA_KOK ve IDA_DENEME_KIPI verilmeli")
endif()
list(APPEND CMAKE_MODULE_PATH "${IDA_KOK}/cmake")
include(IdaBagimliliklar)
enable_testing()

if(IDA_DENEME_KIPI STREQUAL "bos_yol")
  set(IDA_BELLEK_DENETIMI_BETIGI "")
elseif(NOT IDA_DENEME_KIPI STREQUAL "iki_dizin")
  message(FATAL_ERROR "Bilinmeyen kip: ${IDA_DENEME_KIPI}")
endif()

add_subdirectory(a)
add_subdirectory(b)
