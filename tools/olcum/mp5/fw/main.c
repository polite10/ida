/*
 * W0-MP5a ölçüm prototipi: kartta ida_link tel katmanının ayrıştırma ve kodlama maliyeti.
 * Atılabilir prototip (09 §2.1); ürün koduna taşınmaz. Platform kodu gnu17.
 *
 * Ölçülen kod ürün kitaplığıdır (protocol/ida_link, fw-release bayraklarıyla depo derlemesinden
 * bağlanan libida_link.a; bu prototip kitaplığa dokunmaz). Girdi akışları derleme zamanında host'ta
 * üretilir (pc/mp5_uret.c -> mp5_akislar.c), kartta sabittir ve ölçümden önce SRAM'e kopyalanır.
 *
 * Ölçüm bölgesi: kesmeler kapalı (PRIMASK; ayrıca açılıştan sonra SysTick durdurulur, başka kesme
 * kaynağı açılmaz), DWT CYCCNT farkı. Bir yineleme = akışın tamamı (ayrıştırmada bütün baytlar,
 * kodlamada bütün mesajlar). Her seride MP5_YINELEME yineleme; süreler tek tek saklanır, seri sonunda
 * sıralanıp (değer:adet) olarak gönderilir; yüzdelikleri PC hesaplar (ozet.py).
 *
 * Önbellek kipleri: sicak = ICACHE açık, ölçülmeyen bir ısınma yinelemesinden sonra; soguk = ICACHE
 * açık, her yinelemeden önce geçersiz kılınır (kesmeler kapalıyken, ölçüm bölgesinin hemen önünde);
 * kapali = ICACHE kapalı. Ürün yapılandırması ICACHE açıktır (01 §2 md.10).
 *
 * Doğruluk: (1) her ayrıştırma yinelemesinin özeti (Σ msgid + len + son yük baytı; çözmede yapının ilk
 * ve son baytı) host referansıyla aynı olmalı; (2) seri sonunda alıcı sayaçları beklenen değerde
 * olmalı; (3) ölçülmeyen bir doğrulama geçişi, teslim edilen her çerçevenin (msgid, seq, len, yük) ve
 * çözülen her yapının FNV-1a'sını host referansıyla karşılaştırır (üç besleme biçiminde);
 * (4) kodlamada her yinelemenin çıktı boyu ve FNV-1a'sı host referansıyla aynı olmalı.
 */
#include "ida_link/tel.h"
#include "kart.h"
#include "mp5_veri.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef MP5_YINELEME
#define MP5_YINELEME 10000u
#endif
#ifndef MP5_DERLEME
#define MP5_DERLEME "?"
#endif
#ifndef MP5_KIMLIK
#define MP5_KIMLIK "yok"
#endif
#ifndef MP5_ONBELLEK
#define MP5_ONBELLEK 7u /* koşulacak önbellek kipleri: bit 0 sicak, bit 1 soguk, bit 2 kapali */
#endif

#define TAMPON 4096u
#define HAVUZ_U64 (48u * 1024u / 8u)
#define YAPI_AZAMI_N 256u
#define YIGIN_DESEN 0xA5C3A5C3u
#define YIGIN_PAY 8192u

enum { B_TEK = 0, B_P64 = 1, B_BAYT = 2 };
enum { O_SICAK = 0, O_SOGUK = 1, O_KAPALI = 2 };
static const char *const BESLEME_AD[] = {"tek", "p64", "bayt"};
static const char *const ONBELLEK_AD[] = {"sicak", "soguk", "kapali"};

/* ---------------- SRAM'deki çalışma verisi ---------------- */
static uint8_t gelen[TAMPON] __attribute__((aligned(8)));
static uint8_t cikis[TAMPON] __attribute__((aligned(8)));
static uint32_t samp[MP5_YINELEME];
static union {
    uint64_t hiza;
    uint8_t b[256];
} yapi_cikis;
static uint64_t havuz[HAVUZ_U64];

typedef struct {
    uint32_t msgid;
    uint32_t boy;
    const uint8_t *yapi;
} yapi_ram_t;
static yapi_ram_t yapi_ram[YAPI_AZAMI_N];
static yapi_ram_t kod_dizi[64];
static uint32_t kod_n;

static ida_link_alici_t alici;
static ida_link_gonderici_t gonderici;
static ida_link_cerceve_t cerceve;
static uint32_t hata_say;
static uint32_t toplam_hata;
static uint32_t seri_no;
static satir_t s;

/* ---------------- ölçülen işlevler (bütün serilerde aynı kod) ---------------- */
typedef uint32_t (*ayr_islevi_t)(const uint8_t *v, uint32_t n);

static uint32_t __attribute__((noinline)) ayr_tek(const uint8_t *v, uint32_t n)
{
    uint32_t oz = 0u;
    uint32_t i = 0u;
    while (i < n) {
        size_t tk = 0u;
        const ida_link_rc_t rc = ida_link_al(&alici, &v[i], n - i, &tk, &cerceve);
        if (rc == IDA_LINK_CERCEVE) {
            oz += cerceve.msgid + cerceve.len + cerceve.yuk[cerceve.len - 1u];
        } else if (rc != IDA_LINK_BEKLE) {
            hata_say++;
            break;
        }
        i += (uint32_t)tk;
    }
    return oz;
}

static uint32_t __attribute__((noinline)) ayr_p64(const uint8_t *v, uint32_t n)
{
    uint32_t oz = 0u;
    for (uint32_t p = 0u; p < n; p += 64u) {
        const uint32_t m = (n - p) < 64u ? (n - p) : 64u;
        uint32_t j = 0u;
        while (j < m) {
            size_t tk = 0u;
            const ida_link_rc_t rc = ida_link_al(&alici, &v[p + j], m - j, &tk, &cerceve);
            if (rc == IDA_LINK_CERCEVE) {
                oz += cerceve.msgid + cerceve.len + cerceve.yuk[cerceve.len - 1u];
            } else if (rc != IDA_LINK_BEKLE) {
                hata_say++;
                return oz;
            }
            j += (uint32_t)tk;
        }
    }
    return oz;
}

static uint32_t __attribute__((noinline)) ayr_bayt(const uint8_t *v, uint32_t n)
{
    uint32_t oz = 0u;
    for (uint32_t i = 0u; i < n; ++i) {
        size_t tk = 0u;
        const ida_link_rc_t rc = ida_link_al(&alici, &v[i], 1u, &tk, &cerceve);
        if (rc == IDA_LINK_CERCEVE) {
            oz += cerceve.msgid + cerceve.len + cerceve.yuk[cerceve.len - 1u];
        } else if (rc != IDA_LINK_BEKLE) {
            hata_say++;
            break;
        }
    }
    return oz;
}

/* Ayrıştırma + ida_link_coz (tek seferde besleme): teslim edilen her çerçeve tipli yapıya çözülür. */
static uint32_t __attribute__((noinline)) ayr_alcoz(const uint8_t *v, uint32_t n)
{
    uint32_t oz = 0u;
    uint32_t i = 0u;
    while (i < n) {
        size_t tk = 0u;
        const ida_link_rc_t rc = ida_link_al(&alici, &v[i], n - i, &tk, &cerceve);
        if (rc == IDA_LINK_CERCEVE) {
            const uint32_t k = cerceve.msgid - 46000u;
            const uint32_t boy = k < 100u ? mp5_boy_tablosu[k] : 0u;
            if (boy != 0u && ida_link_coz(&cerceve, cerceve.msgid, yapi_cikis.b, boy) == IDA_LINK_TAMAM) {
                oz += cerceve.msgid + yapi_cikis.b[0] + yapi_cikis.b[boy - 1u];
            } else {
                hata_say++;
            }
        } else if (rc != IDA_LINK_BEKLE) {
            hata_say++;
            break;
        }
        i += (uint32_t)tk;
    }
    return oz;
}

/* Kodlama: kod_dizi[0, kod_n) sırasıyla çıkış tamponuna; dönüş yazılan bayt. */
static uint32_t __attribute__((noinline)) kodla(void)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < kod_n; ++i) {
        size_t yz = 0u;
        if (ida_link_gonder(&gonderici, kod_dizi[i].msgid, kod_dizi[i].yapi, kod_dizi[i].boy, &cikis[n],
                            TAMPON - n, &yz) != IDA_LINK_TAMAM) {
            hata_say++;
        }
        n += (uint32_t)yz;
    }
    return n;
}

static uint32_t __attribute__((noinline)) bos_islev(const uint8_t *v, uint32_t n)
{
    (void)v;
    return n;
}

/* ---------------- yardımcılar ---------------- */
static uint32_t fnv(uint32_t h, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0u; i < n; ++i) { h = (h ^ p[i]) * 16777619u; }
    return h;
}

static void yigin_sirala(uint32_t *a, uint32_t n)
{
    /* yığın sıralama (heap sort), yerinde */
    if (n < 2u) { return; }
    for (uint32_t bas = n / 2u; bas-- > 0u;) {
        uint32_t k = bas;
        for (;;) {
            uint32_t c = 2u * k + 1u;
            if (c >= n) { break; }
            if (c + 1u < n && a[c + 1u] > a[c]) { c++; }
            if (a[k] >= a[c]) { break; }
            const uint32_t t = a[k]; a[k] = a[c]; a[c] = t; k = c;
        }
    }
    for (uint32_t son = n - 1u; son > 0u; --son) {
        const uint32_t t = a[0]; a[0] = a[son]; a[son] = t;
        uint32_t k = 0u;
        for (;;) {
            uint32_t c = 2u * k + 1u;
            if (c >= son) { break; }
            if (c + 1u < son && a[c + 1u] > a[c]) { c++; }
            if (a[k] >= a[c]) { break; }
            const uint32_t u = a[k]; a[k] = a[c]; a[c] = u; k = c;
        }
    }
}

/* samp[0, n)'i sıralayıp (değer:adet) satırlarıyla gönderir; ardından E satırı. */
static void dagilim_gonder(uint32_t id, uint32_t n)
{
    yigin_sirala(samp, n);
    uint32_t satir = 0u, farkli = 0u;
    uint32_t i = 0u;
    s.n = 0u;
    while (i < n) {
        const uint32_t v = samp[i];
        uint32_t c = 0u;
        while (i < n && samp[i] == v) { c++; i++; }
        if (s.n == 0u) { s_bas(&s, "H"); s_kv(&s, "id", id); }
        s_cat(&s, " "); s_u64(&s, v); s_cat(&s, ":"); s_u64(&s, c);
        farkli++;
        if (s_kalan(&s) < 32u) { s_gonder(&s); satir++; }
    }
    if (s.n != 0u) { s_gonder(&s); satir++; }
    s_bas(&s, "E");
    s_kv(&s, "id", id);
    s_kv(&s, "h", satir);
    s_kv(&s, "farkli", farkli);
    s_gonder(&s);
}

static void ilk_yaz(void)
{
    s_cat(&s, " ilk=");
    for (uint32_t i = 0u; i < 8u && i < MP5_YINELEME; ++i) {
        if (i) { s_cat(&s, ","); }
        s_u64(&s, samp[i]);
    }
}

static void onbellek_kur(int ob)
{
    if (ob == O_KAPALI) { kart_icache_kapat(); } else { kart_icache_ac(); }
}

/* ---------------- seriler ---------------- */
static void seri_bos(int ob, int cagri)
{
    const uint32_t id = seri_no++;
    uint32_t c0, c1;
    for (uint32_t k = 0u; k < MP5_YINELEME; ++k) {
        __disable_irq();
        if (ob == O_SOGUK) { kart_icache_bosalt(); }
        if (cagri) {
            ayr_islevi_t f = bos_islev;
            __asm volatile("" : "+r"(f));
            c0 = kart_cevrim();
            (void)f(gelen, k);
            c1 = kart_cevrim();
        } else {
            c0 = kart_cevrim();
            c1 = kart_cevrim();
        }
        __enable_irq();
        samp[k] = c1 - c0;
    }
    uint32_t mn = UINT32_MAX, mx = 0u;
    uint64_t top = 0u;
    for (uint32_t k = 0u; k < MP5_YINELEME; ++k) {
        mn = samp[k] < mn ? samp[k] : mn;
        mx = samp[k] > mx ? samp[k] : mx;
        top += samp[k];
    }
    s_bas(&s, "S");
    s_kv(&s, "id", id);
    s_ks(&s, "tur", cagri ? "bos_cagri" : "bos");
    s_ks(&s, "onbellek", ONBELLEK_AD[ob]);
    s_kv(&s, "n", MP5_YINELEME);
    s_kv(&s, "min", mn);
    s_kv(&s, "max", mx);
    s_kv(&s, "top", top);
    ilk_yaz();
    s_gonder(&s);
    dagilim_gonder(id, MP5_YINELEME);
}

static void seri_ayr(const mp5_ayr_t *a, int besleme, int alcoz, int ob)
{
    const uint32_t id = seri_no++;
    ayr_islevi_t f = alcoz ? ayr_alcoz : (besleme == B_P64 ? ayr_p64 : (besleme == B_BAYT ? ayr_bayt : ayr_tek));
    memcpy(gelen, a->bayt, a->n_bayt);
    const uint32_t n = a->n_bayt;
    const uint32_t beklenen = alcoz ? a->ozet_coz : a->ozet_al;
    (void)ida_link_alici_kur(&alici, (ida_link_rol_t)a->rol_alici);
    hata_say = 0u;
    uint32_t ozet_hata = 0u, isinma = 0u, kosu = 0u;
    uint32_t c0, c1;
    if (ob == O_SICAK) {
        __disable_irq();
        c0 = kart_cevrim();
        const uint32_t oz = f(gelen, n);
        c1 = kart_cevrim();
        __enable_irq();
        isinma = c1 - c0;
        ozet_hata += (oz != beklenen) ? 1u : 0u;
        kosu++;
    }
    for (uint32_t k = 0u; k < MP5_YINELEME; ++k) {
        __disable_irq();
        if (ob == O_SOGUK) { kart_icache_bosalt(); }
        c0 = kart_cevrim();
        const uint32_t oz = f(gelen, n);
        c1 = kart_cevrim();
        __enable_irq();
        samp[k] = c1 - c0;
        ozet_hata += (oz != beklenen) ? 1u : 0u;
    }
    kosu += MP5_YINELEME;
    const ida_link_sayaclar_t *z = &alici.sayac;
    const uint32_t bek_gap = (a->n_cerceve % 256u) == 0u ? 0u : kosu - 1u;
    const bool sayac_ok = z->rx_frames == a->n_cerceve * kosu && z->rx_bytes == n * kosu && z->rx_crc_err == 0u &&
                          z->rx_unknown_msgid == 0u && z->rx_iflag == 0u && z->rx_bad_len == 0u &&
                          z->rx_wrong_sys == 0u && z->rx_mav1 == 0u && z->rx_overflow == 0u &&
                          z->rx_resync_bytes == 0u && z->rx_seq_gap == bek_gap;
    if (!sayac_ok || ozet_hata != 0u || hata_say != 0u) { toplam_hata++; }
    uint32_t mn = UINT32_MAX, mx = 0u;
    uint64_t top = 0u;
    for (uint32_t k = 0u; k < MP5_YINELEME; ++k) {
        mn = samp[k] < mn ? samp[k] : mn;
        mx = samp[k] > mx ? samp[k] : mx;
        top += samp[k];
    }
    s_bas(&s, "S");
    s_kv(&s, "id", id);
    s_ks(&s, "tur", "ayr");
    s_ks(&s, "akis", a->ad);
    s_ks(&s, "varyant", a->varyant);
    s_ks(&s, "besleme", BESLEME_AD[besleme]);
    s_ks(&s, "islem", alcoz ? "alcoz" : "al");
    s_ks(&s, "onbellek", ONBELLEK_AD[ob]);
    s_kv(&s, "n", MP5_YINELEME);
    s_kv(&s, "bayt", n);
    s_kv(&s, "cerceve", a->n_cerceve);
    s_kv(&s, "min", mn);
    s_kv(&s, "max", mx);
    s_kv(&s, "top", top);
    s_kv(&s, "isinma", isinma);
    ilk_yaz();
    s_kv(&s, "ozet_hata", ozet_hata);
    s_kv(&s, "hata", hata_say);
    s_kv(&s, "sayac_ok", sayac_ok ? 1u : 0u);
    s_kv(&s, "rx_frames", z->rx_frames);
    s_kv(&s, "rx_bytes", z->rx_bytes);
    s_kv(&s, "rx_crc_err", z->rx_crc_err);
    s_kv(&s, "rx_seq_gap", z->rx_seq_gap);
    s_kv(&s, "rx_resync", z->rx_resync_bytes);
    s_gonder(&s);
    dagilim_gonder(id, MP5_YINELEME);
}

static void kod_hazirla(const mp5_kod_t *k)
{
    kod_n = k->n_mesaj;
    for (uint32_t i = 0u; i < kod_n; ++i) { kod_dizi[i] = yapi_ram[k->yapi_idx[i]]; }
    (void)ida_link_gonderici_kur(&gonderici, (ida_link_rol_t)k->rol_gonderen);
}

static void seri_kod(const mp5_kod_t *k, int ob)
{
    const uint32_t id = seri_no++;
    kod_hazirla(k);
    hata_say = 0u;
    uint32_t bayt_hata = 0u, fnv_hata = 0u, isinma = 0u;
    uint32_t c0, c1;
    if (ob == O_SICAK) {
        gonderici.seq = 0u;
        __disable_irq();
        c0 = kart_cevrim();
        const uint32_t n = kodla();
        c1 = kart_cevrim();
        __enable_irq();
        isinma = c1 - c0;
        bayt_hata += (n != k->n_bayt) ? 1u : 0u;
        fnv_hata += (fnv(2166136261u, cikis, n) != k->fnv) ? 1u : 0u;
    }
    for (uint32_t i = 0u; i < MP5_YINELEME; ++i) {
        gonderici.seq = 0u;
        memset(cikis, 0, k->n_bayt + 16u);
        __disable_irq();
        if (ob == O_SOGUK) { kart_icache_bosalt(); }
        c0 = kart_cevrim();
        const uint32_t n = kodla();
        c1 = kart_cevrim();
        __enable_irq();
        samp[i] = c1 - c0;
        bayt_hata += (n != k->n_bayt) ? 1u : 0u;
        fnv_hata += (fnv(2166136261u, cikis, n) != k->fnv) ? 1u : 0u;
    }
    if (bayt_hata != 0u || fnv_hata != 0u || hata_say != 0u) { toplam_hata++; }
    uint32_t mn = UINT32_MAX, mx = 0u;
    uint64_t top = 0u;
    for (uint32_t i = 0u; i < MP5_YINELEME; ++i) {
        mn = samp[i] < mn ? samp[i] : mn;
        mx = samp[i] > mx ? samp[i] : mx;
        top += samp[i];
    }
    s_bas(&s, "S");
    s_kv(&s, "id", id);
    s_ks(&s, "tur", "kod");
    s_ks(&s, "akis", k->ad);
    s_ks(&s, "varyant", k->varyant);
    s_ks(&s, "islem", "kodla");
    s_ks(&s, "onbellek", ONBELLEK_AD[ob]);
    s_kv(&s, "n", MP5_YINELEME);
    s_kv(&s, "bayt", k->n_bayt);
    s_kv(&s, "cerceve", k->n_mesaj);
    s_kv(&s, "min", mn);
    s_kv(&s, "max", mx);
    s_kv(&s, "top", top);
    s_kv(&s, "isinma", isinma);
    ilk_yaz();
    s_kv(&s, "bayt_hata", bayt_hata);
    s_kv(&s, "fnv_hata", fnv_hata);
    s_kv(&s, "hata", hata_say);
    s_kx(&s, "fnv_beklenen", k->fnv);
    s_gonder(&s);
    dagilim_gonder(id, MP5_YINELEME);
}

/* ---------------- doğrulama geçişi (ölçülmez) ---------------- */
static void dogrula_ayr(const mp5_ayr_t *a)
{
    s_bas(&s, "V");
    s_ks(&s, "akis", a->ad);
    s_ks(&s, "varyant", a->varyant);
    s_kx(&s, "bek_cerceve", a->fnv_cerceve);
    s_kx(&s, "bek_yapi", a->fnv_yapi);
    memcpy(gelen, a->bayt, a->n_bayt);
    const uint32_t parca[3] = {a->n_bayt, 64u, 1u};
    for (int b = 0; b < 3; ++b) {
        (void)ida_link_alici_kur(&alici, (ida_link_rol_t)a->rol_alici);
        uint32_t hc = 2166136261u, hy = 2166136261u, nc = 0u, hata = 0u;
        for (uint32_t p = 0u; p < a->n_bayt; p += parca[b]) {
            const uint32_t m = (a->n_bayt - p) < parca[b] ? (a->n_bayt - p) : parca[b];
            uint32_t j = 0u;
            while (j < m) {
                size_t tk = 0u;
                const ida_link_rc_t rc = ida_link_al(&alici, &gelen[p + j], m - j, &tk, &cerceve);
                if (rc == IDA_LINK_CERCEVE) {
                    nc++;
                    const uint8_t kb[7] = {(uint8_t)cerceve.msgid, (uint8_t)(cerceve.msgid >> 8),
                                           (uint8_t)(cerceve.msgid >> 16), (uint8_t)(cerceve.msgid >> 24),
                                           cerceve.seq, cerceve.len, cerceve.yuk_azami};
                    hc = fnv(hc, kb, sizeof kb);
                    hc = fnv(hc, cerceve.yuk, cerceve.yuk_azami);
                    const uint32_t boy = mp5_boy_tablosu[cerceve.msgid - 46000u];
                    if (ida_link_coz(&cerceve, cerceve.msgid, yapi_cikis.b, boy) != IDA_LINK_TAMAM) { hata++; }
                    hy = fnv(hy, yapi_cikis.b, boy);
                } else if (rc != IDA_LINK_BEKLE) {
                    hata++;
                    break;
                }
                j += (uint32_t)tk;
            }
        }
        const bool ok = hata == 0u && nc == a->n_cerceve && hc == a->fnv_cerceve && hy == a->fnv_yapi;
        if (!ok) { toplam_hata++; }
        s_cat(&s, " ");
        s_cat(&s, BESLEME_AD[b]);
        s_cat(&s, "=");
        s_u64(&s, ok ? 1u : 0u);
    }
    s_gonder(&s);
}

/* ---------------- yığın ---------------- */
static inline __attribute__((always_inline)) uint32_t sp_oku(void)
{
    uint32_t sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

static uint32_t yb_alt, yb_ust;
static void __attribute__((noinline)) yigin_boya(void)
{
    const uint32_t sp = sp_oku();
    yb_ust = (sp - 64u) & ~3u;
    yb_alt = yb_ust - YIGIN_PAY;
    for (uint32_t *p = (uint32_t *)yb_alt; (uint32_t)p < yb_ust; p++) { *p = YIGIN_DESEN; }
}

static uint32_t yigin_olc(uint32_t sp_ref)
{
    const uint32_t *p = (const uint32_t *)yb_alt;
    while ((uint32_t)p < yb_ust && *p == YIGIN_DESEN) { p++; }
    if ((uint32_t)p == yb_alt) { return UINT32_MAX; }
    return sp_ref - (uint32_t)p;
}

static const mp5_ayr_t *ayr_bul(const char *ad, const char *varyant)
{
    for (uint32_t i = 0u; i < MP5_AYR_N; ++i) {
        if (strcmp(mp5_ayr[i].ad, ad) == 0 && strcmp(mp5_ayr[i].varyant, varyant) == 0) { return &mp5_ayr[i]; }
    }
    return NULL;
}

static const mp5_kod_t *kod_bul(const char *ad, const char *varyant)
{
    for (uint32_t i = 0u; i < MP5_KOD_N; ++i) {
        if (strcmp(mp5_kod[i].ad, ad) == 0 && strcmp(mp5_kod[i].varyant, varyant) == 0) { return &mp5_kod[i]; }
    }
    return NULL;
}

/* Ölçülen işlevlerin yığın derinliği: main'in yığın işaretçisinden, işlev bir kez çağrılırken
   bozulan en derin adrese (işlevin kendi çerçevesi + kitaplık çağrıları). */
static void yigin_raporu(void)
{
    const mp5_ayr_t *pen = ayr_bul("PEN", "ham");
    const mp5_ayr_t *buy = ayr_bul("BUY", "ham");
    const mp5_kod_t *ks = kod_bul("KS", "ham");
    const mp5_kod_t *buyk = kod_bul("BUYK", "ham");
    if (pen == NULL || buy == NULL || ks == NULL || buyk == NULL) { toplam_hata++; return; }
    static const char *const AD[] = {"ayr_tek", "ayr_p64", "ayr_bayt", "ayr_alcoz", "kodla_KS", "kodla_BUYK",
                                     "ayr_tek_BUY"};
    static const ayr_islevi_t F[] = {ayr_tek, ayr_p64, ayr_bayt, ayr_alcoz};
    s_bas(&s, "Y");
    for (uint32_t i = 0u; i < 7u; ++i) {
        uint32_t sp_ref, d;
        if (i < 4u || i == 6u) {
            const mp5_ayr_t *a = (i == 6u) ? buy : pen;
            memcpy(gelen, a->bayt, a->n_bayt);
            (void)ida_link_alici_kur(&alici, (ida_link_rol_t)a->rol_alici);
            sp_ref = sp_oku();
            yigin_boya();
            (void)F[i == 6u ? 0u : i](gelen, a->n_bayt);
            d = yigin_olc(sp_ref);
        } else {
            kod_hazirla(i == 4u ? ks : buyk);
            sp_ref = sp_oku();
            yigin_boya();
            (void)kodla();
            d = yigin_olc(sp_ref);
        }
        s_kv(&s, AD[i], d);
    }
    s_gonder(&s);
}

/* ---------------- ana ---------------- */
int main(void)
{
    const uint32_t csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    const uint32_t adim = kart_baslat();

    s.n = 0u;
    s_gonder(&s); /* boş satır */
    s_bas(&s, "B");
    s_ks(&s, "derleme", MP5_DERLEME);
    s_ks(&s, "kimlik", MP5_KIMLIK);
    s_kx(&s, "veri_kimlik", MP5_VERI_KIMLIK);
    s_kx(&s, "dialect_hi", (uint32_t)(IDA_LINK_DIALECT_HASH >> 32));
    s_kx(&s, "dialect_lo", (uint32_t)IDA_LINK_DIALECT_HASH);
    s_kv(&s, "yineleme", MP5_YINELEME);
    s_kv(&s, "onbellek_maske", MP5_ONBELLEK);
    s_kv(&s, "sizeof_alici", sizeof(ida_link_alici_t));
    s_kv(&s, "sizeof_gonderici", sizeof(ida_link_gonderici_t));
    s_kv(&s, "sizeof_cerceve", sizeof(ida_link_cerceve_t));
    s_kv(&s, "ayr_n", MP5_AYR_N);
    s_kv(&s, "kod_n", MP5_KOD_N);
    s_kv(&s, "yapi_n", MP5_YAPI_N);
    s_gonder(&s);
    s_bas(&s, "B2");
    s_kx(&s, "rcc_csr", csr);
    s_kx(&s, "idcode", DBGMCU->IDCODE);
    s_kv(&s, "saat_adim", adim);
    s_kv(&s, "saat_hal_rc", kart_saat_hal_rc());
    s_kv(&s, "hesap_hz", kart_saat_hz());
    s_kv(&s, "hal_hz", HAL_RCC_GetSysClockFreq());
    s_kx(&s, "cfgr2", RCC->CFGR2);
    s_kx(&s, "cfgr3", RCC->CFGR3);
    s_kx(&s, "flash_acr", FLASH->ACR);
    s_kx(&s, "icache_cr", ICACHE->CR);
    s_kx(&s, "pwr_svmsr", PWR->SVMSR);
    s_kx(&s, "cpacr", SCB->CPACR);
    s_kv(&s, "uart_hata", kart_uart_hata());
    s_gonder(&s);
    if (adim != 0u) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        for (;;) {
            s_bas(&s, "X");
            s_ks(&s, "neden", "SAAT_HATASI");
            s_kv(&s, "adim", adim);
            s_gonder(&s);
            HAL_Delay(1000u);
        }
    }
    /* Ölçüm boyunca kesme kaynağı yok: SysTick durdurulur (HAL_Delay bundan sonra kullanılmaz). */
    SysTick->CTRL = 0u;

    /* Yapı görüntüleri SRAM'e (ürün yapıları SRAM'dedir). */
    uint32_t dolu = 0u;
    bool sigdi = MP5_YAPI_N <= YAPI_AZAMI_N;
    for (uint32_t i = 0u; sigdi && i < MP5_YAPI_N; ++i) {
        const uint32_t u = (mp5_yapilar[i].boy + 7u) / 8u;
        if (dolu + u > HAVUZ_U64) { sigdi = false; break; }
        memcpy(&havuz[dolu], mp5_yapilar[i].yapi, mp5_yapilar[i].boy);
        yapi_ram[i].msgid = mp5_yapilar[i].msgid;
        yapi_ram[i].boy = mp5_yapilar[i].boy;
        yapi_ram[i].yapi = (const uint8_t *)&havuz[dolu];
        dolu += u;
    }
    bool akis_sigdi = true;
    for (uint32_t i = 0u; i < MP5_AYR_N; ++i) { akis_sigdi = akis_sigdi && mp5_ayr[i].n_bayt <= TAMPON; }
    for (uint32_t i = 0u; i < MP5_KOD_N; ++i) {
        akis_sigdi = akis_sigdi && mp5_kod[i].n_bayt + 16u <= TAMPON && mp5_kod[i].n_mesaj <= 64u;
    }
    if (!sigdi || !akis_sigdi) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        s_bas(&s, "X");
        s_ks(&s, "neden", "TAMPON");
        s_gonder(&s);
        s_bas(&s, "T");
        s_kv(&s, "seri", 0u);
        s_kv(&s, "hata", 1u);
        s_gonder(&s);
        for (;;) { }
    }

    /* Doğrulama geçişi (ölçülmez). */
    for (uint32_t i = 0u; i < MP5_AYR_N; ++i) { dogrula_ayr(&mp5_ayr[i]); }
    yigin_raporu();

    for (int ob = O_SICAK; ob <= O_KAPALI; ++ob) {
        if ((MP5_ONBELLEK & (1u << (uint32_t)ob)) == 0u) { continue; }
        onbellek_kur(ob);
        s_bas(&s, "C");
        s_ks(&s, "onbellek", ONBELLEK_AD[ob]);
        s_kx(&s, "icache_cr", ICACHE->CR);
        s_gonder(&s);
        seri_bos(ob, 0);
        seri_bos(ob, 1);
        for (uint32_t i = 0u; i < MP5_AYR_N; ++i) {
            const mp5_ayr_t *a = &mp5_ayr[i];
            if (a->grup != 0u) { continue; }
            for (int b = B_TEK; b <= B_BAYT; ++b) { seri_ayr(a, b, 0, ob); }
            seri_ayr(a, B_TEK, 1, ob);
            HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_0);
        }
        for (uint32_t i = 0u; i < MP5_KOD_N; ++i) {
            if (mp5_kod[i].grup == 0u) { seri_kod(&mp5_kod[i], ob); }
        }
        if (ob == O_SICAK) {
            for (uint32_t i = 0u; i < MP5_AYR_N; ++i) {
                if (mp5_ayr[i].grup == 1u) { seri_ayr(&mp5_ayr[i], B_TEK, 0, ob); }
            }
            for (uint32_t i = 0u; i < MP5_KOD_N; ++i) {
                if (mp5_kod[i].grup == 1u) { seri_kod(&mp5_kod[i], ob); }
            }
        }
    }
    kart_icache_ac();
    if (toplam_hata != 0u) { HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET); }
    s_bas(&s, "T");
    s_kv(&s, "seri", seri_no);
    s_kv(&s, "hata", toplam_hata);
    s_gonder(&s);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET);
    for (;;) { }
}
