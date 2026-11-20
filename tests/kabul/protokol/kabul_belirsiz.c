/*
 * "Sözleşme belirsiz" grubu: A1 metninin (§3.11, §4.1 A1-K02–K08) iki biçimde okunabildiği
 * vakalar. Bu testler kırmızı olmaz: her vaka için okumaları ve gözlenen davranışı
 * "[BELIRSIZ] ..." iletisiyle yazar. Liste dondurma öncesi A1'e girdidir.
 *
 *   B1  Başlıkta reddedilen adaydan (incompat_flags ≠ 0, bilinmeyen kimlik, kötü len) sonra
 *       tarama nereden sürer: 0xFD'den sonraki bayttan mı, adayın len'e göre son baytından
 *       sonra mı? (A1-K07 yalnız sağlama hatası için söyler.)
 *   B2  MAVLink 1 sayımı: rx_mav1 çerçeve başına mı, 0xFE baytı başına mı; MAVLink 1
 *       çerçevesinin geri kalan baytları taranır mı, atlanır mı; akış sonundaki tek 0xFE.
 *   B3  rx_seq_gap: boşluk olayı mı, kayıp çerçeve sayısı mı; yinelenen ve geri giden seq.
 *   B4  Denetim sırası: birden çok hata taşıyan çerçevede hangi sayaç artar.
 *   B5  Bilinmeyen kimlik: rx_unknown_msgid (A1-K06) mi, rx_crc_err (Tablo 3.11b, "bilinmeyen
 *       kimlik dahil") mi, ikisi mi?
 *   B6  K08 "ardışık iki geçerli çerçeve": sağlaması tutan ama kimlik/yön yüzünden reddedilen
 *       ara çerçeve seq geçmişine girer mi?
 *   B7  K07 "bir çerçeveye ait olmayan her bayt": sağlaması tutmayan adayın baytları ve kesik
 *       adayın yuttuğu çerçevenin bölge dışında kalan baytları rx_resync_bytes'a girer mi?
 *   B8  Sözde rastgele akışlarda kitaplığın hangi model okumasıyla (B1 × B2, B3) tümüyle
 *       aynı olduğu.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[40000];
static kabul_sonuc_t sonuc;
static kabul_model_t model;

static void sayaclar_yaz(const char *onek, const ida_link_sayaclar_t *s, size_t teslim)
{
    kabul_bilgi("%s teslim=%u frames=%u crc=%u unk=%u iflag=%u badlen=%u wsys=%u mav1=%u resync=%u gap=%u",
                onek, (unsigned)teslim, (unsigned)s->rx_frames, (unsigned)s->rx_crc_err,
                (unsigned)s->rx_unknown_msgid, (unsigned)s->rx_iflag, (unsigned)s->rx_bad_len,
                (unsigned)s->rx_wrong_sys, (unsigned)s->rx_mav1, (unsigned)s->rx_resync_bytes,
                (unsigned)s->rx_seq_gap);
}

/* B1: başlığı reddedilen adayın yükünün içine tam bir geçerli çerçeve gömülür. Okuma A
   (yalnız 0xFD tüketilir): gömülü çerçeve teslim edilir. Okuma B (aday len'e göre atlanır):
   edilmez. Aday dışındaki baytlar temizdir. */
static void test_belirsiz_B1_baslik_reddi_sonrasi_tarama(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0xB1ull);
    static const char *const TUR[3] = {"incompat_flags = 1", "bilinmeyen kimlik 46050", "len = azami + 1"};
    for (int t = 0; t < 3; ++t) {
        const kabul_mesaj_t *m = kabul_mesaj_bul(46030u); /* uzun yüklü mesaj: içine çerçeve sığar */
        uint8_t ic[32];
        size_t ic_n = kabul_temiz_kur(ic, &r, 46003u, 77u, 250u, 25u, 3u, 1, NULL);
        uint8_t len = (uint8_t)(t == 2 ? m->yuk_azami + 1u : 100u);
        uint8_t incompat = (uint8_t)(t == 0 ? 1u : 0u);
        uint32_t id = t == 1 ? 46050u : m->msgid;
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        size_t n = 0;
        for (int deneme = 0; deneme < 4096; ++deneme) {
            for (size_t i = 0; i < len; ++i) {
                yuk[i] = (uint8_t)(1u + kabul_rng_aralik(&r, 0xFCu));
            }
            memcpy(&yuk[40], ic, ic_n);
            n = kabul_cerceve_ham(akis, len, incompat, 0u, 76u, 250u, 25u, id, yuk, len, m->crc_extra);
            if (akis[n - 1u] < 0xFDu && akis[n - 2u] < 0xFDu) {
                break;
            }
        }
        size_t aday = n;
        n += kabul_temiz_kur(&akis[n], &r, 46010u, 78u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        const char *okuma;
        if (sonuc.sayi == 2u && sonuc.t[0].seq == 77u) {
            okuma = "A (yalnız 0xFD tüketildi; gömülü çerçeve teslim edildi)";
        } else if (sonuc.sayi == 1u && sonuc.t[0].seq == 78u) {
            okuma = "B (aday len'e göre atlandı; gömülü çerçeve teslim edilmedi)";
        } else {
            okuma = "hiçbiri";
        }
        kabul_bilgi("[BELIRSIZ] B1 %s (aday %u B): gözlenen okuma %s", TUR[t], (unsigned)aday, okuma);
        sayaclar_yaz("[BELIRSIZ] B1   sayaçlar:", &sonuc.sayac, sonuc.sayi);
    }
}

/* B2 */
static void test_belirsiz_B2_mavlink1_sayimi(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0xB2ull);
    /* (i) Yükünde ikinci bir 0xFE bulunan MAVLink 1 çerçevesi: rx_mav1 = 1 (çerçeve) ya da
       2 (bayt). (ii) Yükünde tam bir geçerli MAVLink 2 çerçevesi bulunan MAVLink 1 çerçevesi:
       gömülü çerçeve teslim edilir (bayt okuması) ya da edilmez (çerçeve okuması). */
    for (int t = 0; t < 2; ++t) {
        size_t n = 0;
        uint8_t ic[32];
        size_t ic_n = kabul_temiz_kur(ic, &r, 46003u, 5u, 250u, 25u, 3u, 1, NULL);
        uint8_t len = 40u;
        akis[n++] = 0xFEu;
        akis[n++] = len;
        akis[n++] = 0u;
        akis[n++] = 250u;
        akis[n++] = 25u;
        akis[n++] = 0u;
        for (size_t i = 0; i < len; ++i) {
            akis[n++] = (uint8_t)(1u + kabul_rng_aralik(&r, 0xFCu));
        }
        if (t == 0) {
            akis[6u + 10u] = 0xFEu;
        } else {
            memcpy(&akis[6u + 5u], ic, ic_n);
        }
        uint16_t crc = kabul_crc(&akis[1], 5u + (size_t)len, 0xFFFFu);
        crc = kabul_crc_bayt(crc, 50u);
        /* MAVLink 1 sağlaması bu katmanda denetlenmez; 0xFD/0xFE çıkarsa 0 yazılır (temizlik). */
        uint8_t c0 = (uint8_t)(crc & 0xFFu);
        uint8_t c1 = (uint8_t)(crc >> 8);
        akis[n++] = (uint8_t)((c0 == 0xFDu || c0 == 0xFEu) ? 0x00u : c0);
        akis[n++] = (uint8_t)((c1 == 0xFDu || c1 == 0xFEu) ? 0x00u : c1);
        size_t m1 = n;
        n += kabul_temiz_kur(&akis[n], &r, 46010u, 6u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        if (t == 0) {
            kabul_bilgi("[BELIRSIZ] B2 yükünde ikinci 0xFE olan MAVLink 1 (%u B): rx_mav1 = %u (%s), rx_resync_bytes = %u (%s)",
                        (unsigned)m1, (unsigned)sonuc.sayac.rx_mav1,
                        sonuc.sayac.rx_mav1 == 2u ? "0xFE baytı başına" : (sonuc.sayac.rx_mav1 == 1u ? "çerçeve başına" : "başka"),
                        (unsigned)sonuc.sayac.rx_resync_bytes,
                        sonuc.sayac.rx_resync_bytes == 0u ? "geri kalan baytlar sayılmadı" : "geri kalan baytlar taranıp sayıldı");
        } else {
            int teslim = sonuc.sayi == 2u;
            kabul_bilgi("[BELIRSIZ] B2 yükünde geçerli MAVLink 2 çerçevesi olan MAVLink 1: gömülü çerçeve %s (%s)",
                        teslim ? "teslim edildi" : "teslim edilmedi",
                        teslim ? "MAVLink 1'in yalnız 0xFE'si tüketiliyor" : "MAVLink 1 çerçevesi bütünüyle atlanıyor");
        }
        sayaclar_yaz("[BELIRSIZ] B2   sayaçlar:", &sonuc.sayac, sonuc.sayi);
    }
    /* (iii) Akış sonunda tek 0xFE. */
    size_t n = kabul_temiz_kur(akis, &r, 46003u, 0u, 250u, 25u, 3u, 1, NULL);
    akis[n++] = 0xFEu;
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    kabul_bilgi("[BELIRSIZ] B2 akış sonunda tek 0xFE: rx_mav1 = %u (%s)", (unsigned)sonuc.sayac.rx_mav1,
                sonuc.sayac.rx_mav1 ? "hemen sayıldı" : "çerçeve tamamlanınca sayılacak");
}

/* B3 */
static void seq_dizisi(const uint8_t *seq, size_t sayi, const char *ad)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0xB3ull);
    size_t n = 0;
    uint32_t olay = 0;
    uint32_t kayip = 0;
    for (size_t i = 0; i < sayi; ++i) {
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        uint8_t gecici[IDA_LINK_CERCEVE_AZAMI];
        (void)kabul_temiz_kur(gecici, &r, 46027u, 0u, 250u, 25u, 6u, 1, yuk);
        n += kabul_cerceve_kur(&akis[n], 46027u, seq[i], 250u, 25u, yuk, sizeof yuk, 1);
        if (i > 0u) {
            uint8_t fark = (uint8_t)(seq[i] - seq[i - 1u] - 1u);
            olay += fark ? 1u : 0u;
            kayip += fark;
        }
    }
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    const char *okuma = sonuc.sayac.rx_seq_gap == olay && olay == kayip ? "iki okuma aynı"
                        : sonuc.sayac.rx_seq_gap == olay                ? "olay sayısı"
                        : sonuc.sayac.rx_seq_gap == kayip               ? "kayıp çerçeve sayısı"
                                                                        : "hiçbiri";
    kabul_bilgi("[BELIRSIZ] B3 %s: rx_seq_gap = %u (olay okuması %u, kayıp okuması %u): gözlenen %s; teslim %u/%u",
                ad, (unsigned)sonuc.sayac.rx_seq_gap, (unsigned)olay, (unsigned)kayip, okuma,
                (unsigned)sonuc.sayi, (unsigned)sayi);
}

static void test_belirsiz_B3_seq_gap_olay_mi_kayip_mi(void)
{
    static const uint8_t A[] = {1u, 5u, 6u};
    seq_dizisi(A, sizeof A, "1 -> 5 (üç kayıp)");
    static const uint8_t B[] = {10u, 11u, 11u, 12u};
    seq_dizisi(B, sizeof B, "yinelenen seq 11 -> 11");
    static const uint8_t C[] = {10u, 11u, 9u, 10u};
    seq_dizisi(C, sizeof C, "geri giden seq 11 -> 9");
    static const uint8_t D[] = {0u, 128u};
    seq_dizisi(D, sizeof D, "0 -> 128");
}

/* B4 ve B5 */
typedef struct {
    const char *ad;
    uint8_t incompat, len_kaydir, sysid, compid;
    uint32_t msgid;
    int crc_boz;
} birlesik_t;

static void test_belirsiz_B4_B5_denetim_sirasi_ve_bilinmeyen_kimlik(void)
{
    static const birlesik_t V[] = {
        {"B5 bilinmeyen kimlik 46050 (tek hata)", 0u, 0u, 250u, 25u, 46050u, 0},
        {"B5 bilinmeyen kimlik 0 (HEARTBEAT)", 0u, 0u, 250u, 25u, 0u, 0},
        {"B4 CRC bozuk + yabancı sysid", 0u, 0u, 7u, 25u, 46020u, 1},
        {"B4 CRC bozuk + yabancı compid", 0u, 0u, 250u, 7u, 46020u, 1},
        {"B4 CRC bozuk + yönü yanlış kimlik (46060)", 0u, 0u, 250u, 25u, 46060u, 1},
        {"B4 yabancı sysid + yönü yanlış kimlik (CRC doğru)", 0u, 0u, 7u, 25u, 46060u, 0},
        {"B4 incompat + bilinmeyen kimlik", 1u, 0u, 250u, 25u, 46050u, 0},
        {"B4 incompat + len > azami", 1u, 1u, 250u, 25u, 46020u, 0},
        {"B4 incompat + CRC bozuk", 1u, 0u, 250u, 25u, 46020u, 1},
        {"B4 len > azami + yabancı sysid (CRC o len'le doğru)", 0u, 1u, 7u, 25u, 46020u, 0},
        {"B4 len > azami + CRC bozuk", 0u, 1u, 250u, 25u, 46020u, 1},
        {"B4 bilinmeyen kimlik + yabancı sysid", 0u, 0u, 7u, 25u, 46050u, 0},
    };
    kabul_rng_t r;
    kabul_rng_kur(&r, 0xB4ull);
    for (size_t i = 0; i < sizeof V / sizeof V[0]; ++i) {
        const kabul_mesaj_t *m = kabul_mesaj_bul(V[i].msgid);
        uint8_t crc_extra = (uint8_t)(m != NULL ? m->crc_extra : 0x33u);
        uint8_t azami = (uint8_t)(m != NULL ? m->yuk_azami : 20u);
        uint8_t len = (uint8_t)(V[i].len_kaydir ? azami + 1u : azami);
        size_t n = kabul_temiz_ham(akis, &r, len, V[i].incompat, 0u, 3u, V[i].sysid, V[i].compid, V[i].msgid,
                                   crc_extra, 0u);
        if (V[i].crc_boz) {
            uint8_t v = akis[n - 1u];
            akis[n - 1u] = (uint8_t)(v == 0x01u ? 0x02u : 0x01u);
        }
        n += kabul_temiz_kur(&akis[n], &r, 46010u, 4u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        const ida_link_sayaclar_t *s = &sonuc.sayac;
        kabul_bilgi("[BELIRSIZ] %s: artan sayaç(lar):%s%s%s%s%s%s; sonraki geçerli çerçeve %s", V[i].ad,
                    s->rx_crc_err ? " rx_crc_err" : "", s->rx_unknown_msgid ? " rx_unknown_msgid" : "",
                    s->rx_iflag ? " rx_iflag" : "", s->rx_bad_len ? " rx_bad_len" : "",
                    s->rx_wrong_sys ? " rx_wrong_sys" : "", kabul_ret_toplami(s) == 0u ? " (hiçbiri)" : "",
                    (sonuc.sayi >= 1u && sonuc.t[sonuc.sayi - 1u].seq == 4u) ? "teslim edildi" : "TESLİM EDİLMEDİ");
    }
}

/* B6 */
static void test_belirsiz_B6_reddedilen_ara_cerceve_ve_seq(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0xB6ull);
    static const char *const AD[2] = {"yabancı kimlik", "yönü yanlış kimlik"};
    for (int t = 0; t < 2; ++t) {
        size_t n = kabul_temiz_kur(akis, &r, 46020u, 1u, 250u, 25u, 9u, 1, NULL);
        if (t == 0) {
            n += kabul_temiz_kur(&akis[n], &r, 46020u, 2u, 1u, 1u, 9u, 1, NULL);
        } else {
            n += kabul_temiz_kur(&akis[n], &r, 46060u, 2u, 250u, 25u, 9u, 1, NULL);
        }
        n += kabul_temiz_kur(&akis[n], &r, 46020u, 3u, 250u, 25u, 9u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        kabul_bilgi("[BELIRSIZ] B6 seq 1, [2: %s, sağlama doğru], 3: rx_seq_gap = %u (%s)", AD[t],
                    (unsigned)sonuc.sayac.rx_seq_gap,
                    sonuc.sayac.rx_seq_gap == 1u ? "reddedilen çerçeve seq geçmişine girmiyor"
                                                 : "reddedilen çerçeve seq geçmişine giriyor");
    }
}

/* B7 */
static void test_belirsiz_B7_aday_baytlari_ve_resync(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0xB7ull);
    /* (i) Sağlaması tutmayan tam çerçeve (temiz) + geçerli çerçeve. */
    size_t n = kabul_temiz_kur(akis, &r, 46020u, 1u, 250u, 25u, 47u, 1, NULL);
    size_t aday = n;
    akis[n - 1u] = (uint8_t)(akis[n - 1u] == 0x01u ? 0x02u : 0x01u);
    n += kabul_temiz_kur(&akis[n], &r, 46020u, 2u, 250u, 25u, 47u, 1, NULL);
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    kabul_bilgi("[BELIRSIZ] B7 sağlaması tutmayan %u baytlık aday: rx_resync_bytes = %u (%s)", (unsigned)aday,
                (unsigned)sonuc.sayac.rx_resync_bytes,
                sonuc.sayac.rx_resync_bytes == 0u ? "aday baytları sayılmıyor"
                : sonuc.sayac.rx_resync_bytes == aday ? "aday baytlarının hepsi sayılıyor" : "başka");
    /* (ii) Kesik çerçeve (başlık + 5 bayt) + iki geçerli çerçeve: ilki yutulur; yutulan
       çerçevenin aday bölgesi dışında kalan baytları sayılır mı? */
    n = kabul_temiz_kur(akis, &r, 46020u, 1u, 250u, 25u, 47u, 1, NULL);
    size_t kesik = 15u;
    size_t a_son = n; /* adayın bölgesi [0, 59) */
    n = kesik;
    size_t b_bas = n;
    n += kabul_temiz_kur(&akis[n], &r, 46030u, 2u, 250u, 25u, 200u, 1, NULL);
    size_t b_son = n;
    n += kabul_temiz_kur(&akis[n], &r, 46020u, 3u, 250u, 25u, 47u, 1, NULL);
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    size_t artik = b_son - a_son;
    kabul_bilgi("[BELIRSIZ] B7 kesik aday (bölge 0..%u) yuttuğu çerçeveyi (%u..%u) böldü: dışta kalan %u bayt; "
                "rx_resync_bytes = %u (%s); teslim %u, rx_crc_err %u",
                (unsigned)a_son, (unsigned)b_bas, (unsigned)b_son, (unsigned)artik,
                (unsigned)sonuc.sayac.rx_resync_bytes,
                sonuc.sayac.rx_resync_bytes == artik ? "dışta kalan baytlar çerçeveye ait sayılmıyor"
                                                     : "başka", (unsigned)sonuc.sayi, (unsigned)sonuc.sayac.rx_crc_err);
}

/* B8 */
static int model_ayni(const kabul_model_t *m, const kabul_sonuc_t *o, int kayip)
{
    if (m->sonuc.sayi != o->sayi) {
        return 0;
    }
    for (size_t i = 0; i < o->sayi; ++i) {
        if (m->sonuc.t[i].msgid != o->t[i].msgid || m->sonuc.t[i].seq != o->t[i].seq ||
            m->sonuc.t[i].len != o->t[i].len || memcmp(m->sonuc.t[i].yuk, o->t[i].yuk, IDA_LINK_YUK_AZAMI) != 0) {
            return 0;
        }
    }
    ida_link_sayaclar_t b = m->sonuc.sayac;
    if (kayip) {
        b.rx_seq_gap = m->seq_gap_kayip;
    }
    return memcmp(&b, &o->sayac, sizeof b) == 0;
}

static void test_belirsiz_B8_rastgele_akislarda_model_okumasi(void)
{
    static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};
    static const kabul_akis_turu_t TUR[] = {KABUL_AKIS_DUZ_RASTGELE, KABUL_AKIS_FD_YOGUN,
                                            KABUL_AKIS_GOMULU_RASTGELE, KABUL_AKIS_TEMIZ_BOZUK,
                                            KABUL_AKIS_KARISIK};
    for (size_t ti = 0; ti < sizeof TUR / sizeof TUR[0]; ++ti) {
        unsigned eslesen[8] = {0};
        unsigned akis_sayisi = 0;
        unsigned teslim_farki_yok = 0;
        for (uint64_t t = 0; t < 6u; ++t) {
            for (size_t ri = 0; ri < 2; ++ri) {
                size_t n = kabul_akis_uret(TUR[ti], ROLLER[ri], 0xB800ull + t * 3u + ti * 101u, akis, sizeof akis,
                                           20000u, NULL, NULL);
                kabul_besle_yeni(ROLLER[ri], akis, n, &sonuc);
                ++akis_sayisi;
                int herhangi_teslim = 0;
                for (int k = 0; k < 8; ++k) {
                    kabul_okuma_t o;
                    o.bas_okuma = k & 1;
                    o.mav1_okuma = (k >> 1) & 1;
                    kabul_model_ayristir(ROLLER[ri], o, akis, n, &model);
                    if (model_ayni(&model, &sonuc, (k >> 2) & 1)) {
                        ++eslesen[k];
                    }
                    if (model.sonuc.sayi == sonuc.sayi) {
                        herhangi_teslim = 1;
                    }
                }
                teslim_farki_yok += (unsigned)herhangi_teslim;
            }
        }
        kabul_bilgi("[BELIRSIZ] B8 %s (%u akış): tümüyle aynı olan model okuması sayıları "
                    "[başlık A/B, MAVLink1 bayt/çerçeve, seq olay/kayıp]: "
                    "A-bayt-olay %u, B-bayt-olay %u, A-çerçeve-olay %u, B-çerçeve-olay %u, "
                    "A-bayt-kayıp %u, B-bayt-kayıp %u, A-çerçeve-kayıp %u, B-çerçeve-kayıp %u; "
                    "teslim sayısı en az bir modelle aynı: %u",
                    KABUL_AKIS_AD[TUR[ti]], akis_sayisi, eslesen[0], eslesen[1], eslesen[2], eslesen[3], eslesen[4],
                    eslesen[5], eslesen[6], eslesen[7], teslim_farki_yok);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_belirsiz_B1_baslik_reddi_sonrasi_tarama);
    RUN_TEST(test_belirsiz_B2_mavlink1_sayimi);
    RUN_TEST(test_belirsiz_B3_seq_gap_olay_mi_kayip_mi);
    RUN_TEST(test_belirsiz_B4_B5_denetim_sirasi_ve_bilinmeyen_kimlik);
    RUN_TEST(test_belirsiz_B6_reddedilen_ara_cerceve_ve_seq);
    RUN_TEST(test_belirsiz_B7_aday_baytlari_ve_resync);
    RUN_TEST(test_belirsiz_B8_rastgele_akislarda_model_okumasi);
    return UNITY_END();
}
