# tests/kabul/protokol — ida_link tel katmanının bağımsız kabul testleri (W2-protokol, kabul
# bölümünün son paragrafı; yazan ida-test-yazari).
#
# Testler yalnız sözleşmeden (docs/sozlesmeler/A1-kart-baglantisi.md), görev belgesinden ve
# açık başlıktan (protocol/ida_link/include/ida_link/tel.h) türetildi; uygulama kaynağı,
# uygulayıcının birim testleri ve veri dosyaları kullanılmaz. Ek B'nin 38 altın çerçevesi,
# Tablo 3.3 ve ida.xml'in alan tipleri derleme sırasında bu dizindeki betikle çıkarılır.
#
# CTest adları kural numarasını taşır: kabul.protokol.A1-K02 … A1-K08, A1-EkB, A1-K101,
# A1-3.11 (sayaç genişliği), A1-K07.saglamlik (sözde rastgele akışlar), belirsiz (A1'in iki
# biçimde okunabildiği vakalar: gözlenen davranış iletiyle yazılır, test kırmızı olmaz).
#
# Yalnız PC kökünde derlenir (tests/ yalnız host'ta eklenir).

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
ida_unity_kur()

set(_kabul_betik "${CMAKE_CURRENT_SOURCE_DIR}/ek_b_cikar.py")
set(_kabul_sozlesme "${PROJECT_SOURCE_DIR}/docs/sozlesmeler/A1-kart-baglantisi.md")
set(_kabul_xml "${PROJECT_SOURCE_DIR}/protocol/ida.xml")
set(_kabul_u "${CMAKE_CURRENT_BINARY_DIR}/uretilen")

add_custom_command(
  OUTPUT "${_kabul_u}/kabul_ek_b.c" "${_kabul_u}/kabul_ek_b.h"
  COMMAND "${Python3_EXECUTABLE}" -I "${_kabul_betik}"
          --sozlesme "${_kabul_sozlesme}" --xml "${_kabul_xml}"
          --cikti-c "${_kabul_u}/kabul_ek_b.c" --cikti-h "${_kabul_u}/kabul_ek_b.h"
  DEPENDS "${_kabul_betik}" "${_kabul_sozlesme}" "${_kabul_xml}"
  COMMENT "kabul: A1 Ek B, Tablo 3.3 ve ida.xml alanları çıkarılıyor (bağımsız betik)"
  VERBATIM)
add_custom_target(kabul_protokol_uret DEPENDS "${_kabul_u}/kabul_ek_b.c" "${_kabul_u}/kabul_ek_b.h")

add_library(kabul_protokol_ortak STATIC kabul_ortak.c "${_kabul_u}/kabul_ek_b.c")
target_include_directories(kabul_protokol_ortak PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${_kabul_u}")
# ida_link ve ida_mavlink_uretilen protocol/ida_link'te (bu dizinden sonra) tanımlanır; ad
# üretim anında çözülür.
target_link_libraries(kabul_protokol_ortak PUBLIC ida_link ida_mavlink_uretilen ida_unity)
add_dependencies(kabul_protokol_ortak kabul_protokol_uret ida_link_uret)
ida_ayar_uygula(kabul_protokol_ortak CEKIRDEK)

function(kabul_protokol_testi ad kaynak)
  set(_hedef "kabul_protokol_${kaynak}")
  add_executable(${_hedef} "${kaynak}.c")
  target_link_libraries(${_hedef} PRIVATE kabul_protokol_ortak)
  ida_ayar_uygula(${_hedef} CEKIRDEK)
  add_test(NAME "kabul.protokol.${ad}" COMMAND ${_hedef})
endfunction()

kabul_protokol_testi(A1-K02            kabul_k02)
kabul_protokol_testi(A1-K04            kabul_k04)
kabul_protokol_testi(A1-K05            kabul_k05)
kabul_protokol_testi(A1-K06            kabul_k06)
kabul_protokol_testi(A1-K07            kabul_k07)
kabul_protokol_testi(A1-K08            kabul_k08)
kabul_protokol_testi(A1-3.11           kabul_sayac)
kabul_protokol_testi(A1-EkB            kabul_ek_b)
kabul_protokol_testi(A1-K07.saglamlik  kabul_saglamlik)
kabul_protokol_testi(A1-K101           kabul_k101)
kabul_protokol_testi(belirsiz          kabul_belirsiz)
