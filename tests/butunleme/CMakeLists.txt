# tests/butunleme — sim sunucusunun bütünleme testleri (W4-kopru): Z1'in A1 üzerinden koşusu
# (süreç içi karşı taraf ve iki süreç: ida_sim kitaplığı + ida_sil), belirlenimcilik, acil
# durdurma engeli ve HIL 1x'in sözde uçbirimle kartsız sınaması.
#
# Fizik Eigen'e (vcpkg) bağlıdır; vcpkg etkin değilse dizin atlanır (tests/zincir ile aynı).
# ida_sunucu ve ida_sil sim/ altında, tests/'ten sonra tanımlanır; hedef adları üretimde çözülür.

if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
  message(STATUS "vcpkg etkin değil (VCPKG_ROOT yok): tests/butunleme atlandı")
  return()
endif()

add_executable(ida_butunleme_test test_butunleme.cpp)
target_link_libraries(ida_butunleme_test PRIVATE ida_sunucu ida_zincir ida_core)
ida_ayar_uygula(ida_butunleme_test)
if(NOT MSVC)
  target_compile_options(ida_butunleme_test PRIVATE -Wshadow)
endif()

add_test(NAME butunleme.z1_sil_ic COMMAND ida_butunleme_test z1_sil_ic)
add_test(NAME butunleme.z1_belirlenim COMMAND ida_butunleme_test z1_belirlenim)
add_test(NAME butunleme.acil_durdurma COMMAND ida_butunleme_test acil_durdurma)
add_test(NAME butunleme.z1_sil_tcp COMMAND ida_butunleme_test z1_sil_tcp $<TARGET_FILE:ida_sil>)
set_tests_properties(butunleme.z1_sil_tcp PROPERTIES TIMEOUT 300)
if(NOT WIN32)
  # Sözde uçbirim yalnız POSIX'te; gerçek zamanlı 60 s (zamanlamaya duyarlı: ağır yük altında
  # koşturulmaz; ölçümün eşiği yoktur).
  add_test(NAME butunleme.hil_pty COMMAND ida_butunleme_test hil_pty)
  set_tests_properties(butunleme.hil_pty PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
  add_test(NAME butunleme.hil_cikarma_takma COMMAND ida_butunleme_test hil_cikarma_takma)
  set_tests_properties(butunleme.hil_cikarma_takma PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
endif()
