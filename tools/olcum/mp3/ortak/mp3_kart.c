/* W0-MP3a ölçüm prototipi: kart test mantığı (bkz. mp3_kart.h; atılabilir). */
#include "mp3_kart.h"

#include <string.h>

#define BITTI_BOS_MS 3000u

static const mp3_platform_t *P;
static const char *kimlik_metin;

static int durum;
static mp3_kur_t kur;
static mp3_baud_t baud_bilgi;
static uint32_t kosu;
static uint32_t t_durum;        /* durum girişi (µs) */
static uint32_t t_son_rx;       /* son geçerli çerçeve (µs) */
static uint32_t t0;             /* koşu başlangıcı (ESLE) */
static uint32_t bekleyen_baud;  /* 0 değilse gönderim boşalınca bu hıza geçilir */
static mp3_baud_t bekleyen_bilgi;
static int bekleyen_esleme;     /* geçişten sonra ESLEME'ye girilecek */

/* kalıcı sayaçlar (açılıştan beri; KIMLIK'te) */
static uint32_t esle_zaman_asimi, kosu_kesildi, kur_ret, kosu_sayisi;

/* koşu sayaçları */
static mp3_ayr_t ayr;
static mp3_sira_t sira;
static uint32_t rx_kosu_hata, rx_tur_hata, rx_icerik_hata, rx_kontrol, rx_kontrol_bayt;
static uint32_t bayt_duzelt;    /* pencereyi başlatan ESLE'nin, sayaç sıfırlandıktan sonra tüketilecek baytları */
static uint32_t tx_cerceve, tx_bayt, tx_gec_azami_us, tx_son_us, tx_bekleme, echo_dusen;
static int tx_beklemede;        /* sıradaki planlı çerçeve yer beklerken bir kez sayılır */
static uint32_t sure_us;
static mp3_ozet_t ozet;         /* BITIR'de dondurulur */

/* tek denetim çerçevesi yuvası (yer açılınca gönderilir; veri çerçeveleri bu boşalmadan sıraya girmez) */
static uint8_t kb[MP3_AZAMI_CERCEVE];
static uint32_t kb_n;

static uint8_t gecici[MP3_AZAMI_CERCEVE];
static uint8_t yuk_tampon[MP3_AZAMI_YUK];

static uint32_t simdi(void) { return P->zaman_us(); }
static uint32_t gecen_ms(uint32_t t) { return (uint32_t)(simdi() - t) / 1000u; }

static void durum_gir(int d)
{
    durum = d;
    t_durum = simdi();
}

static void kontrol_gonder(uint8_t tur, uint32_t sr, const uint8_t *yuk, uint16_t n)
{
    /* Önceki denetim çerçevesi hâlâ bekliyorsa yenisi onun yerini alır (aynı tür yinelemeleri zararsızdır). */
    kb_n = (uint32_t)mp3_kodla(kb, sizeof kb, tur, kosu, sr, yuk, n);
}

static void kontrol_pompa(void)
{
    if (kb_n != 0u && P->tx_bos() >= kb_n) {
        P->tx_yaz(kb, kb_n);
        kb_n = 0u;
    }
}

static void kimlik_gonder(uint32_t sr)
{
    char s[MP3_AZAMI_YUK];
    uint32_t n = 0;
    const char *parca[] = { "kimlik=", kimlik_metin, " durum=" };
    for (unsigned i = 0; i < 3u; i++) {
        for (const char *t = parca[i]; *t != '\0' && n < sizeof s - 1u; t++) { s[n++] = *t; }
    }
    if (n < sizeof s - 1u) { s[n++] = (char)('0' + durum); }
    /* sayaçlar */
    const struct { const char *ad; uint32_t v; } sy[] = {
        { " esle_zaman_asimi=", esle_zaman_asimi }, { " kosu_kesildi=", kosu_kesildi },
        { " kur_ret=", kur_ret }, { " kosu_sayisi=", kosu_sayisi } };
    for (unsigned i = 0; i < sizeof sy / sizeof sy[0]; i++) {
        for (const char *t = sy[i].ad; *t != '\0' && n < sizeof s - 1u; t++) { s[n++] = *t; }
        char d[11];
        int k = 0;
        uint32_t v = sy[i].v;
        do { d[k++] = (char)('0' + (char)(v % 10u)); v /= 10u; } while (v != 0u);
        while (k > 0 && n < sizeof s - 1u) { s[n++] = d[--k]; }
    }
    s[n] = '\0';
    if (P->kimlik_ek != NULL) { P->kimlik_ek(s + n, (uint32_t)(sizeof s - n)); }
    n = (uint32_t)strlen(s);
    kontrol_gonder(MP3_T_KIMLIK, sr, (const uint8_t *)s, (uint16_t)n);
}

static void kosu_sayac_sifirla(void)
{
    mp3_ayr_sayac_sifirla(&ayr);
    mp3_sira_sifirla(&sira);
    rx_kosu_hata = rx_tur_hata = rx_icerik_hata = rx_kontrol = rx_kontrol_bayt = 0u;
    tx_cerceve = tx_bayt = tx_gec_azami_us = tx_son_us = tx_bekleme = echo_dusen = 0u;
    tx_beklemede = 0;
    sure_us = 0u;
    mp3_hw_t h;
    P->hw_oku(&h, 1);
}

/* `son`: penceredeki son çerçevenin (BITIR) henüz tüketilmemiş baytları. */
static void ozet_dondur(uint32_t son)
{
    mp3_hw_t h;
    P->hw_oku(&h, 0);
    memset(&ozet, 0, sizeof ozet);
    ozet.kosu = kosu;
    ozet.mod = kur.mod;
    ozet.baud = kur.baud;
    ozet.sure_us = sure_us;
    ozet.rx_bayt = ayr.tuketilen - bayt_duzelt + son;
    ozet.rx_iyi = ayr.iyi;
    ozet.rx_atilan_bayt = ayr.atilan_bayt;
    ozet.rx_baslik_hata = ayr.baslik_hata;
    ozet.rx_surum_hata = ayr.surum_hata;
    ozet.rx_uzunluk_hata = ayr.uzunluk_hata;
    ozet.rx_crc_hata = ayr.crc_hata;
    ozet.rx_kosu_hata = rx_kosu_hata;
    ozet.rx_tur_hata = rx_tur_hata;
    ozet.rx_icerik_hata = rx_icerik_hata;
    ozet.rx_alinan = sira.alinan;
    ozet.rx_beklenen = sira.beklenen;
    ozet.rx_eksik = sira.eksik;
    ozet.rx_tekrar = sira.tekrar;
    ozet.rx_sirasiz = sira.sirasiz;
    ozet.rx_eski = sira.eski;
    ozet.rx_kontrol = rx_kontrol;
    ozet.rx_kontrol_bayt = rx_kontrol_bayt;
    ozet.uart_ore = h.uart_ore;
    ozet.uart_fe = h.uart_fe;
    ozet.uart_ne = h.uart_ne;
    ozet.uart_pe = h.uart_pe;
    ozet.dma_rx_hata = h.dma_rx_hata;
    ozet.dma_tx_hata = h.dma_tx_hata;
    ozet.rx_tasma = h.rx_tasma;
    ozet.rx_tasma_bayt = h.rx_tasma_bayt;
    ozet.rx_halka_azami = h.rx_halka_azami;
    ozet.rx_halka_boy = h.rx_halka_boy;
    ozet.tx_cerceve = tx_cerceve;
    ozet.tx_bayt = tx_bayt;
    ozet.tx_plan = kur.mod == MP3_MOD_AKIS ? kur.kart_adet : 0u;
    ozet.tx_eksik = ozet.tx_plan > tx_cerceve ? ozet.tx_plan - tx_cerceve : 0u;
    ozet.tx_gec_azami_us = tx_gec_azami_us;
    ozet.tx_son_us = tx_son_us;
    ozet.tx_bekleme = tx_bekleme;
    ozet.echo_dusen = echo_dusen;
}

static void ozet_gonder(void)
{
    uint8_t y[MP3_OZET_BOY];
    mp3_ozet_yaz(y, &ozet);
    kontrol_gonder(MP3_T_OZET, 0u, y, (uint16_t)sizeof y);
}

/* Hız geçişini planlar: gönderim (bekleyen denetim çerçevesi dahil) boşalınca uygulanır. */
static void hiz_planla(const mp3_baud_t *b, int esleme)
{
    bekleyen_bilgi = *b;
    bekleyen_baud = b->baud;
    bekleyen_esleme = esleme;
}

static void kontrol_hizina_don(void)
{
    mp3_baud_t b;
    if (P->baud_hesapla(MP3_KONTROL_BAUD, &b) == 0) { hiz_planla(&b, 0); }
    durum_gir(MP3_D_KONTROL);
}

static int kur_gecerli(const mp3_kur_t *k)
{
    if (k->baud < 1200u || k->baud > 20000000u) { return 0; }
    if (k->mod != MP3_MOD_AKIS && k->mod != MP3_MOD_ECHO) { return 0; }
    if (k->yuk > MP3_AZAMI_YUK) { return 0; }
    if (k->mod == MP3_MOD_AKIS && k->kart_adet > 0u && k->aralik_ns < 1000u) { return 0; }
    if (k->mod == MP3_MOD_ECHO && k->kart_adet != 0u) { return 0; }
    if (k->esle_ms < 100u || k->bos_ms < 100u) { return 0; }
    return 1;
}

static void cerceve_kontrol(const mp3_cerceve_t *c)
{
    if (c->tur == MP3_T_SORGU) { kimlik_gonder(c->sira); return; }
    if (c->tur != MP3_T_KUR) { return; }
    mp3_kur_yanit_t y;
    memset(&y, 0, sizeof y);
    mp3_kur_t k;
    memset(&k, 0, sizeof k);
    if (c->uzunluk == MP3_KUR_BOY) { mp3_kur_oku(c->yuk, &k); }
    y.baud = k.baud;
    mp3_baud_t b;
    memset(&b, 0, sizeof b);
    if (c->uzunluk != MP3_KUR_BOY || !kur_gecerli(&k)) {
        y.neden = MP3_RET_AYAR;
    } else if (P->baud_hesapla(k.baud, &b) != 0) {
        y.neden = MP3_RET_BAUD;
    } else {
        y.kabul = 1u;
    }
    y.usart_hz = b.usart_hz;
    y.brr = b.brr;
    y.over8 = b.over8;
    y.hesap_baud = b.hesap_baud;
    y.hata_ppm = b.hata_ppm;
    kosu = c->kosu;
    uint8_t p[MP3_KUR_YANIT_BOY];
    mp3_kur_yanit_yaz(p, &y);
    kontrol_gonder(MP3_T_KUR_YANIT, c->sira, p, (uint16_t)sizeof p);
    if (!y.kabul) { kur_ret++; return; }
    kur = k;
    baud_bilgi = b;
    hiz_planla(&b, 1);
}

static void cerceve_kosu(const mp3_cerceve_t *c)
{
    if (c->kosu != kosu) { rx_kosu_hata++; return; }
    t_son_rx = simdi();
    switch (c->tur) {
    case MP3_T_ESLE:
        rx_kontrol++;
        rx_kontrol_bayt += MP3_EK + (uint32_t)c->uzunluk;
        kontrol_gonder(MP3_T_ESLE_YANIT, c->sira, NULL, 0u);   /* PC yanıtı kaçırıp yinelediyse */
        return;
    case MP3_T_BITIR:
        sure_us = simdi() - t0;
        rx_kontrol++;
        rx_kontrol_bayt += MP3_EK + (uint32_t)c->uzunluk;
        ozet_dondur(MP3_EK + (uint32_t)c->uzunluk);
        ozet_gonder();
        durum_gir(MP3_D_BITTI);
        return;
    case MP3_T_VERI_PK:
    case MP3_T_ECHO_ISTEK: {
        const uint8_t bek = kur.mod == MP3_MOD_AKIS ? (uint8_t)MP3_T_VERI_PK : (uint8_t)MP3_T_ECHO_ISTEK;
        if (c->tur != bek || c->uzunluk != kur.yuk) { rx_tur_hata++; return; }
        if (!mp3_veri_dogru(c->yuk, c->uzunluk, c->tur, c->kosu, c->sira)) { rx_icerik_hata++; return; }
        (void)mp3_sira_isle(&sira, c->sira);
        if (c->tur == MP3_T_ECHO_ISTEK) {
            const uint32_t boy = MP3_EK + (uint32_t)c->uzunluk;
            if (kb_n != 0u || P->tx_bos() < boy) { echo_dusen++; return; }
            memcpy(yuk_tampon, c->yuk, c->uzunluk);
            const size_t n = mp3_kodla(gecici, sizeof gecici, MP3_T_ECHO_YANIT, kosu, c->sira, yuk_tampon, c->uzunluk);
            P->tx_yaz(gecici, (uint32_t)n);
            tx_cerceve++;
            tx_bayt += (uint32_t)n;
            tx_son_us = simdi() - t0;
        }
        return;
    }
    default:
        rx_tur_hata++;
        return;
    }
}

static void cerceve_cb(void *ctx, const mp3_cerceve_t *c)
{
    (void)ctx;
    switch (durum) {
    case MP3_D_KONTROL:
        if (bekleyen_baud == 0u) { cerceve_kontrol(c); }
        return;
    case MP3_D_ESLEME:
        if (bekleyen_baud != 0u) { return; }
        if (c->tur == MP3_T_ESLE && c->kosu == kosu) {
            kosu_sayac_sifirla();
            bayt_duzelt = MP3_EK + (uint32_t)c->uzunluk;   /* bu çerçeve geri çağrıdan sonra tüketilir */
            t0 = simdi();
            t_son_rx = t0;
            kosu_sayisi++;
            kontrol_gonder(MP3_T_ESLE_YANIT, c->sira, NULL, 0u);
            durum_gir(MP3_D_KOSU);
        }
        return;
    case MP3_D_KOSU:
        cerceve_kosu(c);
        return;
    case MP3_D_BITTI:
        if (c->kosu != kosu) { return; }
        if (c->tur == MP3_T_BITIR) { ozet_gonder(); t_durum = simdi(); }
        else if (c->tur == MP3_T_DONUS) { kontrol_hizina_don(); }
        return;
    default:
        return;
    }
}

void mp3_kart_baslat(const mp3_platform_t *p, const char *kimlik)
{
    P = p;
    kimlik_metin = kimlik;
    mp3_hazirla();
    mp3_ayr_sifirla(&ayr);
    mp3_sira_sifirla(&sira);
    kb_n = 0u;
    bekleyen_baud = 0u;
    durum_gir(MP3_D_KONTROL);
}

int mp3_kart_durum(void) { return durum; }

void mp3_kart_besle(const uint8_t *p, uint32_t n)
{
    mp3_ayr_besle(&ayr, p, n, cerceve_cb, NULL);
}

static void akis_gonder(void)
{
    if (kur.mod != MP3_MOD_AKIS || kb_n != 0u) { return; }
    const uint32_t boy = MP3_EK + (uint32_t)kur.yuk;
    while (tx_cerceve < kur.kart_adet) {
        const uint32_t gecen = simdi() - t0;
        const uint64_t plan = (uint64_t)tx_cerceve * kur.aralik_ns / 1000u;   /* µs */
        if ((uint64_t)gecen < plan) { return; }
        if (P->tx_bos() < boy) {
            if (!tx_beklemede) { tx_bekleme++; tx_beklemede = 1; }
            return;
        }
        tx_beklemede = 0;
        const uint64_t gec = (uint64_t)gecen - plan;
        if (gec > tx_gec_azami_us) { tx_gec_azami_us = gec > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)gec; }
        mp3_veri_uret(yuk_tampon, kur.yuk, MP3_T_VERI_KP, kosu, tx_cerceve);
        const size_t n = mp3_kodla(gecici, sizeof gecici, MP3_T_VERI_KP, kosu, tx_cerceve, yuk_tampon, kur.yuk);
        P->tx_yaz(gecici, (uint32_t)n);
        tx_cerceve++;
        tx_bayt += (uint32_t)n;
        tx_son_us = gecen;   /* gecikmeyle AYNI zaman damgası: plan_us ≤ tx_son_us ≤ plan_us + tx_gec_azami_us */
    }
}

void mp3_kart_adim(void)
{
    kontrol_pompa();
    if (bekleyen_baud != 0u) {
        if (kb_n == 0u && P->tx_bosaldi()) {
            P->baud_uygula(&bekleyen_bilgi);
            bekleyen_baud = 0u;
            if (bekleyen_esleme) { bekleyen_esleme = 0; durum_gir(MP3_D_ESLEME); }
        }
        return;
    }
    switch (durum) {
    case MP3_D_ESLEME:
        if (gecen_ms(t_durum) >= kur.esle_ms) { esle_zaman_asimi++; kontrol_hizina_don(); }
        break;
    case MP3_D_KOSU:
        akis_gonder();
        if (gecen_ms(t_son_rx) >= kur.bos_ms) { kosu_kesildi++; kontrol_hizina_don(); }
        break;
    case MP3_D_BITTI:
        if (gecen_ms(t_durum) >= BITTI_BOS_MS) { kontrol_hizina_don(); }
        break;
    default:
        break;
    }
}
