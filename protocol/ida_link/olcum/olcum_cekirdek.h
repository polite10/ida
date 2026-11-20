/*
 * olcum_cekirdek.h — ida_link ayrıştırma ve kodlama maliyeti ölçümünün çekirdek döngüsü
 * (W2-protokol §5).
 *
 * Saat, işletim sistemi ya da stdio kullanmaz: süreyi çağıran ölçer (PC'de olcum_host.c;
 * kartta MP5/W3 işinde DWT sayacıyla aynı işlevler çağrılabilir). Sonuçlar yalnız ölçümün
 * yapıldığı makineye aittir; bir makineden ötekine aktarılmaz.
 *
 * Yük: (1) bir sim -> kart penceresi dolusu tipik çerçeve (17 çerçeve, 1 190 B; A1 3.1'in
 * B_pencere = 1 200 B sınırının altında): TF, IMU, MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK,
 * SAFETY, PING360_HEAD, iki kırpılmamış PING360_DATA (258 B), DET_BATCH, DET_RECORD,
 * CMD_SIMPLE, KEEPALIVE; alan değerleri A1 Ek B'den. (2) Dialect'in en büyük çerçevesi
 * (kırpılmamış PING360_DATA, 258 B).
 */
#ifndef IDA_LINK_OLCUM_CEKIRDEK_H
#define IDA_LINK_OLCUM_CEKIRDEK_H

#include "ida_link/tel.h"

#include <stddef.h>
#include <stdint.h>

#define IDA_LINK_OLCUM_PENCERE_AZAMI 1200u
#define IDA_LINK_OLCUM_CERCEVE_SAYISI 17u
#define IDA_LINK_OLCUM_YAPI_AZAMI 256u

typedef struct {
    uint32_t msgid;
    uint16_t yapi_boyu;
    union {
        uint64_t hiza;
        uint8_t b[IDA_LINK_OLCUM_YAPI_AZAMI];
    } yapi;
} ida_link_olcum_mesaj_t;

typedef struct {
    ida_link_olcum_mesaj_t mesaj[IDA_LINK_OLCUM_CERCEVE_SAYISI];
    uint8_t pencere[IDA_LINK_OLCUM_PENCERE_AZAMI];
    size_t pencere_bayt;
    uint8_t buyuk[IDA_LINK_CERCEVE_AZAMI];
    size_t buyuk_bayt;
    uint8_t cikis[IDA_LINK_OLCUM_PENCERE_AZAMI];
    ida_link_alici_t alici;
    ida_link_gonderici_t gonderici;
    ida_link_cerceve_t cerceve;
} ida_link_olcum_t;

/* Yükü kurar. Dönüş: 0 başarılı; negatif: kurulum hatası (altın veri ya da kitaplık). */
int ida_link_olcum_hazirla(ida_link_olcum_t *o);

/* Her biri `yineleme` kez çalışır; dönüş, iyileştiricinin işi silmemesi için bir özettir.
   Ayrıştırmada ayrıca teslim edilen çerçeve sayısı denetlenir: beklenen değilse 0 döner. */
uint32_t ida_link_olcum_pencere_ayristir(ida_link_olcum_t *o, uint32_t yineleme);
uint32_t ida_link_olcum_pencere_kodla(ida_link_olcum_t *o, uint32_t yineleme);
uint32_t ida_link_olcum_buyuk_ayristir(ida_link_olcum_t *o, uint32_t yineleme);
uint32_t ida_link_olcum_buyuk_kodla(ida_link_olcum_t *o, uint32_t yineleme);

#endif /* IDA_LINK_OLCUM_CEKIRDEK_H */
