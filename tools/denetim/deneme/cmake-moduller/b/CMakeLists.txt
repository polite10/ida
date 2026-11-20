# İkinci dizin: içermeler include_guard yüzünden boştur; işlevler yine çalışmalı.
include(IdaDenetim)
include(IdaMavlink)

add_library(deneme_b STATIC ../ortak/temiz.c)
ida_bellek_denetimi_ekle(deneme.b.bellek deneme_b)

# Aynı üretilmiş başlıklar: yeni bir ad ve a/'nın adıyla yinelenen çağrı (ikisi de
# ikinci bir üretim komutu tanımlamaz).
ida_mavlink_uret(deneme_mavlink_b)
ida_mavlink_uret(deneme_mavlink_a)
add_library(deneme_b_mavlink OBJECT ../ortak/mavlink_tu.c)
target_link_libraries(deneme_b_mavlink PRIVATE deneme_mavlink_b)
