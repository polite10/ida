# İDA'nın CPM ile alınan bağımlılıkları (W2-depo §3). Her biri commit ve SHA-256 ile sabittir;
# hareketli etiket yoktur. vcpkg ile alınanlar kökteki vcpkg.json'dadır.
#
#   ida_unity_kur()     -> ida_unity (statik; C birim test düzeneği)
#   ida_pymavlink_kur() -> IDA_PYMAVLINK_KOK (çağıranın kapsamında) ve aynı değer genel
#                          IDA_PYMAVLINK_KOK özelliğinde; IDA_PYMAVLINK_SURUMU de öyle.
#
# İşlevler herhangi bir dizinden, birden çok kez çağrılabilir: sürüm sabitleri işlevlerin
# içindedir, yollar işlevin kendi dosyasının yerinden çözülür, "zaten kuruldu" bilgisi
# hedefte ya da genel özellikte tutulur (modül düzeyindeki normal değişkenler
# include_guard yüzünden yalnız modülü ilk içeren dizinde görünür).
# CPM.cmake kök dizinden içerilmelidir (CPM'in kendi durumu için; kök CMakeLists.txt yapar).

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

function(ida_unity_kur)
  if(TARGET ida_unity)
    return()
  endif()
  # Unity v2.7.0 (ThrowTheSwitch/Unity, MIT), commit b6763fbd...; arşiv GitHub'ın commit
  # arşividir, özet bu iş sırasında indirilen arşivden alındı.
  set(_commit "b6763fbd9cedfacaa89e2ad9fd00d615a234e355")
  set(_ozet "50edf40d4c433cfb7935a3de0d8a3c1f497576fdd472acf48bf84507651025fc")
  CPMAddPackage(
    NAME unity
    URL "https://github.com/ThrowTheSwitch/Unity/archive/${_commit}.tar.gz"
    URL_HASH SHA256=${_ozet}
    DOWNLOAD_ONLY YES)
  add_library(ida_unity STATIC "${unity_SOURCE_DIR}/src/unity.c")
  # Üçüncü taraf kodu: İDA uyarı politikası uygulanmaz; başlıklar sistem başlığı sayılır.
  target_include_directories(ida_unity SYSTEM PUBLIC "${unity_SOURCE_DIR}/src")
  set_target_properties(ida_unity PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON)
  if(MSVC)
    target_compile_options(ida_unity PRIVATE /W0 /utf-8)
  else()
    target_compile_options(ida_unity PRIVATE -w)
  endif()
  if(CMAKE_CROSSCOMPILING)
    # Kartta stdio yok: çıktı firmware'in test çıkış işlevine gider (firmware/test_destek).
    get_filename_component(_destek "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../firmware/test_destek" ABSOLUTE)
    target_compile_definitions(ida_unity PUBLIC UNITY_INCLUDE_CONFIG_H)
    target_include_directories(ida_unity SYSTEM PUBLIC "${_destek}")
  endif()
endfunction()

function(ida_pymavlink_kur)
  get_property(_kok GLOBAL PROPERTY IDA_PYMAVLINK_KOK)
  if(NOT _kok)
    # pymavlink 2.4.50 (PyPI kaynak dağıtımı, LGPL-3.0); yalnız derleme zamanında kod
    # üreteci olarak koşar (iskelet §7), ürüne girmez. A1 sözleşmesinin doğrulaması, altın
    # baytları ve CRC_EXTRA değerleri bu sürümle üretildi (A1 §3.1, §3.2, §7.3). Özet
    # PyPI'nin yayımladığı değerdir; indirilen arşivden de doğrulandı.
    set(_surum "2.4.50")
    CPMAddPackage(
      NAME pymavlink
      URL "https://files.pythonhosted.org/packages/3c/4e/212af84fdb81b62e934faf1ae579cc5ffbb32d2756e9c7c63feff9a62877/pymavlink-2.4.50.tar.gz"
      URL_HASH SHA256=8d65a8b05b2971babeef4b6ca74a247a72276f62f7559466c64e96ece7f10062
      DOWNLOAD_ONLY YES)
    set(_kok "${pymavlink_SOURCE_DIR}")
    set_property(GLOBAL PROPERTY IDA_PYMAVLINK_KOK "${_kok}")
    set_property(GLOBAL PROPERTY IDA_PYMAVLINK_SURUMU "${_surum}")
  endif()
  get_property(_surum GLOBAL PROPERTY IDA_PYMAVLINK_SURUMU)
  set(IDA_PYMAVLINK_KOK "${_kok}" PARENT_SCOPE)
  set(IDA_PYMAVLINK_SURUMU "${_surum}" PARENT_SCOPE)
endfunction()
