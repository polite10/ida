// W0-MP3a ölçüm prototipi: koşu kaydı ve çözümleyici (bkz. kayit.hpp; atılabilir).
#include "kayit.hpp"

#include "mp3_cerceve.h"

#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <unistd.h>

namespace mp3 {

const char *hukum_ad(Hukum h)
{
    switch (h) {
    case Hukum::Gecti: return "GECTI";
    case Hukum::Gecmedi: return "GECMEDI";
    case Hukum::Gecersiz: return "GECERSIZ";
    case Hukum::Baslamadi: return "BASLAMADI";
    }
    return "?";
}

namespace {

std::string hex8(uint32_t v)
{
    char b[9];
    std::snprintf(b, sizeof b, "%08X", v);
    return b;
}

std::optional<uint64_t> sayi_u(const std::string &t)
{
    if (t.empty() || t.size() > 20) return std::nullopt;
    uint64_t v = 0;
    for (char c : t) {
        if (c < '0' || c > '9') return std::nullopt;
        const auto d = static_cast<uint64_t>(c - '0');
        if (v > (UINT64_MAX - d) / 10) return std::nullopt;
        v = v * 10 + d;
    }
    return v;
}

std::optional<int64_t> sayi_i(const std::string &t)
{
    if (!t.empty() && t[0] == '-') {
        const auto v = sayi_u(t.substr(1));
        if (!v || *v > static_cast<uint64_t>(INT64_MAX)) return std::nullopt;
        return -static_cast<int64_t>(*v);
    }
    const auto v = sayi_u(t);
    if (!v || *v > static_cast<uint64_t>(INT64_MAX)) return std::nullopt;
    return static_cast<int64_t>(*v);
}

std::vector<std::string> bol(const std::string &s)
{
    std::vector<std::string> r;
    size_t i = 0;
    while (i <= s.size()) {
        const size_t j = s.find(' ', i);
        if (j == std::string::npos) { r.push_back(s.substr(i)); break; }
        r.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return r;
}

// µs gösterimi, ns tam sayısından kesin (yuvarlama yok)
std::string us(int64_t ns)
{
    char b[48];
    const char *isaret = ns < 0 ? "-" : "";
    const uint64_t a = ns < 0 ? static_cast<uint64_t>(-ns) : static_cast<uint64_t>(ns);
    std::snprintf(b, sizeof b, "%s%llu.%03llu", isaret, static_cast<unsigned long long>(a / 1000),
                  static_cast<unsigned long long>(a % 1000));
    return b;
}

struct Harita {
    std::map<std::string, std::string> m;
    std::string ad;
    std::vector<std::string> *hata;

    const std::string *ham(const std::string &k) const
    {
        auto it = m.find(k);
        if (it == m.end()) { hata->push_back(ad + ": alan yok: " + k); return nullptr; }
        return &it->second;
    }
    uint64_t u(const std::string &k) const
    {
        const auto *h = ham(k);
        if (!h) return 0;
        const auto v = sayi_u(*h);
        if (!v) { hata->push_back(ad + ": sayı değil: " + k + "=" + *h); return 0; }
        return *v;
    }
    // Temsil aralığıyla (sol Y2): değer `ust`'ü aşarsa kayıt GECERSIZ.
    uint64_t u(const std::string &k, uint64_t ust) const
    {
        const uint64_t v = u(k);
        if (v > ust) hata->push_back(ad + ": aralık dışı: " + k + "=" + std::to_string(v) + " > " + std::to_string(ust));
        return v;
    }
    int64_t i(const std::string &k) const
    {
        const auto *h = ham(k);
        if (!h) return 0;
        const auto v = sayi_i(*h);
        if (!v) { hata->push_back(ad + ": sayı değil: " + k + "=" + *h); return 0; }
        return *v;
    }
    std::string s(const std::string &k) const
    {
        const auto *h = ham(k);
        return h ? *h : std::string();
    }
};

struct RSatir {
    uint64_t sira;
    int64_t plan, yb, ye, yanit;   // -1: yok
};

}  // namespace

std::string satir_imzala(const std::string &govde)
{
    const uint32_t c = mp3_crc32(reinterpret_cast<const uint8_t *>(govde.data()), govde.size());
    return govde + " *" + hex8(c);
}

bool KayitYazici::ac(const std::string &yol, std::string &hata)
{
    const int fd = ::open(yol.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) { hata = yol + ": " + std::strerror(errno) + " (var olan kayıt ezilmez)"; return false; }
    ::close(fd);
    o_.open(yol, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!o_) { hata = yol + ": açılamadı"; return false; }
    acik_ = true;
    no_ = 0;
    return true;
}

void KayitYazici::yaz(const std::string &tur, const std::string &govde)
{
    std::string g = tur + " " + std::to_string(no_++);
    if (!govde.empty()) g += " " + govde;
    o_ << satir_imzala(g) << '\n';
}

void KayitYazici::alanli(const std::string &tur, const Alanlar &a)
{
    std::string g;
    for (const auto &[k, v] : a) {
        if (!g.empty()) g += ' ';
        std::string t = v.empty() ? std::string("-") : v;
        for (char &c : t) {
            if (c == ' ' || c == '\n' || c == '\r' || c == '*' || c == '=') c = '_';
        }
        g += k + "=" + t;
    }
    yaz(tur, g);
}

void KayitYazici::konumsal(const std::string &tur, const std::string &govde) { yaz(tur, govde); }

bool KayitYazici::kapat()
{
    if (!acik_) return false;
    yaz("S", "satir=" + std::to_string(no_));
    o_.flush();
    const bool ok = static_cast<bool>(o_);
    o_.close();
    acik_ = false;
    return ok;
}

uint64_t akis_araligi(uint32_t etkin_baud, uint16_t yuk, double oran)
{
    const double boy = static_cast<double>(yuk) + MP3_EK;
    const double bps = oran * static_cast<double>(etkin_baud) / 10.0;
    if (!(bps > 0) || !std::isfinite(bps)) return 0;
    return static_cast<uint64_t>(std::ceil(boy * 1e9 / bps));
}

uint64_t echo_araligi(uint64_t hiz_milli)
{
    if (hiz_milli == 0 || 1000000000000ull % hiz_milli != 0) return 0;
    return 1000000000000ull / hiz_milli;
}

std::string profil_adi(bool echo, uint16_t yuk, uint64_t oran_ppm, uint64_t hiz_milli)
{
    if (echo) {
        if ((yuk == 64 || yuk == 256) && (hiz_milli == 200000 || hiz_milli == 400000)) return "mp3-echo-v1";
        return "deneysel";
    }
    if (yuk == 256 && oran_ppm == 800000) return "mp3-akis-v1";
    return "deneysel";
}

int64_t takvim_payi(int64_t pencere_ns) { return std::max<int64_t>(TAKVIM_PAY_TABAN_NS, pencere_ns / 100); }

int64_t yuzdelik(const std::vector<int64_t> &v, uint64_t pay, uint64_t payda)
{
    if (v.empty()) return 0;
    const uint64_t n = v.size();
    uint64_t k = (pay * n + payda - 1) / payda;   // ⌈q·n⌉
    if (k == 0) k = 1;
    if (k > n) k = n;
    return v[k - 1];
}

AnalizSonuc analiz_dosya(const std::string &yol, const AnalizBeklenti &b)
{
    std::ifstream f(yol, std::ios::binary);
    if (!f) {
        AnalizSonuc s;
        s.nedenler.push_back("kayıt açılamadı: " + yol);
        return s;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return analiz_metin(ss.str(), b);
}

AnalizSonuc analiz_metin(const std::string &icerik, const AnalizBeklenti &bek)
{
    mp3_hazirla();
    AnalizSonuc sonuc;
    std::vector<std::string> butunluk;   // kayıt bütünlüğü (GECERSIZ)
    auto gecersiz = [&]() {
        sonuc.hukum = Hukum::Gecersiz;
        sonuc.nedenler = butunluk;
        return sonuc;
    };

    // ---- 1. satır ve CRC bütünlüğü ----
    if (icerik.empty() || icerik.back() != '\n') {
        butunluk.push_back("kayıt boş ya da son satır yarım (satır sonu yok)");
        return gecersiz();
    }
    std::vector<std::string> satirlar;
    {
        size_t i = 0;
        while (i < icerik.size()) {
            const size_t j = icerik.find('\n', i);
            satirlar.push_back(icerik.substr(i, j - i));
            i = j + 1;
        }
    }
    std::optional<Harita> B, P, K;
    std::optional<uint64_t> S_satir;
    std::vector<RSatir> R;
    std::vector<std::string> alan_hata;
    for (size_t no = 0; no < satirlar.size(); ++no) {
        const std::string &l = satirlar[no];
        const std::string yer = "satır " + std::to_string(no) + ": ";
        if (S_satir) { butunluk.push_back(yer + "S satırından sonra veri var"); return gecersiz(); }
        const size_t y = l.rfind(" *");
        if (y == std::string::npos || l.size() != y + 10) { butunluk.push_back(yer + "imza biçimi bozuk"); return gecersiz(); }
        const std::string govde = l.substr(0, y);
        uint32_t c = 0;
        for (size_t k = y + 2; k < l.size(); ++k) {
            const char h = l[k];
            uint32_t d;
            if (h >= '0' && h <= '9') d = static_cast<uint32_t>(h - '0');
            else if (h >= 'A' && h <= 'F') d = static_cast<uint32_t>(h - 'A' + 10);
            else { butunluk.push_back(yer + "imza onaltılık değil"); return gecersiz(); }
            c = (c << 4) | d;
        }
        if (mp3_crc32(reinterpret_cast<const uint8_t *>(govde.data()), govde.size()) != c) {
            butunluk.push_back(yer + "CRC tutmuyor");
            return gecersiz();
        }
        const auto t = bol(govde);
        if (t.size() < 2) { butunluk.push_back(yer + "eksik"); return gecersiz(); }
        const auto sn = sayi_u(t[1]);
        if (!sn || *sn != no) { butunluk.push_back(yer + "satır numarası kesintili"); return gecersiz(); }
        const std::string &tur = t[0];
        if (no == 0 && tur != "B") { butunluk.push_back("ilk satır B değil"); return gecersiz(); }
        if (tur == "R") {
            if (!B || P) { butunluk.push_back(yer + "R yerinde değil"); return gecersiz(); }
            if (t.size() != 7) { butunluk.push_back(yer + "R alan sayısı"); return gecersiz(); }
            RSatir r{};
            const auto s0 = sayi_u(t[2]);
            if (!s0) { butunluk.push_back(yer + "R sıra"); return gecersiz(); }
            r.sira = *s0;
            int64_t *hedef[4] = {&r.plan, &r.yb, &r.ye, &r.yanit};
            for (int k = 0; k < 4; ++k) {
                const std::string &v = t[static_cast<size_t>(3 + k)];
                if (v == "-" && k > 0) { *hedef[k] = -1; continue; }
                const auto x = sayi_u(v);
                if (!x || *x > static_cast<uint64_t>(INT64_MAX)) { butunluk.push_back(yer + "R zaman"); return gecersiz(); }
                *hedef[k] = static_cast<int64_t>(*x);
            }
            R.push_back(r);
            continue;
        }
        Harita h;
        h.ad = tur;
        h.hata = &alan_hata;
        for (size_t k = 2; k < t.size(); ++k) {
            const size_t e = t[k].find('=');
            if (e == std::string::npos || e == 0 || e + 1 == t[k].size()) {
                butunluk.push_back(yer + "alan biçimi: " + t[k]);
                return gecersiz();
            }
            if (!h.m.emplace(t[k].substr(0, e), t[k].substr(e + 1)).second) {
                butunluk.push_back(yer + "yinelenen alan: " + t[k]);
                return gecersiz();
            }
        }
        if (tur == "B") {
            if (B || no != 0) { butunluk.push_back(yer + "ikinci B"); return gecersiz(); }
            B = h;
        } else if (tur == "P") {
            if (!B || P) { butunluk.push_back(yer + "P yerinde değil"); return gecersiz(); }
            P = h;
        } else if (tur == "K") {
            if (!P || K) { butunluk.push_back(yer + "K yerinde değil"); return gecersiz(); }
            K = h;
        } else if (tur == "S") {
            if (!P) { butunluk.push_back(yer + "S, P'den önce"); return gecersiz(); }
            const auto v = sayi_u(h.m.count("satir") ? h.m["satir"] : "");
            if (!v || *v != no || h.m.size() != 1) { butunluk.push_back(yer + "S satır sayısı tutmuyor"); return gecersiz(); }
            S_satir = *v;
        } else {
            butunluk.push_back(yer + "bilinmeyen tür: " + tur);
            return gecersiz();
        }
    }
    if (!S_satir) { butunluk.push_back("S (son) satırı yok: kayıt kesik"); return gecersiz(); }
    if (!B || !P) { butunluk.push_back("B ya da P yok"); return gecersiz(); }

    // ---- 2. alanlar ----
    const Harita &b = *B;
    const Harita &p = *P;
    // Temsil aralıkları (sol Y2): B ayarları KUR ve araç sınırları; P host sayaç/zamanları INT64_MAX; K tel u32.
    constexpr uint64_t U32 = 0xFFFFFFFFull, I64 = static_cast<uint64_t>(INT64_MAX);
    const uint64_t surum = b.u("surum", 255);
    const uint64_t kosu = b.u("kosu", U32);
    const std::string mod = b.s("mod");
    const std::string profil = b.s("profil");
    const uint64_t baud = b.u("baud", 20000000), yuk = b.u("yuk", MP3_AZAMI_YUK), pc_adet = b.u("pc_adet", U32),
                   kart_adet = b.u("kart_adet", U32);
    const uint64_t aralik = b.u("aralik_ns", U32), zaman_asimi_ms = b.u("zaman_asimi_ms", 60000);
    const uint64_t oran_ppm = b.u("oran_ppm", 1000000), hiz_milli = b.u("hiz_milli", 100000000000ull);
    const uint64_t kart_hesap = b.u("kart_hesap_baud", U32), pc_hesap = b.u("pc_hesap_baud", U32);
    const uint64_t kart_usart_hz = b.u("kart_usart_hz", U32), kart_brr = b.u("kart_brr", 0xFFFF),
                   kart_over8 = b.u("kart_over8", 1);
    const int64_t kart_ppm = b.i("kart_hata_ppm");
    if (kart_ppm < INT32_MIN || kart_ppm > INT32_MAX) alan_hata.push_back("B: kart_hata_ppm int32 dışı");
    const uint64_t azami_bekleyen = b.u("azami_bekleyen", 100000);
    std::map<std::string, uint64_t> bv;
    for (const char *k : {"os_istenen", "os_geri_i", "os_geri_o"}) bv[k] = b.u(k, U32);
    for (const char *k : {"os_tcsetattr", "os_ioctl", "kart_kabul"}) bv[k] = b.u(k, 1);
    bv["os_errno"] = b.u("os_errno", static_cast<uint64_t>(INT32_MAX));
    bv["kart_neden"] = b.u("kart_neden", 255);
    for (const char *k : {"port", "kart_kimlik", "saat", "utc"}) (void)b.s(k);
    const std::string asama = p.s("asama");
    const uint64_t p_kosu = p.u("kosu", U32);
    static const char *const P_SAYAC[] = {
        "tx_cerceve", "tx_bayt", "tx_atlanan", "tx_gec_azami_ns", "yazma_hata", "yazma_azami_ns", "takvim_kesildi",
        "rx_okunan_bayt", "esle_oncesi_bayt", "ozet_sonrasi_bayt", "rx_bayt", "rx_iyi", "rx_atilan_bayt",
        "rx_baslik_hata", "rx_surum_hata", "rx_uzunluk_hata", "rx_crc_hata", "rx_kosu_hata", "rx_tur_hata",
        "rx_icerik_hata", "rx_alinan", "rx_beklenen", "rx_eksik", "rx_tekrar", "rx_sirasiz", "rx_eski", "rx_kontrol",
        "rx_kontrol_bayt", "fazla_yanit", "kopma", "esle_deneme", "ozet_deneme", "donus_ok", "t_son_plan_ns",
        "t_son_yaz_ns", "t_son_rx_ns", "azami_bekleyen_gozlenen"};
    std::map<std::string, uint64_t> pv;
    for (const char *k : P_SAYAC) pv[k] = p.u(k, I64);
    for (const char *k : {"takvim_kesildi", "kopma", "donus_ok"}) (void)p.u(k, 1);
    if (p.m.size() != std::size(P_SAYAC) + 2) alan_hata.push_back("P: alan sayısı " + std::to_string(p.m.size()));
    std::map<std::string, uint64_t> kv;
    if (K) {
        for (unsigned i = 0; i < static_cast<unsigned>(MP3_OZET_N); ++i) kv[mp3_ozet_ad(i)] = K->u(mp3_ozet_ad(i), U32);
        if (K->m.size() != static_cast<size_t>(MP3_OZET_N)) alan_hata.push_back("K: alan sayısı " + std::to_string(K->m.size()));
    }
    if (!alan_hata.empty()) { butunluk.insert(butunluk.end(), alan_hata.begin(), alan_hata.end()); return gecersiz(); }
    if (surum != MP3_SURUM) { butunluk.push_back("kayıt sürümü " + std::to_string(surum)); return gecersiz(); }
    if (mod != "akis" && mod != "echo") { butunluk.push_back("mod bilinmiyor: " + mod); return gecersiz(); }
    const bool echo = mod == "echo";
    if (p_kosu != kosu) { butunluk.push_back("P koşu kimliği B ile tutmuyor"); return gecersiz(); }
    if (!echo && !R.empty()) { butunluk.push_back("akış kaydında R satırı var"); return gecersiz(); }
    if (yuk > MP3_AZAMI_YUK || baud == 0 || baud > 0xFFFFFFFFull) { butunluk.push_back("B yük/baud geçersiz"); return gecersiz(); }
    sonuc.profil = profil;

    // ---- 3. profil ve takvim tanımı (her aşamada; kayıt kendi ayarlarından yeniden üretilebilmeli) ----
    const std::string bek_profil = profil_adi(echo, static_cast<uint16_t>(yuk), oran_ppm, hiz_milli);
    if (profil != bek_profil) { butunluk.push_back("profil " + profil + " ayarlarla tutmuyor (beklenen " + bek_profil + ")"); return gecersiz(); }
    const uint64_t etkin_baud = std::min<uint64_t>(baud, pc_hesap > 0 ? pc_hesap : baud);
    uint64_t bek_aralik = 0;
    if (echo) {
        bek_aralik = echo_araligi(hiz_milli);
        if (oran_ppm != 0) { butunluk.push_back("echo kaydında oran_ppm"); return gecersiz(); }
    } else {
        if (hiz_milli != 0 || oran_ppm == 0 || oran_ppm > 1000000) { butunluk.push_back("akış oranı geçersiz"); return gecersiz(); }
        bek_aralik = akis_araligi(static_cast<uint32_t>(etkin_baud), static_cast<uint16_t>(yuk),
                                  static_cast<double>(oran_ppm) / 1e6);
    }
    if (bek_aralik == 0 || aralik != bek_aralik) {
        butunluk.push_back("takvim çözülemiyor: aralik_ns " + std::to_string(aralik) + " != " + std::to_string(bek_aralik));
        return gecersiz();
    }
    if (pc_adet == 0) { butunluk.push_back("pc_adet 0"); return gecersiz(); }
    const uint64_t boy = yuk + MP3_EK;
    const double kap = static_cast<double>(etkin_baud) / 10.0;
    const double yon_bps = static_cast<double>(boy) * 1e9 / static_cast<double>(aralik);
    const double oran = yon_bps / kap;
    const double sinir = echo ? ECHO_KAPASITE_SINIR : AKIS_ORAN_SINIR;
    const bool kapasite_asti = oran > sinir * (1.0 + 1e-12);
    char kap_metin[200];
    std::snprintf(kap_metin, sizeof kap_metin, "yön başına %.0f B/s, 8N1 kapasitesinin (%llu baud) %.2f%%'i > %.0f%%",
                  yon_bps, static_cast<unsigned long long>(etkin_baud), 100.0 * oran, 100.0 * sinir);

    // ---- 4. aşama ----
    if (asama == "kur_ret" || asama == "os_hiz_ret" || asama == "esle_yok" || asama == "kur_uyusmaz") {
        sonuc.hukum = Hukum::Baslamadi;
        sonuc.nedenler.push_back("koşu başlamadı: " + asama + " (hız doğrulanmadı)");
        sonuc.rapor = "mod=" + mod + " profil=" + profil + " baud=" + std::to_string(baud) + " asama=" + asama +
                      " donus_ok=" + std::to_string(pv["donus_ok"]) + "\n";
        return sonuc;
    }
    if (asama == "kapasite") {
        if (!kapasite_asti || K || !R.empty() || pv["tx_cerceve"] != 0) {
            butunluk.push_back("asama=kapasite ama kayıt kapasite aşımını göstermiyor ya da trafik var");
            return gecersiz();
        }
        sonuc.hukum = Hukum::Gecmedi;
        sonuc.nedenler.push_back(std::string("kapasite (koşturulmadı): ") + kap_metin);
        sonuc.rapor = "mod=" + mod + " profil=" + profil + " baud=" + std::to_string(baud) + " yuk=" + std::to_string(yuk) +
                      " aralik_ns=" + std::to_string(aralik) + " asama=kapasite\n";
        return sonuc;
    }
    if (asama != "tamam") { butunluk.push_back("koşu kesildi/tamamlanmadı: asama=" + asama); return gecersiz(); }
    if (!K) { butunluk.push_back("kart özeti (K) yok"); return gecersiz(); }

    // ---- 5. iç tutarlılık (tutmazsa GECERSIZ) ----
    std::vector<std::string> &t = butunluk;
    // Taşma denetimli aritmetik (sol Y2): sonuç INT64_MAX'ı aşarsa kayıt GECERSIZ (işaretli zamana güvenle çevrilir).
    bool tasti = false;
    auto ek = [&](uint64_t a, uint64_t c) -> uint64_t {
        uint64_t r = 0;
        if (__builtin_add_overflow(a, c, &r) || r > I64) { tasti = true; return I64; }
        return r;
    };
    auto ca = [&](uint64_t a, uint64_t c) -> uint64_t {
        uint64_t r = 0;
        if (__builtin_mul_overflow(a, c, &r) || r > I64) { tasti = true; return I64; }
        return r;
    };
    auto topla = [&](std::initializer_list<uint64_t> l) {
        uint64_t r = 0;
        for (uint64_t x : l) r = ek(r, x);
        return r;
    };
    auto ic = [&](const std::string &ad, uint64_t a, uint64_t c) {
        if (a != c) t.push_back(ad + ": " + std::to_string(a) + " != " + std::to_string(c));
    };
    ic("K koşu", kv["kosu"], kosu & 0xFFFFFFFFu);
    ic("K mod", kv["mod"], echo ? MP3_MOD_ECHO : MP3_MOD_AKIS);
    ic("K baud", kv["baud"], baud);
    ic("kart kabulü", bv["kart_kabul"], 1);
    ic("kart ret nedeni", bv["kart_neden"], 0);
    ic("OS tcsetattr", bv["os_tcsetattr"], 1);
    ic("OS hız ayarı (IOSSIOSPEED)", bv["os_ioctl"], 1);
    ic("OS istenen hız", bv["os_istenen"], baud);
    ic("OS geri okuma (giriş hızı)", bv["os_geri_i"], baud);
    ic("OS geri okuma (çıkış hızı)", bv["os_geri_o"], baud);
    // BRR'den geri hesap (ST LL_USART_GetBaudRate ile aynı çözüm): yazılan ayar bildirilen hızı vermeli
    const uint32_t brr_baud = mp3_brr_baud(static_cast<uint32_t>(kart_usart_hz), static_cast<uint32_t>(kart_brr),
                                           static_cast<uint32_t>(kart_over8));
    if (kart_usart_hz == 0 || kart_over8 > 1 || brr_baud == 0) t.push_back("kart BRR/OVER8/saat geçersiz");
    ic("BRR'den hesaplanan baud / kartın bildirdiği", brr_baud, kart_hesap);
    ic("PC'nin hesapladığı baud / kartın bildirdiği", pc_hesap, kart_hesap);
    {
        mp3_baud_t yeniden{};
        if (mp3_baud_hesapla(static_cast<uint32_t>(kart_usart_hz), static_cast<uint32_t>(baud), &yeniden) != 0 ||
            yeniden.brr != kart_brr || yeniden.over8 != kart_over8)
            t.push_back("BRR/OVER8 seçimi kayıttaki saat ve hızla yeniden üretilemiyor");
        const int64_t ppm = (static_cast<int64_t>(brr_baud) - static_cast<int64_t>(baud)) * 1000000 / static_cast<int64_t>(baud);
        if (ppm != kart_ppm) t.push_back("kart_hata_ppm yeniden hesapla tutmuyor");
    }
    // çerçeve muhasebesi: rx_iyi = veri (tekil) + hatalı/fazla veri + denetim
    ic("PC rx_iyi muhasebesi", pv["rx_iyi"],
       topla({pv["rx_alinan"], pv["rx_tekrar"], pv["rx_eski"], pv["rx_icerik_hata"], pv["rx_tur_hata"], pv["rx_kosu_hata"],
              pv["fazla_yanit"], pv["rx_kontrol"]}));
    ic("kart rx_iyi muhasebesi", kv["rx_iyi"],
       topla({kv["rx_alinan"], kv["rx_tekrar"], kv["rx_eski"], kv["rx_icerik_hata"], kv["rx_tur_hata"], kv["rx_kosu_hata"],
              kv["rx_kontrol"]}));
    if (pv["rx_kontrol"] < 1) t.push_back("PC penceresinde OZET yok");
    if (kv["rx_kontrol"] < 1) t.push_back("kart penceresinde BITIR yok");
    ic("PC denetim baytı (ESLE_YANIT yinelemeleri + 1 OZET)", pv["rx_kontrol_bayt"],
       ek(ca(pv["rx_kontrol"] - (pv["rx_kontrol"] ? 1 : 0), MP3_EK), MP3_EK + MP3_OZET_BOY));
    ic("kart denetim baytı (yükü boş ESLE/BITIR)", kv["rx_kontrol_bayt"], ca(kv["rx_kontrol"], MP3_EK));
    ic("PC okunan bayt = ESLE öncesi + pencere + OZET sonrası", pv["rx_okunan_bayt"],
       topla({pv["esle_oncesi_bayt"], pv["rx_bayt"], pv["ozet_sonrasi_bayt"]}));
    const bool pc_hatasiz = topla({pv["rx_tekrar"], pv["rx_eski"], pv["rx_icerik_hata"], pv["rx_tur_hata"], pv["rx_kosu_hata"],
                                   pv["fazla_yanit"], pv["rx_atilan_bayt"]}) == 0;
    const bool kart_hatasiz = topla({kv["rx_tekrar"], kv["rx_eski"], kv["rx_icerik_hata"], kv["rx_tur_hata"],
                                     kv["rx_kosu_hata"], kv["rx_atilan_bayt"]}) == 0;
    if (pc_hatasiz) ic("PC pencere baytı", pv["rx_bayt"], ek(ca(pv["rx_alinan"], boy), pv["rx_kontrol_bayt"]));
    if (kart_hatasiz) ic("kart pencere baytı", kv["rx_bayt"], ek(ca(kv["rx_alinan"], boy), kv["rx_kontrol_bayt"]));
    ic("PC tel baytı / çerçeve", pv["tx_bayt"], ca(pv["tx_cerceve"], boy));
    ic("kart tel baytı / çerçeve", kv["tx_bayt"], ca(kv["tx_cerceve"], boy));
    if (pv["rx_alinan"] > pv["rx_beklenen"] || kv["rx_alinan"] > kv["rx_beklenen"]) t.push_back("alınan > beklenen sıra");
    if (ek(pv["tx_cerceve"], pv["tx_atlanan"]) > pc_adet) t.push_back("PC gönderilen + atlanan > plan");
    // ---- takvim kronolojisi (sol Y3; imkânsız sıra GECERSIZ). Zamanlar tam sayı ns (PC) ve µs (kart); yuvarlama
    // payı yok: kart planı firmware ile aynı tam sayı bölmesiyle plan_us = ⌊(k−1)·aralık/1000⌋ hesaplanır ve kart
    // tx_son_us'u gecikmeyle aynı damgadan yazar. PC: plan ≤ son yazma sonu ≤ plan + azami gecikme + azami yazma.
    const uint64_t pencere = ca(pc_adet, aralik);
    if (pv["tx_cerceve"] == 0) {
        if (pv["t_son_yaz_ns"] != 0 || pv["t_son_plan_ns"] != 0) t.push_back("PC hiç göndermediği halde son zaman var");
    } else {
        if (pv["t_son_yaz_ns"] < pv["t_son_plan_ns"]) t.push_back("PC son yazma sonu son plandan önce");
        if (pv["t_son_yaz_ns"] > topla({pv["t_son_plan_ns"], pv["tx_gec_azami_ns"], pv["yazma_azami_ns"]}))
            t.push_back("PC son yazma sonu > son plan + azami gecikme + azami yazma süresi");
        if (pv["t_son_plan_ns"] >= pencere) t.push_back("PC son plan pencerenin dışında");
    }
    if (echo) {
        ic("echo kart_adet", kart_adet, 0);
        ic("echo kart planı", kv["tx_plan"], 0);
    } else {
        ic("akış kart planı / B", kv["tx_plan"], kart_adet);
        ic("akış kart/PC planı", kart_adet, pc_adet);
        ic("akış atlanan", pv["tx_atlanan"], 0);
        if (pv["tx_cerceve"] > 0) ic("son planlı gönderim zamanı", pv["t_son_plan_ns"], ca(pv["tx_cerceve"] - 1, aralik));
        if (kv["tx_cerceve"] == 0) {
            if (kv["tx_son_us"] != 0) t.push_back("kart hiç göndermediği halde son gönderim zamanı var");
        } else {
            const uint64_t kart_plan_us = ca(kv["tx_cerceve"] - 1, aralik) / 1000u;
            if (kv["tx_son_us"] < kart_plan_us) t.push_back("kart son gönderimi son planından önce (" +
                std::to_string(kv["tx_son_us"]) + " < " + std::to_string(kart_plan_us) + " µs)");
            if (kv["tx_son_us"] > ek(kart_plan_us, kv["tx_gec_azami_us"]))
                t.push_back("kart son gönderimi > son plan + azami gecikme (" + std::to_string(kv["tx_son_us"]) + " > " +
                            std::to_string(kart_plan_us) + " + " + std::to_string(kv["tx_gec_azami_us"]) + " µs)");
            if (kv["tx_son_us"] > kv["sure_us"]) t.push_back("kart son gönderimi koşu süresinden (ESLE→BITIR) sonra");
        }
    }

    // ---- 6. echo R zaman çizelgesi (tutmazsa GECERSIZ) ----
    std::vector<int64_t> rtt_plan, rtt_yaz, yazma;
    uint64_t zaman_asimi = 0, atlanan = 0, yanitli = 0, gonderilen = 0, gec_r = 0;
    int64_t r_gec_azami = 0, r_yazma_azami = 0, r_son_ye = -1, r_son_plan = 0, r_son_yanit = 0;
    if (echo) {
        if (R.size() != pc_adet) t.push_back("R satırı sayısı " + std::to_string(R.size()) + " != pc_adet " + std::to_string(pc_adet));
        const int64_t za_ns = static_cast<int64_t>(zaman_asimi_ms) * 1000000;
        int64_t onceki_ye = -1;
        // Bekleyen kuyruğunun R'den bağımsız yeniden üretimi (sol Y4; tanım oturum.hpp BekleyenKuyruk): k'nin karar
        // anı t_k = yazma başı. j < k bekler ⇔ t_k ≤ E_j, E_j = min(yanıt_j − 1, yazma_sonu_j + zaman_aşımı)
        // (yanıt yoksa yalnız ikincisi). Doluluk k eklendikten sonra sayılır (k dahil).
        std::priority_queue<int64_t, std::vector<int64_t>, std::greater<int64_t>> bekleyen_son;
        uint64_t yeniden_azami = 0;
        for (size_t i = 0; i < R.size() && t.empty(); ++i) {
            const RSatir &x = R[i];
            const std::string yer = "R " + std::to_string(i) + ": ";
            if (x.sira != i) { t.push_back(yer + "sıra kesintili"); break; }
            if (static_cast<uint64_t>(x.plan) != ca(i, aralik) || tasti) { t.push_back(yer + "plan zamanı sıra × aralık değil"); break; }
            if (x.yb < 0) {
                if (x.ye >= 0 || x.yanit >= 0) { t.push_back(yer + "gönderilmeyen istekte zaman var"); break; }
                atlanan++;
                continue;
            }
            if (x.ye < 0 || x.yb < x.plan || x.ye < x.yb || x.yb < onceki_ye) { t.push_back(yer + "yazma zaman sırası bozuk"); break; }
            onceki_ye = x.ye;
            gonderilen++;
            while (!bekleyen_son.empty() && bekleyen_son.top() < x.yb) bekleyen_son.pop();
            yeniden_azami = std::max<uint64_t>(yeniden_azami, bekleyen_son.size() + 1);
            {
                const int64_t zs = static_cast<int64_t>(ek(static_cast<uint64_t>(x.ye), static_cast<uint64_t>(za_ns)));
                bekleyen_son.push(x.yanit >= 0 ? std::min(x.yanit - 1, zs) : zs);
            }
            r_gec_azami = std::max(r_gec_azami, x.yb - x.plan);
            r_yazma_azami = std::max(r_yazma_azami, x.ye - x.yb);
            r_son_ye = x.ye;
            r_son_plan = x.plan;
            yazma.push_back(x.ye - x.yb);
            if (x.yanit < 0) { zaman_asimi++; continue; }
            if (x.yanit < x.yb) { t.push_back(yer + "yanıt yazmadan önce"); break; }
            yanitli++;
            r_son_yanit = std::max(r_son_yanit, x.yanit);
            if (x.yanit - x.ye > za_ns) { zaman_asimi++; gec_r++; continue; }
            rtt_plan.push_back(x.yanit - x.plan);
            rtt_yaz.push_back(x.yanit - x.yb);
        }
        if (t.empty()) {
            ic("R gönderilen / P tx_cerceve", gonderilen, pv["tx_cerceve"]);
            ic("R atlanan / P tx_atlanan", atlanan, pv["tx_atlanan"]);
            ic("R yanıtlı / P rx_alinan", yanitli, pv["rx_alinan"]);
            ic("R azami takvim gecikmesi / P", static_cast<uint64_t>(r_gec_azami), pv["tx_gec_azami_ns"]);
            ic("R azami yazma süresi / P", static_cast<uint64_t>(r_yazma_azami), pv["yazma_azami_ns"]);
            if (gonderilen > 0) {
                ic("R son yazma sonu / P", static_cast<uint64_t>(r_son_ye), pv["t_son_yaz_ns"]);
                ic("R son plan / P", static_cast<uint64_t>(r_son_plan), pv["t_son_plan_ns"]);
            }
            if (yanitli > 0) ic("R son yanıt / P", static_cast<uint64_t>(r_son_yanit), pv["t_son_rx_ns"]);
            if (pv["azami_bekleyen_gozlenen"] > azami_bekleyen) t.push_back("gözlenen bekleyen > kuyruk sınırı");
            if (yeniden_azami > azami_bekleyen)
                t.push_back("R'den yeniden üretilen azami bekleyen " + std::to_string(yeniden_azami) + " > kuyruk sınırı " +
                            std::to_string(azami_bekleyen));
            // Çevrim içi ölçüm yeniden üretilenden küçük olamaz (yanıtın görünür olması gecikebilir; büyük olabilir).
            if (pv["azami_bekleyen_gozlenen"] < yeniden_azami)
                t.push_back("P azami bekleyen " + std::to_string(pv["azami_bekleyen_gozlenen"]) + " < R'den yeniden üretilen " +
                            std::to_string(yeniden_azami));
            if (gonderilen == 0 && pv["azami_bekleyen_gozlenen"] != 0) t.push_back("gönderim yokken bekleyen > 0");
        }
    }
    if (tasti) t.push_back("aritmetik taşma: kayıt değerleri temsil edilemiyor");
    if (!t.empty()) return gecersiz();

    // ---- 7. ölçütler (tutmazsa GECMEDI) ----
    std::vector<std::string> &n = sonuc.nedenler;
    auto esit = [&](const std::string &ad, uint64_t a, uint64_t c) {
        if (a != c) n.push_back(ad + ": " + std::to_string(a) + " != " + std::to_string(c));
    };
    if (kapasite_asti) n.push_back(std::string("kapasite: ") + kap_metin);
    if (bek.kampanya && profil == "deneysel") n.push_back("dondurulmuş kampanya profili değil (profil=deneysel)");
    for (const char *k : {"tx_atlanan", "yazma_hata", "takvim_kesildi", "rx_atilan_bayt", "rx_baslik_hata", "rx_surum_hata",
                          "rx_uzunluk_hata", "rx_crc_hata", "rx_kosu_hata", "rx_tur_hata", "rx_icerik_hata", "rx_eksik",
                          "rx_tekrar", "rx_sirasiz", "rx_eski", "fazla_yanit", "kopma"}) {
        if (pv[k] != 0) n.push_back(std::string("PC ") + k + "=" + std::to_string(pv[k]));
    }
    for (const char *k : {"rx_atilan_bayt", "rx_baslik_hata", "rx_surum_hata", "rx_uzunluk_hata", "rx_crc_hata",
                          "rx_kosu_hata", "rx_tur_hata", "rx_icerik_hata", "rx_eksik", "rx_tekrar", "rx_sirasiz",
                          "rx_eski", "uart_ore", "uart_fe", "uart_ne", "uart_pe", "dma_rx_hata", "dma_tx_hata",
                          "rx_tasma", "rx_tasma_bayt", "tx_eksik", "echo_dusen"}) {
        if (kv[k] != 0) n.push_back(std::string("kart ") + k + "=" + std::to_string(kv[k]));
    }
    if (pv["donus_ok"] != 1) n.push_back("denetim hızına dönüş doğrulanmadı");
    // iki ucun sayaç ve bayt eşleşmesi
    esit("PC gönderilen / plan", pv["tx_cerceve"], pc_adet);
    esit("kart alınan / PC gönderilen", kv["rx_alinan"], pv["tx_cerceve"]);
    // beklenen sıra = gönderilen en büyük sıra + 1 (akışta gönderilen sayısı; echoda atlanan yoksa aynı)
    const uint64_t pc_son_sira = echo ? (gonderilen ? static_cast<uint64_t>(r_son_plan) / aralik + 1 : 0) : pv["tx_cerceve"];
    esit("kart beklenen sıra / PC gönderilen son sıra + 1", kv["rx_beklenen"], pc_son_sira);
    esit("PC alınan / kart gönderilen", pv["rx_alinan"], kv["tx_cerceve"]);
    if (!echo) esit("PC beklenen sıra / kart gönderilen", pv["rx_beklenen"], kv["tx_cerceve"]);
    esit("kart veri baytı alınan / PC gönderilen", kv["rx_bayt"] - std::min(kv["rx_bayt"], kv["rx_kontrol_bayt"]), pv["tx_bayt"]);
    esit("PC veri baytı alınan / kart gönderilen", pv["rx_bayt"] - std::min(pv["rx_bayt"], pv["rx_kontrol_bayt"]), kv["tx_bayt"]);
    if (echo) {
        esit("kart yanıt / kart alınan istek", kv["tx_cerceve"], kv["rx_alinan"]);
    } else {
        esit("kart gönderilen / plan", kv["tx_cerceve"], kart_adet);
    }
    // takvim (değerler §2'de INT64_MAX ile sınırlandı, toplamlar §5'te taşma denetimli)
    const int64_t pay = takvim_payi(static_cast<int64_t>(pencere));
    const int64_t pencere_pay = static_cast<int64_t>(ek(pencere, static_cast<uint64_t>(pay)));
    if (static_cast<int64_t>(pv["tx_gec_azami_ns"]) > TAKVIM_GEC_AZAMI_NS)
        n.push_back("PC takvim gecikmesi " + us(static_cast<int64_t>(pv["tx_gec_azami_ns"])) + " µs > 10 ms");
    if (static_cast<int64_t>(pv["t_son_yaz_ns"]) > pencere_pay)
        n.push_back("PC son yazma t0+" + us(static_cast<int64_t>(pv["t_son_yaz_ns"])) + " µs, pencere + pay " + us(pencere_pay) + " µs'yi aşıyor");
    if (!echo) {
        if (static_cast<int64_t>(kv["tx_gec_azami_us"]) * 1000 > TAKVIM_GEC_AZAMI_NS)
            n.push_back("kart takvim gecikmesi " + std::to_string(kv["tx_gec_azami_us"]) + " µs > 10 ms");
        if (static_cast<int64_t>(kv["tx_son_us"]) * 1000 > pencere_pay)
            n.push_back("kart son gönderim t0+" + std::to_string(kv["tx_son_us"]) + " µs, pencere + pay " + us(pencere_pay) + " µs'yi aşıyor");
    }
    // süre
    const double plan_sn = static_cast<double>(pencere) / 1e9;
    const double yaz_sn = (static_cast<double>(pv["t_son_yaz_ns"]) + static_cast<double>(aralik)) / 1e9;
    const double kart_sn = static_cast<double>(kv["sure_us"]) / 1e6;
    if (bek.en_az_sn > 0) {
        if (plan_sn < bek.en_az_sn) n.push_back("süre: plan " + std::to_string(plan_sn) + " s < " + std::to_string(bek.en_az_sn));
        if (yaz_sn < bek.en_az_sn) n.push_back("süre: son yazma " + std::to_string(yaz_sn) + " s < " + std::to_string(bek.en_az_sn));
        if (kart_sn < bek.en_az_sn) n.push_back("süre: kart " + std::to_string(kart_sn) + " s < " + std::to_string(bek.en_az_sn));
    }
    if (echo) {
        if (zaman_asimi != 0) n.push_back("zaman aşımı/kayıp yanıt: " + std::to_string(zaman_asimi) + " (geç gelen " + std::to_string(gec_r) + ")");
        if (atlanan != 0) n.push_back("kuyruk sınırı nedeniyle gönderilmeyen istek: " + std::to_string(atlanan));
    }

    // ---- 8. rapor ----
    std::ostringstream r;
    char tb[600];
    std::snprintf(tb, sizeof tb,
                  "mod=%s profil=%s baud=%llu kart: USART %llu Hz, BRR=0x%llX OVER8=%llu -> %llu baud (%lld ppm, BRR'den); "
                  "OS istenen=%llu geri=%llu/%llu\n",
                  mod.c_str(), profil.c_str(), static_cast<unsigned long long>(baud),
                  static_cast<unsigned long long>(kart_usart_hz), static_cast<unsigned long long>(kart_brr),
                  static_cast<unsigned long long>(kart_over8), static_cast<unsigned long long>(brr_baud),
                  static_cast<long long>(kart_ppm), static_cast<unsigned long long>(bv["os_istenen"]),
                  static_cast<unsigned long long>(bv["os_geri_i"]), static_cast<unsigned long long>(bv["os_geri_o"]));
    r << tb;
    std::snprintf(tb, sizeof tb,
                  "PLAN: yük %llu B, çerçeve %llu B, aralık %llu ns (%.3f çerçeve/s), yön başına %.0f B/s = %.2f%% (8N1, "
                  "etkin %llu baud), pencere %.6f s\n",
                  static_cast<unsigned long long>(yuk), static_cast<unsigned long long>(boy),
                  static_cast<unsigned long long>(aralik), 1e9 / static_cast<double>(aralik), yon_bps, 100.0 * oran,
                  static_cast<unsigned long long>(etkin_baud), plan_sn);
    r << tb;
    const double pc_gs = static_cast<double>(pv["t_son_yaz_ns"]) / 1e9;
    const double kart_gs = static_cast<double>(kv["tx_son_us"]) / 1e6;
    std::snprintf(tb, sizeof tb,
                  "GERÇEKLEŞEN: PC->kart %llu çerçeve, %llu tel baytı, son yazma sonu t0+%.6f s (t0-son yazma ortalaması %.0f B/s; n çerçeve n-1 aralığa düşer); "
                  "kart->PC %llu çerçeve, %llu tel baytı, son gönderim halkaya t0+%.6f s (%.0f B/s); iki yön toplamı %llu B "
                  "(tek yön kapasitesiyle karşılaştırılmaz)\n",
                  static_cast<unsigned long long>(pv["tx_cerceve"]), static_cast<unsigned long long>(pv["tx_bayt"]), pc_gs,
                  pc_gs > 0 ? static_cast<double>(pv["tx_bayt"]) / pc_gs : 0.0,
                  static_cast<unsigned long long>(kv["tx_cerceve"]), static_cast<unsigned long long>(kv["tx_bayt"]), kart_gs,
                  kart_gs > 0 ? static_cast<double>(kv["tx_bayt"]) / kart_gs : 0.0,
                  static_cast<unsigned long long>(pv["tx_bayt"] + kv["tx_bayt"]));
    r << tb;
    std::snprintf(tb, sizeof tb,
                  "takvim: PC azami gecikme %s µs, yazma çağrısı azami %s µs; kart azami gecikme %llu µs, yer bekleme %llu; "
                  "sınır 10 ms ve son tamamlama pencere + %s µs; kart (ESLE->BITIR) %.3f s; kart alım halkası azami "
                  "doluluk %llu/%llu B\n",
                  us(static_cast<int64_t>(pv["tx_gec_azami_ns"])).c_str(), us(static_cast<int64_t>(pv["yazma_azami_ns"])).c_str(),
                  static_cast<unsigned long long>(kv["tx_gec_azami_us"]), static_cast<unsigned long long>(kv["tx_bekleme"]),
                  us(pay).c_str(), kart_sn, static_cast<unsigned long long>(kv["rx_halka_azami"]),
                  static_cast<unsigned long long>(kv["rx_halka_boy"]));
    r << tb;
    std::snprintf(tb, sizeof tb,
                  "muhasebe: PC okunan %llu = ESLE öncesi %llu + pencere %llu + OZET sonrası %llu; PC denetim %llu çerçeve; "
                  "kart pencere %llu B, denetim %llu çerçeve\n",
                  static_cast<unsigned long long>(pv["rx_okunan_bayt"]), static_cast<unsigned long long>(pv["esle_oncesi_bayt"]),
                  static_cast<unsigned long long>(pv["rx_bayt"]), static_cast<unsigned long long>(pv["ozet_sonrasi_bayt"]),
                  static_cast<unsigned long long>(pv["rx_kontrol"]), static_cast<unsigned long long>(kv["rx_bayt"]),
                  static_cast<unsigned long long>(kv["rx_kontrol"]));
    r << tb;
    if (echo) {
        std::sort(rtt_plan.begin(), rtt_plan.end());
        std::sort(rtt_yaz.begin(), rtt_yaz.end());
        std::sort(yazma.begin(), yazma.end());
        auto satir = [&](const char *ad, const std::vector<int64_t> &v) {
            if (v.empty()) { r << ad << ": örnek yok\n"; return; }
            r << ad << " (µs): n=" << v.size() << " min=" << us(v.front()) << " p50=" << us(yuzdelik(v, 1, 2))
              << " p99=" << us(yuzdelik(v, 99, 100)) << " p99.9=" << us(yuzdelik(v, 999, 1000))
              << " maks=" << us(v.back()) << "\n";
        };
        satir("RTT planlanan gönderimden", rtt_plan);
        satir("RTT yazma başlangıcından", rtt_yaz);
        satir("yazma çağrısı süresi", yazma);
        r << "istek=" << pc_adet << " gönderilen=" << gonderilen << " yanıtlı=" << yanitli << " zaman_asimi=" << zaman_asimi
          << " (bunun geç gelen " << gec_r << ") atlanan=" << atlanan << " (zaman aşımı " << zaman_asimi_ms
          << " ms, yazma sonundan; kuyruk sınırı " << azami_bekleyen << ", gözlenen azami bekleyen "
          << pv["azami_bekleyen_gozlenen"] << ")\n";
    }
    sonuc.rapor = r.str();
    sonuc.hukum = n.empty() ? Hukum::Gecti : Hukum::Gecmedi;
    return sonuc;
}

}  // namespace mp3
