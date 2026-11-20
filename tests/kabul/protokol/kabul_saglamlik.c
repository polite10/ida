/*
 * Sağlamlık kabul testleri (A1-K07 §7.1 satırı, A1-K02/K04–K08 değişmezleri; görev: sabit
 * tohumlu sözde rastgele akışlar, her parçalamanın aynı sonucu vermesi, tanımsız davranış
 * yokluğu — host-asan ön ayarında ASan + UBSan ile derlenir).
 *
 *   (a) parçalama değişmezliği: tek çağrı, tek bayt, sabit k ∈ {2, 3, 7, 13, 64, 255, 256,
 *       1000}, sıfır uzunluklu çağrılar içeren rastgele parçalama (8 tohum) ve kısa akışlarda
 *       her iki parçaya bölme noktası aynı teslim dizisini ve aynı sayaçları verir;
 *   (b) her teslim sözleşmeye uygundur: karşı tarafın kimliği (K04), rolün yönü (K06),
 *       1 <= len <= azami ve sıfır dolgu (K05), akışta bayt bayt bulunan geçerli bir çerçeve
 *       (incompat_flags = 0, CRC tutar; K02, K07); rx_frames teslim sayısıdır; rx_bytes akış
 *       boyudur;
 *   (c) gömülü çerçeve: önündeki 267 baytta 0xFD ve 0xFE bulunmayan her gömülü geçerli
 *       çerçeve (o noktaya uzanan bir aday olamayacağından ayrıştırıcı boştadır) teslim
 *       edilir; teslim edilenler gömülü çerçevelerin sıralı bir alt dizisidir;
 *   (d) her akışta (düz rastgele, 0xFD yoğun, gömülü, temiz + bozuk/kesik, karışık; sonuna
 *       temiz kuyruk eklenerek her aday akış içinde biter) teslimler ve BÜTÜN sayaçlar A1'in
 *       güncel K02/K04–K08 metninden yazılmış referans modelle (kabul_model_ayristir) tam
 *       eşittir; temiz + bozuk akışlarda bu, K07 §7.1'in "bozulan adayın kapladığı bölgeden
 *       sonraki ilk geçerli çerçevenin yakalanması" sınamasıdır.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define AKIS_AZAMI 40000u

static uint8_t akis[AKIS_AZAMI];
static kabul_gomulu_t gomulu[KABUL_GOMULU_AZAMI];
static kabul_sonuc_t tek;
static kabul_sonuc_t diger;
static ida_link_alici_t alici;

static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};

static void parcali(ida_link_rol_t rol, const uint8_t *s, size_t n, kabul_parcalayici_t *p,
                    kabul_sonuc_t *o)
{
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, rol));
    kabul_sonuc_sifirla(o);
    kabul_besle(&alici, s, n, p, o);
}

/* (b) */
static void teslimler_gecerli(ida_link_rol_t rol, const uint8_t *s, size_t n, const kabul_sonuc_t *o,
                              const char *baglam)
{
    char ileti[160];
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)o->sayi, o->sayac.rx_frames, baglam);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)n, o->sayac.rx_bytes, baglam);
    size_t ara = 0; /* teslimler akış sırasındadır: arama bir öncekinin sonrasından sürer */
    for (size_t i = 0; i < o->sayi; ++i) {
        const kabul_teslim_t *t = &o->t[i];
        const kabul_mesaj_t *m = kabul_mesaj_al(t->msgid);
        snprintf(ileti, sizeof ileti, "%s: %u. teslim msgid %lu", baglam, (unsigned)i, (unsigned long)t->msgid);
        TEST_ASSERT_TRUE_MESSAGE((m->yon & kabul_rol_alir_yon(rol)) != 0u, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(kabul_karsi_sysid(rol), t->sysid, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(kabul_karsi_compid(rol), t->compid, ileti);
        TEST_ASSERT_TRUE_MESSAGE(t->len >= 1u && t->len <= m->yuk_azami, ileti);
        /* Akışta bu teslimin çerçevesi (compat_flags 0 ile) bayt bayt bulunur. */
        uint8_t c[IDA_LINK_CERCEVE_AZAMI];
        size_t cn = kabul_cerceve_ham(c, t->len, 0u, 0u, t->seq, t->sysid, t->compid, t->msgid, t->yuk,
                                      t->len, m->crc_extra);
        int bulundu = 0;
        for (size_t k = ara; k + cn <= n; ++k) {
            if (s[k] == 0xFDu && memcmp(&s[k], c, cn) == 0) {
                bulundu = 1;
                ara = k + cn;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(bulundu, ileti);
    }
}

/* (a) */
static void parcalama_degismez(ida_link_rol_t rol, const uint8_t *s, size_t n, const char *baglam,
                               int her_bolme)
{
    char ileti[160];
    kabul_parcalayici_t p = {KABUL_PARCA_TUMU, 0u, {0u}};
    parcali(rol, s, n, &p, &tek);
    teslimler_gecerli(rol, s, n, &tek, baglam);
    static const size_t K[] = {1u, 2u, 3u, 7u, 13u, 64u, 255u, 256u, 1000u};
    for (size_t i = 0; i < sizeof K / sizeof K[0]; ++i) {
        kabul_parcalayici_t ps = kabul_parcalayici(KABUL_PARCA_SABIT, K[i], 0u);
        parcali(rol, s, n, &ps, &diger);
        snprintf(ileti, sizeof ileti, "%s, %u baytlık parçalar", baglam, (unsigned)K[i]);
        kabul_sonuc_esit(&tek, &diger, ileti);
    }
    for (uint64_t t = 1; t <= 8u; ++t) {
        kabul_parcalayici_t pr = kabul_parcalayici(KABUL_PARCA_RASTGELE, (size_t)(t * 37u), 0x50415243ull + t);
        parcali(rol, s, n, &pr, &diger);
        snprintf(ileti, sizeof ileti, "%s, rastgele parçalama %u", baglam, (unsigned)t);
        kabul_sonuc_esit(&tek, &diger, ileti);
    }
    if (her_bolme) {
        for (size_t k = 0; k <= n; ++k) {
            kabul_parcalayici_t pi = kabul_parcalayici(KABUL_PARCA_IKI, k, 0u);
            parcali(rol, s, n, &pi, &diger);
            snprintf(ileti, sizeof ileti, "%s, %u. bayttan bölme", baglam, (unsigned)k);
            kabul_sonuc_esit(&tek, &diger, ileti);
        }
    }
}

static void akis_dene(kabul_akis_turu_t tur, uint64_t tohum, size_t boy, int her_bolme)
{
    for (size_t ri = 0; ri < 2; ++ri) {
        char baglam[96];
        size_t gs = 0;
        size_t n = kabul_akis_uret(tur, ROLLER[ri], tohum, akis, sizeof akis, boy, gomulu, &gs);
        n = kabul_temiz_kuyruk(akis, n, sizeof akis, tohum);
        snprintf(baglam, sizeof baglam, "%s akış (rol %d, tohum %llx, %u B)", KABUL_AKIS_AD[tur],
                 (int)ROLLER[ri], (unsigned long long)tohum, (unsigned)n);
        parcalama_degismez(ROLLER[ri], akis, n, baglam, her_bolme);
        kabul_modelle_esit(ROLLER[ri], akis, n, &tek, baglam);
    }
}

static void test_A1_K07_saglamlik_a_duz_rastgele(void)
{
    for (uint64_t t = 0; t < 6u; ++t) {
        akis_dene(KABUL_AKIS_DUZ_RASTGELE, 0xD0A0ull + t, 6000u, 0);
    }
    akis_dene(KABUL_AKIS_DUZ_RASTGELE, 0xD0B0ull, 1500u, 1);
}

static void test_A1_K07_saglamlik_a_fd_yogun(void)
{
    for (uint64_t t = 0; t < 6u; ++t) {
        akis_dene(KABUL_AKIS_FD_YOGUN, 0xFD00ull + t, 6000u, 0);
    }
    akis_dene(KABUL_AKIS_FD_YOGUN, 0xFD10ull, 1500u, 1);
}

static void test_A1_K07_saglamlik_a_karisik(void)
{
    for (uint64_t t = 0; t < 8u; ++t) {
        akis_dene(KABUL_AKIS_KARISIK, 0xCA00ull + t, 16000u, 0);
    }
    akis_dene(KABUL_AKIS_KARISIK, 0xCA10ull, 2500u, 1);
}

/* (c) */
static void test_A1_K07_saglamlik_c_rastgeleye_gomulu(void)
{
    for (uint64_t t = 0; t < 8u; ++t) {
        for (size_t ri = 0; ri < 2; ++ri) {
            ida_link_rol_t rol = ROLLER[ri];
            char baglam[96];
            size_t gs = 0;
            size_t n = kabul_akis_uret(KABUL_AKIS_GOMULU_RASTGELE, rol, 0x6E00ull + t, akis, sizeof akis,
                                       16000u, gomulu, &gs);
            n = kabul_temiz_kuyruk(akis, n, sizeof akis, t);
            snprintf(baglam, sizeof baglam, "gömülü (rol %d, tohum %u)", (int)rol, (unsigned)t);
            parcalama_degismez(rol, akis, n, baglam, t == 0u);
            kabul_modelle_esit(rol, akis, n, &tek, baglam);
            /* tek: tek çağrılı besleme sonucu. Teslimler gömülülerin sıralı alt dizisidir. */
            static uint8_t teslim_edildi[KABUL_GOMULU_AZAMI];
            memset(teslim_edildi, 0, sizeof teslim_edildi);
            size_t g = 0;
            for (size_t i = 0; i < tek.sayi; ++i) {
                while (g < gs && !(gomulu[g].msgid == tek.t[i].msgid && gomulu[g].seq == tek.t[i].seq)) {
                    ++g;
                }
                TEST_ASSERT_TRUE_MESSAGE(g < gs, "gömülü olmayan bir çerçeve teslim edildi");
                teslim_edildi[g] = 1u;
                ++g;
            }
            /* Önündeki pencerede ([bas − 267, bas); teslim edilmiş bir önceki gömülü çerçeve
               pencereye giriyorsa onun sonundan itibaren) 0xFD ve 0xFE bulunmayan her gömülü
               çerçeve teslim edilir: 0xFD ya da 0xFE ile başlayan hiçbir aday (MAVLink 2 en çok
               267 B, MAVLink 1 en çok 263 B) o noktaya uzanamaz. */
            size_t zorunlu = 0;
            for (size_t k = 0; k < gs; ++k) {
                size_t bas = gomulu[k].bas;
                size_t ws = bas > IDA_LINK_CERCEVE_AZAMI ? bas - IDA_LINK_CERCEVE_AZAMI : 0u;
                if (k > 0u && teslim_edildi[k - 1u]) {
                    size_t onceki_son = gomulu[k - 1u].bas + gomulu[k - 1u].boy;
                    if (onceki_son > ws) {
                        ws = onceki_son;
                    }
                }
                int temiz = 1;
                for (size_t j = ws; j < bas; ++j) {
                    if (akis[j] == 0xFDu || akis[j] == 0xFEu) {
                        temiz = 0;
                        break;
                    }
                }
                if (!temiz) {
                    continue;
                }
                ++zorunlu;
                char ileti[160];
                snprintf(ileti, sizeof ileti, "%s: %u. baytta başlayan gömülü çerçeve (seq %u) teslim edilmedi",
                         baglam, (unsigned)bas, (unsigned)gomulu[k].seq);
                TEST_ASSERT_TRUE_MESSAGE(teslim_edildi[k], ileti);
            }
            TEST_ASSERT_TRUE_MESSAGE(zorunlu > 0u, "kurgu: zorunlu teslim edilecek gömülü çerçeve yok");
        }
    }
}

/* (d) */
static void test_A1_K07_saglamlik_d_temiz_bozuk_modelle_ayni(void)
{
    uint32_t toplam_bozuk = 0;
    for (uint64_t t = 0; t < 12u; ++t) {
        for (size_t ri = 0; ri < 2; ++ri) {
            ida_link_rol_t rol = ROLLER[ri];
            char baglam[96];
            size_t gs = 0;
            size_t n = kabul_akis_uret(KABUL_AKIS_TEMIZ_BOZUK, rol, 0x7E00ull + t, akis, sizeof akis, 16000u,
                                       gomulu, &gs);
            n = kabul_temiz_kuyruk(akis, n, sizeof akis, t);
            snprintf(baglam, sizeof baglam, "temiz + bozuk (rol %d, tohum %u, %u B)", (int)rol, (unsigned)t,
                     (unsigned)n);
            parcalama_degismez(rol, akis, n, baglam, t == 0u);
            kabul_modelle_esit(rol, akis, n, &tek, baglam);
            toplam_bozuk += tek.sayac.rx_crc_err;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(toplam_bozuk > 50u, "kurgu: yeterince bozuk aday üretilmedi");
}

/* Hiçbir girdi tanımsız davranış üretmez: 256 farklı tek baytlık akış ve 0xFD + her olası
   başlık baytı (incompat, len) birleşimi, iki rol, tek bayt besleme. Sonuçlar tek çağrıyla
   aynıdır (ASan/UBSan'lı derlemede bellek hatası da yakalanır). */
static void test_A1_K07_saglamlik_tum_tek_bayt_ve_baslik_birlesimleri(void)
{
    for (size_t ri = 0; ri < 2; ++ri) {
        for (unsigned b = 0; b < 256u; ++b) {
            akis[0] = (uint8_t)b;
            size_t n1 = kabul_temiz_kuyruk(akis, 1u, sizeof akis, b);
            parcalama_degismez(ROLLER[ri], akis, 1u, "tek bayt", 0);
            parcalama_degismez(ROLLER[ri], akis, n1, "tek bayt + kuyruk", 0);
            kabul_modelle_esit(ROLLER[ri], akis, n1, &tek, "tek bayt + kuyruk");
        }
        kabul_rng_t r;
        kabul_rng_kur(&r, 0xBA51ull);
        for (unsigned len = 0; len < 256u; len += 5u) {
            for (unsigned inc = 0; inc < 256u; inc += 51u) {
                size_t n = 0;
                akis[n++] = 0xFDu;
                akis[n++] = (uint8_t)len;
                akis[n++] = (uint8_t)inc;
                akis[n++] = 0u;
                akis[n++] = 0u;
                akis[n++] = kabul_karsi_sysid(ROLLER[ri]);
                akis[n++] = kabul_karsi_compid(ROLLER[ri]);
                uint32_t id = kabul_mesajlar[(len + inc) % KABUL_MESAJ_SAYISI].msgid;
                akis[n++] = (uint8_t)(id & 0xFFu);
                akis[n++] = (uint8_t)(id >> 8);
                akis[n++] = 0u;
                for (unsigned k = 0; k < len + 2u + 20u; ++k) {
                    akis[n++] = kabul_rng_bayt(&r);
                }
                parcalama_degismez(ROLLER[ri], akis, n, "başlık birleşimi", 0);
                n = kabul_temiz_kuyruk(akis, n, sizeof akis, len * 256u + inc);
                parcalama_degismez(ROLLER[ri], akis, n, "başlık birleşimi + kuyruk", 0);
                kabul_modelle_esit(ROLLER[ri], akis, n, &tek, "başlık birleşimi + kuyruk");
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K07_saglamlik_a_duz_rastgele);
    RUN_TEST(test_A1_K07_saglamlik_a_fd_yogun);
    RUN_TEST(test_A1_K07_saglamlik_a_karisik);
    RUN_TEST(test_A1_K07_saglamlik_c_rastgeleye_gomulu);
    RUN_TEST(test_A1_K07_saglamlik_d_temiz_bozuk_modelle_ayni);
    RUN_TEST(test_A1_K07_saglamlik_tum_tek_bayt_ve_baslik_birlesimleri);
    return UNITY_END();
}
