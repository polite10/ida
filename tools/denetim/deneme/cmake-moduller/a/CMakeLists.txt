# Modülleri ilk içeren dizin.
include(IdaDenetim)
include(IdaMavlink)

add_library(deneme_a STATIC ../ortak/temiz.c)
ida_bellek_denetimi_ekle(deneme.a.bellek deneme_a)

ida_mavlink_uret(deneme_mavlink_a)
add_library(deneme_a_mavlink OBJECT ../ortak/mavlink_tu.c)
target_link_libraries(deneme_a_mavlink PRIVATE deneme_mavlink_a)
