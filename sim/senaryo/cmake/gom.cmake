# Bir dosyayı C++ bayt dizisi olarak gömer (derleme zamanında; cmake -P ile çalışır).
#
#   cmake -DGIRDI=<dosya> -DCIKTI=<.cpp> -DAD=<simge> -P gom.cmake
#
# Üretilen kaynak `ida::senaryo::gomulu` ad alanında iki simge tanımlar:
#   extern const unsigned char <AD>[];      dosyanın ham baytları (sonda ek NUL yok)
#   extern const std::size_t <AD>_boyu;     bayt sayısı
# Bayt dizisi kullanılır: MSVC dizgi değişmezlerinin toplam boyu 64 KB ile sınırlıdır (C2026).

if(NOT DEFINED GIRDI OR NOT DEFINED CIKTI OR NOT DEFINED AD)
  message(FATAL_ERROR "gom.cmake: GIRDI, CIKTI ve AD verilmeli")
endif()

file(READ "${GIRDI}" _hex HEX)
string(LENGTH "${_hex}" _hex_uzunluk)
math(EXPR _boyu "${_hex_uzunluk} / 2")

# Her baytı "0x..," biçimine çevir; satır başına 32 bayt (derleyici dostu satır uzunluğu).
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _dizi "${_hex}")
string(REPEAT "0x[0-9a-f][0-9a-f]," 32 _satir_deseni)
string(REGEX REPLACE "(${_satir_deseni})" "\\1\n" _dizi "${_dizi}")
if(_boyu EQUAL 0)
  set(_dizi "0x00,")
endif()

set(_metin "// Üretildi: sim/senaryo/cmake/gom.cmake. Elle değiştirmeyin.\n")
string(APPEND _metin "#include <cstddef>\n\nnamespace ida::senaryo::gomulu {\n\n")
string(APPEND _metin "extern const unsigned char ${AD}[];\nextern const std::size_t ${AD}_boyu;\n\n")
string(APPEND _metin "const unsigned char ${AD}[] = {\n${_dizi}\n};\n")
string(APPEND _metin "const std::size_t ${AD}_boyu = ${_boyu};\n\n}  // namespace ida::senaryo::gomulu\n")

# İçerik değişmediyse dosyaya dokunma (gereksiz yeniden derleme olmasın).
if(EXISTS "${CIKTI}")
  file(READ "${CIKTI}" _eski)
  if(_eski STREQUAL _metin)
    return()
  endif()
endif()
file(WRITE "${CIKTI}" "${_metin}")
