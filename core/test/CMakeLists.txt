# Çekirdek birim testleri (Unity). PC'de CTest ile koşar; firmware kökünde aynı testler
# arm-none-eabi ile bağlanabilir ELF olarak derlenir, kartta koşturma bu işte yok.
# Testler iç başlıkları (core/src) görür: iç durumu doğrudan kurmak gereken yerde yapı
# alanlarına yazarlar (çekirdekte test kipi ya da koşullu derleme yoktur).

ida_unity_kur()

add_library(ida_core_test_destek STATIC destek.c)
target_include_directories(ida_core_test_destek PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(ida_core_test_destek PUBLIC ida_core ida_unity)
ida_ayar_uygula(ida_core_test_destek CEKIRDEK)

# ida_core_birim_testi(<ad> <kaynak>): cekirdek.birim.<ad> testi.
function(ida_core_birim_testi ad kaynak)
  set(_hedef ida_core_test_${ad})
  add_executable(${_hedef} ${kaynak})
  target_link_libraries(${_hedef} PRIVATE ida_core_test_destek ida_core ida_unity)
  ida_ayar_uygula(${_hedef} CEKIRDEK)
  if(CMAKE_CROSSCOMPILING)
    # Kartta: firmware'in başlangıç dosyası, bağlayıcı betiği ve test çıkışı (firmware/);
    # float aşkın işlevleri newlib libm'dedir.
    ida_fw_elf_kur(${_hedef})
    target_link_libraries(${_hedef} PRIVATE ida_fw_test_destek m)
  else()
    add_test(NAME cekirdek.birim.${ad} COMMAND ${_hedef})
  endif()
endfunction()

ida_core_birim_testi(iskelet test_iskelet.c)
ida_core_birim_testi(zaman_mat test_zaman_mat.c)
ida_core_birim_testi(param test_param.c)
ida_core_birim_testi(param_hiz test_param_hiz.c)
ida_core_birim_testi(cerceve test_cerceve.c)
ida_core_birim_testi(emniyet test_emniyet.c)
ida_core_birim_testi(ornek test_ornek.c)
ida_core_birim_testi(komut test_komut.c)
ida_core_birim_testi(olay test_olay.c)
ida_core_birim_testi(arka_plan test_arka_plan.c)
ida_core_birim_testi(belirlenimcilik test_belirlenimcilik.c)
ida_core_birim_testi(kestirim kestirim/test_kestirim.c)
ida_core_birim_testi(kontrol_tahsis kontrol/test_tahsis.c)
ida_core_birim_testi(kontrol_dongu kontrol/test_dongu.c)
ida_core_birim_testi(kontrol_guvenli kontrol/test_guvenli.c)
ida_core_birim_testi(kontrol_sol_bulgulari kontrol/test_sol_bulgulari.c)
ida_core_birim_testi(kontrol_dalis_yasak kontrol/test_dalis_yasak.c)
ida_core_birim_testi(gorev_kip_ariza gorev/test_gorev_kip_ariza.c)
ida_core_birim_testi(gorev_arm gorev/test_gorev_arm.c)
ida_core_birim_testi(gorev_degismez gorev/test_gorev_degismez.c)
ida_core_birim_testi(gorev_sol gorev/test_gorev_sol.c)
ida_core_birim_testi(gudum gudum/test_gudum.c)
ida_core_birim_testi(gudum_gecis gudum/test_gudum_gecis.c)
ida_core_birim_testi(gorev_cokme gorev/test_gorev_cokme.c)
# W5-gorev-yurutucu: görev yüklemesi, durum makinesi, öğe yürütücüleri, AUTO güdümü.
ida_core_birim_testi(gorev_yukle gorev/test_gorev_yukle.c)
ida_core_birim_testi(gorev_yurut gorev/test_gorev_yurut.c)
ida_core_birim_testi(gudum_oto gudum/test_gudum_oto.c)
# W5-atis-emniyet: atış kontrol emniyet zinciri (core/test/atis/).
include("${CMAKE_CURRENT_SOURCE_DIR}/atis/testler.cmake")

# ---- A4-K22: çekirdek nesnelerinde yazılabilir veri bölümüne düşen simge yok --------------
# Sanitizer derlemesi araç simgeleri (ASan global tanımları) eklediği için orada kurulmaz.
if(NOT IDA_SANITIZER AND NOT MSVC)
  if(CMAKE_CROSSCOMPILING)
    set(_bicim elf)
  else()
    set(_bicim macho)
    if(NOT APPLE)
      set(_bicim elf)
    endif()
  endif()
  add_test(NAME cekirdek.yazilabilir_veri
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DARSIV=$<TARGET_FILE:ida_core>"
            "-DBICIM=${_bicim}" -P "${CMAKE_CURRENT_SOURCE_DIR}/yazilabilir_veri.cmake")
  set_tests_properties(cekirdek.yazilabilir_veri PROPERTIES
    PASS_REGULAR_EXPRESSION "yazılabilir veri simgesi yok")
  # Karşı örnek: değişebilir statik nesne taşıyan deneme arşivi yakalanır.
  add_library(ida_core_deneme_yazilabilir STATIC deneme/yazilabilir.c)
  add_test(NAME cekirdek.yazilabilir_veri.deneme_yakalanir
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}"
            "-DARSIV=$<TARGET_FILE:ida_core_deneme_yazilabilir>"
            "-DBICIM=${_bicim}" -P "${CMAKE_CURRENT_SOURCE_DIR}/yazilabilir_veri.cmake")
  set_tests_properties(cekirdek.yazilabilir_veri.deneme_yakalanir PROPERTIES
    PASS_REGULAR_EXPRESSION "yazılabilir veri simgesi:.*ida_deneme_sayac")

  # A4-K12, K21, K97: dış başvurular yalnız izinli listede (saat, ayırma, platform işlevi yok).
  add_test(NAME cekirdek.dis_basvurular
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DARSIV=$<TARGET_FILE:ida_core>"
            "-DBICIM=${_bicim}" -P "${CMAKE_CURRENT_SOURCE_DIR}/dis_basvurular.cmake")
  set_tests_properties(cekirdek.dis_basvurular PROPERTIES
    PASS_REGULAR_EXPRESSION "dış başvurular izinli listede")
  add_library(ida_core_deneme_saat STATIC deneme/saat_okuyan.c)
  add_test(NAME cekirdek.dis_basvurular.deneme_yakalanir
    COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DARSIV=$<TARGET_FILE:ida_core_deneme_saat>"
            "-DBICIM=${_bicim}" -P "${CMAKE_CURRENT_SOURCE_DIR}/dis_basvurular.cmake")
  set_tests_properties(cekirdek.dis_basvurular.deneme_yakalanir PROPERTIES
    PASS_REGULAR_EXPRESSION "izinsiz dış başvuru: clock")

  # A4-K07: ölçüm zamanını duvar saatine atayan kod derlenmez.
  add_test(NAME cekirdek.k07_tip_ayrimi
    COMMAND "${CMAKE_C_COMPILER}" -std=c17 -fsyntax-only
            "-I${CMAKE_CURRENT_SOURCE_DIR}/../include"
            "${CMAKE_CURRENT_SOURCE_DIR}/deneme/tip_karisimi.c")
  set_tests_properties(cekirdek.k07_tip_ayrimi PROPERTIES
    PASS_REGULAR_EXPRESSION "incompatible type")
endif()
