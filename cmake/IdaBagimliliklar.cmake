# İDA'nın CPM ile alınan bağımlılıkları (W2-depo §3). Her biri commit ve SHA-256 ile sabittir;
# hareketli etiket yoktur. vcpkg ile alınanlar kökteki vcpkg.json'dadır.
#
#   ida_unity_kur()     -> ida_unity (statik; C birim test düzeneği)
#   ida_pymavlink_kur() -> IDA_PYMAVLINK_KOK (MAVLink kod üretecinin kaynak dizini)

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

# Unity v2.7.0 (ThrowTheSwitch/Unity, MIT), commit b6763fbd...; arşiv GitHub'ın commit
# arşividir, özet bu iş sırasında indirilen arşivden alındı.
set(IDA_UNITY_COMMIT "b6763fbd9cedfacaa89e2ad9fd00d615a234e355")
set(IDA_UNITY_SHA256 "50edf40d4c433cfb7935a3de0d8a3c1f497576fdd472acf48bf84507651025fc")

# pymavlink 2.4.50 (PyPI kaynak dağıtımı, LGPL-3.0); yalnız derleme zamanında kod üreteci
# olarak koşar (iskelet §7), ürüne girmez. A1 sözleşmesinin doğrulaması, altın baytları ve
# CRC_EXTRA değerleri bu sürümle üretildi (A1 §3.1, §3.2, §7.3). Özet PyPI'nin yayımladığı
# değerdir; indirilen arşivden de doğrulandı.
set(IDA_PYMAVLINK_SURUMU "2.4.50")
set(IDA_PYMAVLINK_URL
  "https://files.pythonhosted.org/packages/3c/4e/212af84fdb81b62e934faf1ae579cc5ffbb32d2756e9c7c63feff9a62877/pymavlink-2.4.50.tar.gz")
set(IDA_PYMAVLINK_SHA256 "8d65a8b05b2971babeef4b6ca74a247a72276f62f7559466c64e96ece7f10062")

function(ida_unity_kur)
  if(TARGET ida_unity)
    return()
  endif()
  CPMAddPackage(
    NAME unity
    URL "https://github.com/ThrowTheSwitch/Unity/archive/${IDA_UNITY_COMMIT}.tar.gz"
    URL_HASH SHA256=${IDA_UNITY_SHA256}
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
    target_compile_definitions(ida_unity PUBLIC UNITY_INCLUDE_CONFIG_H)
    target_include_directories(ida_unity SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/firmware/test_destek")
  endif()
endfunction()

function(ida_pymavlink_kur)
  if(DEFINED IDA_PYMAVLINK_KOK)
    return()
  endif()
  CPMAddPackage(
    NAME pymavlink
    URL "${IDA_PYMAVLINK_URL}"
    URL_HASH SHA256=${IDA_PYMAVLINK_SHA256}
    DOWNLOAD_ONLY YES)
  set(IDA_PYMAVLINK_KOK "${pymavlink_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
