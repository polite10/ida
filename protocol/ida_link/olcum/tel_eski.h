/*
 * tel_eski.h — W3-tel-hiz öncesi tel katmanının açık işlevleri, ida_link_eski_ önekiyle.
 *
 * Uygulama tel_eski.c'dedir: protocol/ida_link/src/tel.c'nin W3-tel-hiz öncesi hâlinin
 * (main @ 920b717) değiştirilmemiş kopyası. Yalnız iki iş içindir: eski ve yeni
 * ayrıştırıcının aynı girdide aynı sonucu verdiğini gösteren fark testi (tel_fark.c) ve
 * önce/sonra ölçümü (olcum_cekirdek.c). Ürüne bağlanmaz; ida_link kitaplığına girmez.
 *
 * Tipler ve sabitler ida_link/tel.h'dekilerdir (örnek yapıları ortak; iki uygulama aynı
 * yapı üzerinde çalışır). Her işlevin anlamı tel.h'deki aynı adlı (öneksiz) işlevinkidir.
 */
#ifndef IDA_LINK_TEL_ESKI_H
#define IDA_LINK_TEL_ESKI_H

#include "ida_link/tel.h"

#ifdef __cplusplus
extern "C" {
#endif

ida_link_rc_t ida_link_eski_alici_kur(ida_link_alici_t *a, ida_link_rol_t rol);
ida_link_rc_t ida_link_eski_al(ida_link_alici_t *a, const uint8_t *veri, size_t n,
                               size_t *tuketilen, ida_link_cerceve_t *cerceve);
ida_link_rc_t ida_link_eski_alici_tasma(ida_link_alici_t *a);
ida_link_rc_t ida_link_eski_gonderici_kur(ida_link_gonderici_t *g, ida_link_rol_t rol);
ida_link_rc_t ida_link_eski_gonder(ida_link_gonderici_t *g, uint32_t msgid,
                                   const void *mesaj, size_t mesaj_boyu,
                                   uint8_t *cikis, size_t kapasite, size_t *yazilan);
ida_link_rc_t ida_link_eski_coz(const ida_link_cerceve_t *cerceve, uint32_t msgid,
                                void *mesaj, size_t mesaj_boyu);
ida_link_rc_t ida_link_eski_mesaj_bilgisi(uint32_t msgid, ida_link_mesaj_bilgisi_t *b);

#ifdef __cplusplus
}
#endif

#endif /* IDA_LINK_TEL_ESKI_H */
