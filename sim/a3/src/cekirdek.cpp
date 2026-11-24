// cekirdek.cpp — A3 sunucusunun soketten bağımsız çekirdeği (bkz. cekirdek.hpp).
//
// Kural numaraları docs/sozlesmeler/A3-web.md'dedir. EMNİYET işaretli kısımlar: erişim denetimi
// (A3-K02…K12) ve sanal emniyet girdileri ile yer istasyonu komutlarının taşınması (A3-K89…K104,
// K117…K119). Sunucu karar vermez: kısıtlayan/gevşeten ayrımını isteğin biçiminden çıkarır,
// gevşetenden revizyon ve onay jetonu ister, kısıtlayanı hiçbir zaman kuyruk doluluğu ya da
// revizyon yüzünden reddetmez; uygulama kararı komut alıcısınındır (sim).
#include "cekirdek.hpp"

#include "kodlayici.hpp"
#include "ozet.hpp"
#include "rastgele.hpp"
#include "statik.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <unordered_set>

namespace ida::a3 {

namespace {

struct KA {
    std::uint16_t kanal = 0;
    std::uint64_t anahtar = 0;
    bool operator<(const KA& o) const noexcept { return kanal != o.kanal ? kanal < o.kanal : anahtar < o.anahtar; }
    bool operator==(const KA& o) const noexcept { return kanal == o.kanal && anahtar == o.anahtar; }
};

struct KAHash {
    std::size_t operator()(const KA& k) const noexcept
    {
        return std::hash<std::uint64_t>{}((k.anahtar * 0x9E3779B97F4A7C15ull) ^ k.kanal);
    }
};

struct Kare {
    KA ka;
    std::int64_t t_ns = 0;
    std::uint32_t sira = 0;
    bool oncesinde_dusme = false;  // halkada (dünya → A3) önceki kayıt düştü
    std::vector<std::uint8_t> yuk;
};
using KareP = std::shared_ptr<const Kare>;

struct SonDeger {
    std::uint64_t uretim = 0;
    KareP kare;
};

struct OlayKaydi {
    std::uint32_t sira = 0;
    std::vector<std::uint8_t> cerceve;  // başlık + yük; JSON karşılığı istek anında üretilir (W4-K22 b)
};
using OlayP = std::shared_ptr<const OlayKaydi>;

enum class Neden : std::uint8_t { jeton = 0, host = 1, origin = 2, alt_protokol = 3 };
constexpr std::string_view kNedenAdi[] = {"jeton", "host", "origin", "alt_protokol"};

enum class Asama : std::uint8_t { baslik, govde_uzunluk, govde_parcali, isle };

struct Baglanti {
    BagId id = 0;
    bool ws = false;
    std::string girdi;
    std::string cikis;
    std::size_t cikis_bas = 0;
    bool kapanacak = false;
    bool okuma_kapali = false;
    Zaman olusma{};
    Zaman son_etkinlik{};
    // HTTP çerçeveleme (W4-K23 3): girdiden çerçevelenmekte olan istek
    Asama asama = Asama::baslik;
    bool istek_var = false;
    Zaman istek_bas{};
    http::Istek cer_istek;
    std::string cer_govde;
    bool cer_asildi = false;
    bool cer_dogrulandi = false;  // başlık tamam ve geçerli Bearer taşıyor
    std::uint64_t kalan = 0;
    std::size_t govde_siniri = sinir::govde;
    std::size_t okunan_govde = 0;
    std::unique_ptr<http::ParcaliCozucu> parcali;
    // Çerçevelenmiş, sırası bekleyen istekler (HTTP/1.1 sırası)
    struct Bekleyen {
        http::Istek istek;
        std::string govde;
        bool asildi = false;
        bool kisitlayan = false;  // biçimden kısıtlayan emniyet isteği
        bool dogrulandi = false;  // geçerli Bearer
        bool hatali = false;      // çerçeveleme hatası: 400 + kapanış
        std::string hata;
        bool islendi = false;  // sırası gelmeden işlendi; yanıtı `cikis`te
        std::string cikis;
        bool kapat = false;
    };
    std::deque<Bekleyen> sira;
    std::size_t sira_bayt = 0;
    bool cerceve_durdu = false;  // yükseltme/kapatma/hata/sınır aşımından sonra çerçeveleme durur
    bool sira_tasti = false;
    bool eof = false;  // karşı taraf gönderim yönünü kapattı
    Zaman eof_bas{};
    bool eksik_islendi = false;
    bool okuma_firsati = false;  // kabulden sonra en az bir okuma fırsatı buldu (W4-K23 2')
    bool hic_dogrulandi = false;  // geçerli Bearer taşıyan bir istek başlığı çerçevelendi (W4-K23 3'')
    // İşleme bağlamı (istek_isle ve işleyiciler)
    http::Istek istek;
    std::string govde;
    bool govde_asildi = false;
    // WebSocket
    std::uint32_t ws_no = 0;
    std::string ws_ileti;
    bool ws_parcada = false;
    bool ws_kapaniyor = false;
    Zaman son_alim{};
    Zaman son_ping{};
    std::map<std::uint16_t, std::uint8_t> abonelik;  // kanal indeksi → istenen hz (durum dışı: 0)
    std::unordered_map<KA, std::uint64_t, KAHash> gonderilen;
    std::map<std::uint16_t, std::deque<KareP>> dizi;
    std::unordered_set<KA, KAHash> dusme;
    std::deque<OlayP> olaylar;
    std::uint64_t ezilen = 0;
    std::uint64_t dusen_dizi = 0;
    std::size_t kuyruk_bayt = 0;
    bool yazildi = false;  // son döngüden beri çıkış ilerledi
    Zaman son_yazma{};
    bool kapanma_goruldu = false;
    Zaman kapanma_bas{};

    std::size_t bekleyen_cikis() const noexcept { return cikis.size() - cikis_bas; }
};

enum class KomutDurum : std::uint8_t { kuyrukta, uygulandi, reddedildi };

struct KomutKaydi {
    std::uint32_t istemci = 0;
    KomutDurum durum = KomutDurum::kuyrukta;
    bool adim_var = false;
    std::uint64_t adim = 0;
    std::string hata_kodu;
    std::string hata_ileti;
    std::string sonuc_ayrinti;
};

struct YinelemeKaydi {
    std::string yontem;
    std::string yol;
    std::array<std::uint8_t, 32> ozet{};
    int http = 0;
    std::string govde;
    std::optional<std::string> run_id;
    Zaman zaman{};
};

struct Jeton {
    std::uint32_t istemci = 0;
    std::string run_id;
    std::uint32_t emniyet_rev = 0;
    std::string islem;
    Zaman son{};
};

struct YiKomut {
    KomutSinifi sinif = KomutSinifi::notr;
    bool onay = false;
    const json::Deger* parametreler = nullptr;
};

struct Yanit {
    int http = 200;
    std::string govde;
    std::vector<std::pair<std::string, std::string>> ek;
    bool kapat = false;
};

struct RetSayaci {
    Zaman son{};
    bool hic = false;
    std::uint64_t biriken = 0;
    std::string yol;
};

bool canli(CalistirmaDurumu d) noexcept
{
    return d == CalistirmaDurumu::hazirlaniyor || d == CalistirmaDurumu::kosuyor || d == CalistirmaDurumu::duraklatildi;
}

// UTF-8 sınırına dikkat ederek kırpma.
std::string kirp_utf8(std::string_view s, std::size_t azami)
{
    if (s.size() <= azami) {
        return std::string(s);
    }
    std::size_t n = azami;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0u) == 0x80u) {
        --n;
    }
    return std::string(s.substr(0, n));
}

bool ondalik_u64(std::string_view s, std::uint64_t ust, std::uint64_t& v)
{
    if (s.empty() || s.size() > 20) {
        return false;
    }
    v = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        const auto r = static_cast<std::uint64_t>(c - '0');
        if (v > (ust - r) / 10u) {
            return false;
        }
        v = v * 10u + r;
    }
    return true;
}

bool sabit_sureli_esit(std::string_view a, std::string_view b) noexcept
{
    // Uzunluk farklı olsa da b uzunluğunca karşılaştırılır (A3-K06).
    unsigned fark = a.size() == b.size() ? 0u : 1u;
    for (std::size_t i = 0; i < b.size(); ++i) {
        const auto x = static_cast<unsigned char>(i < a.size() ? a[i] : 0);
        fark |= static_cast<unsigned>(x ^ static_cast<unsigned char>(b[i]));
    }
    return fark == 0;
}

std::string_view icerik_turu(const std::filesystem::path& p)
{
    const std::string e = p.extension().string();
    if (e == ".html") return "text/html; charset=utf-8";
    if (e == ".js" || e == ".mjs") return "text/javascript; charset=utf-8";
    if (e == ".css") return "text/css; charset=utf-8";
    if (e == ".json" || e == ".map") return "application/json";
    if (e == ".svg") return "image/svg+xml";
    if (e == ".png") return "image/png";
    if (e == ".ico") return "image/x-icon";
    if (e == ".txt") return "text/plain; charset=utf-8";
    if (e == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

}  // namespace

struct Cekirdek::Ic {
    Ic(const SunucuAyari& a, KomutAlicisi& al, Yayinci::Ic& y, std::string j, std::uint16_t p)
        : ayar(a), alici(al), yayin(y), jeton(std::move(j)), port(p)
    {
    }

    SunucuAyari ayar;
    KomutAlicisi& alici;
    Yayinci::Ic& yayin;
    std::string jeton;
    std::uint16_t port;

    std::set<std::string, std::less<>> izinli_host;
    std::set<std::string, std::less<>> izinli_origin;
    std::string katalog_ozeti;
    json::Deger katalog;
    json::Deger komut_katalogu;
    std::unique_ptr<Sema> sema;
    std::unique_ptr<Sema> komut_sema;
    std::map<std::string, YiKomut, std::less<>> yi_komutlar;
    std::uint16_t idx_calistirma = 0;
    std::uint16_t idx_baglanti = 0;
    const KanalTanimi* olay_kanali = nullptr;

    std::map<BagId, std::unique_ptr<Baglanti>> bag;
    BagId sonraki_bag = 1;
    std::uint32_t sonraki_ws = 1;

    std::map<KA, SonDeger> son;
    std::unordered_map<KA, std::uint64_t, KAHash> dizi_sayac;
    std::deque<OlayP> gecmis;
    std::size_t gecmis_bayt = 0;
    std::uint32_t olay_sira = 0;
    std::uint64_t gorulen_olay_dusen = 0;
    std::uint64_t gecersiz_olay = 0;
    std::int64_t son_sim_t_ns = 0;
    std::optional<std::string> son_run_id;
    std::uint8_t son_durum = 0;

    std::uint32_t komut_no = 0;
    std::map<std::uint32_t, KomutKaydi> komutlar;
    std::map<std::pair<std::uint32_t, std::string>, YinelemeKaydi> yinelemeler;
    std::set<std::uint32_t> istemciler;
    std::uint32_t sonraki_istemci = 1;
    std::map<std::string, Jeton, std::less<>> jetonlar;
    std::optional<Jeton> istek_jetonu;  // işlenen istekte tüketilen onay jetonu (onay_jetonu_tuket)
    StatikKok statik_kok;               // W4-K23 (1): başlangıçta bir kez açılır, ömür boyu tutulur
    // EMNİYET — W4-K22 (a): alıcı doluyken kısıtlayan etkiler sabit boylu, etki başına mandalda
    // bekler (acil durdurma; tüp başına fiş ve pim; daraltıcı yer istasyonu komutu türü başına
    // son istek). Anahtar kümesi hazırlıkta sabitlenir; mandal taşamaz. Aynı etkinin yeni isteği
    // eskisinin yerini alır; eskisinin komut numarası `birlesen`'e girer ve sonucu, mandaldaki
    // isteğin dünya sonucuyla birlikte bildirilir (sınır dolarsa en eski SONUÇ kaydı atılır).
    struct Mandal {
        bool dolu = false;
        Komut k;
        std::deque<std::uint32_t> birlesen;
    };
    std::map<std::string, Mandal, std::less<>> mandallar;
    std::map<std::uint32_t, std::deque<std::uint32_t>> birlesik_sonuc;  // iletilen no → birleşenler
    std::uint8_t tavan = 50;     // A3-K37

    std::array<RetSayaci, 4> retler{};
    bool tik_basladi = false;
    bool dis_tik = false;
    Zaman sonraki_tik{};
    Zaman sonraki_bakim{};
    bool kapaniyor = false;
    SunucuSayaclari say;

    // ---- yardımcılar ---------------------------------------------------------------------------

    void gunluk(std::string_view s) const
    {
        if (ayar.gunluk) {
            ayar.gunluk(s);
        }
    }

    std::uint32_t komut_no_al()
    {
        ++komut_no;
        if (komut_no == 0) {
            komut_no = 1;
        }
        say.komut_no = komut_no;
        return komut_no;
    }

    void komut_kaydet(std::uint32_t no, std::uint32_t istemci, KomutDurum d, std::string_view kod = {},
                      std::string_view ileti = {})
    {
        KomutKaydi& k = komutlar[no];
        k.istemci = istemci;
        k.durum = d;
        k.hata_kodu = kod;
        k.hata_ileti = ileti;
        while (komutlar.size() > sinir::komut_gecmisi) {
            komutlar.erase(komutlar.begin());
        }
    }

    static std::string ret_govde(std::string_view kod, std::string_view ileti, std::string_view ayrinti,
                                 std::uint32_t no)
    {
        json::Nesne h;
        h.metin("kod", kod).metin("ileti", kirp_utf8(ileti, 1000));
        if (!ayrinti.empty()) {
            h.ham("ayrinti", ayrinti);
        }
        json::Nesne n;
        n.metin("sonuc", "ret");
        if (no == 0) {
            n.bos("komut_no");
        } else {
            n.sayi_u("komut_no", no);
        }
        n.ham("hata", h.bitir());
        return n.bitir();
    }

    static Yanit ret(std::string_view kod, std::string_view ileti, std::string_view ayrinti = {}, std::uint32_t no = 0)
    {
        Yanit y;
        y.http = hata_http(kod);
        if (y.http == 0) {
            y.http = 500;
        }
        y.govde = ret_govde(kod, ileti, ayrinti, no);
        return y;
    }

    // EMNİYET (W4-K23 3'): HTTP bağlantısında bekleyen çıkış kesin tavanlıdır; ekleme tavanı
    // aşacaksa eklenmez ve bağlantı kapatılır.
    bool cikisa_ekle(Baglanti& b, std::string_view s)
    {
        if (!b.ws && b.bekleyen_cikis() + s.size() > sinir::http_cikis_tavani) {
            ++say.cikis_tavani_kapanisi;
            b.kapanacak = true;
            b.okuma_kapali = true;
            return false;
        }
        // EMNİYET (W4-K23 3''): kimliği doğrulanmamış bağlantıların bekleyen çıkış toplamı kesin
        // tavanlıdır; aşılacaksa en eski kimliği doğrulanmamış bağlantı (çıkışı atılarak) kapatılır.
        while (!b.ws && !b.hic_dogrulandi) {
            std::size_t toplam = 0;
            Baglanti* en_eski = nullptr;
            for (auto& [id, xp] : bag) {
                (void)id;
                Baglanti& x = *xp;
                if (x.ws || x.hic_dogrulandi) {
                    continue;
                }
                toplam += x.bekleyen_cikis();
                if (x.bekleyen_cikis() > 0 &&
                    (en_eski == nullptr || x.olusma < en_eski->olusma || (x.olusma == en_eski->olusma && x.id < en_eski->id))) {
                    en_eski = &x;
                }
            }
            if (toplam + s.size() <= sinir::dogrulanmamis_cikis_toplami) {
                break;
            }
            ++say.toplam_cikis_kapanisi;
            if (en_eski == nullptr || en_eski == &b) {
                zorla_kapat(b);
                return false;
            }
            zorla_kapat(*en_eski);
        }
        b.cikis += s;
        return true;
    }

    // Bağlantı çıkışı atılarak kapatılır (G/Ç döngüsü sonraki turda soketi kapatır).
    void zorla_kapat(Baglanti& x)
    {
        x.cikis.clear();
        x.cikis_bas = 0;
        x.kapanacak = true;
        x.okuma_kapali = true;
        sira_birak(x);
    }

    void yaz(Baglanti& b, const Yanit& y)
    {
        std::string_view tur = y.govde.empty() ? std::string_view{} : std::string_view("application/json");
        cikisa_ekle(b, http::yanit(y.http, tur, y.govde, y.kapat, y.ek, b.istek.yontem == "HEAD"));
        if (y.kapat) {
            b.kapanacak = true;
            b.okuma_kapali = true;
        }
    }

    void yaz_ham(Baglanti& b, int http, std::string_view tur, std::string_view govde, bool kapat = false,
                 const std::vector<std::pair<std::string, std::string>>& ek = {})
    {
        cikisa_ekle(b, http::yanit(http, tur, govde, kapat, ek, b.istek.yontem == "HEAD"));
        if (kapat) {
            b.kapanacak = true;
            b.okuma_kapali = true;
        }
    }

    Baglanti* bul(BagId id) const
    {
        const auto it = bag.find(id);
        return it == bag.end() ? nullptr : it->second.get();
    }

    // ---- hazırlık ------------------------------------------------------------------------------

    bool hazirla(std::string& hata)
    {
        const auto kb = katalog_baytlari();
        const auto ozet_ = ozet::sha256(kb.data(), kb.size());
        katalog_ozeti = "sha256:" + ozet::onaltilik(ozet_.data(), ozet_.size());
        json::CozumSiniri genis;
        genis.azami_deger = 2'000'000;
        std::string h;
        if (!json::coz(std::string_view(reinterpret_cast<const char*>(kb.data()), kb.size()), katalog, &h, genis)) {
            hata = "A3 kataloğu çözülemedi: " + h;
            return false;
        }
        const auto mb = komut_katalogu_baytlari();
        if (!json::coz(std::string_view(reinterpret_cast<const char*>(mb.data()), mb.size()), komut_katalogu, &h,
                       genis)) {
            hata = "A1 komut kataloğu çözülemedi: " + h;
            return false;
        }
        sema = std::make_unique<Sema>(katalog);
        komut_sema = std::make_unique<Sema>(komut_katalogu);
        const json::Deger* defs = katalog.bul("$defs");
        if (defs == nullptr || !defs->nesne_mi()) {
            hata = "A3 kataloğunda $defs yok";
            return false;
        }
        for (const auto& [ad, s] : defs->nesne) {
            if (!sema->desteklenmeyen_bul(s, h)) {
                hata = "A3 şeması desteklenmiyor ($defs/" + ad + "): " + h;
                return false;
            }
        }
        const json::Deger* liste = komut_katalogu.bul("komutlar");
        if (liste == nullptr || liste->tur != json::Tur::dizi) {
            hata = "A1 komut kataloğunda komutlar yok";
            return false;
        }
        for (const auto& k : liste->dizi) {
            const json::Deger* ad = k.bul("ad");
            const json::Deger* sinif = k.bul("sinif");
            const json::Deger* onay = k.bul("onay");
            const json::Deger* par = k.bul("parametreler");
            if (ad == nullptr || ad->tur != json::Tur::metin || sinif == nullptr || sinif->tur != json::Tur::metin ||
                onay == nullptr || onay->tur != json::Tur::mantiksal || par == nullptr) {
                hata = "A1 komut kataloğu öğesi eksik";
                return false;
            }
            YiKomut y;
            if (sinif->metin == "daraltici") {
                y.sinif = KomutSinifi::daraltici;
            } else if (sinif->metin == "genisletici") {
                y.sinif = KomutSinifi::genisletici;
            } else if (sinif->metin == "notr") {
                y.sinif = KomutSinifi::notr;
            } else {
                hata = "A1 komut kataloğunda bilinmeyen sınıf: " + sinif->metin;
                return false;
            }
            y.onay = onay->mantik;
            y.parametreler = par;
            if (!komut_sema->desteklenmeyen_bul(*par, h)) {
                hata = "A1 komut şeması desteklenmiyor (" + ad->metin + "): " + h;
                return false;
            }
            yi_komutlar[ad->metin] = y;
        }
        if (!ayar.statik_kok.empty() && !statik_kok.ac(ayar.statik_kok, h)) {
            hata = "statik kök açılamadı: " + h;
            return false;
        }
        for (const char* m : {"acil_durdurma", "silahlandirma_fisi|0", "silahlandirma_fisi|1", "emniyet_pimi|0", "emniyet_pimi|1"}) {
            mandallar[m] = Mandal{};
        }
        for (const auto& [ad, y] : yi_komutlar) {
            if (y.sinif == KomutSinifi::daraltici) {
                mandallar["yi|" + ad] = Mandal{};
            }
        }
        const KanalTanimi* c = kanal_bul("calistirma/durum");
        const KanalTanimi* bd = kanal_bul("baglanti/durum");
        olay_kanali = kanal_bul("olay");
        if (c == nullptr || bd == nullptr || olay_kanali == nullptr) {
            hata = "katalogda zorunlu kanal yok";
            return false;
        }
        idx_calistirma = c->indeks;
        idx_baglanti = bd->indeks;
        // `baglanti/durum`'un bağlantıya özgü üç alanı yükün sonundadır (u64, u64, u32): her
        // bağlantıya gönderimde son 20 bayt yamanır (A3-K40, K41).
        const YukTanimi* y = bd->yuk;
        if (y->alan_sayisi < 3 || y->alanlar[y->alan_sayisi - 3].ad != "bu_baglanti_ezilen_durum" ||
            y->alanlar[y->alan_sayisi - 3].tip != Tip::u64 || y->alanlar[y->alan_sayisi - 2].tip != Tip::u64 ||
            y->alanlar[y->alan_sayisi - 1].ad != "bu_baglanti_olay_kuyrugu" || y->alanlar[y->alan_sayisi - 1].tip != Tip::u32) {
            hata = "baglanti/durum yükünün sonu beklenen düzende değil";
            return false;
        }
        const std::string p = std::to_string(port);
        for (const char* ad : {"127.0.0.1:", "localhost:", "[::1]:"}) {
            izinli_host.insert(std::string(ad) + p);
            izinli_origin.insert(std::string("http://") + ad + p);
        }
        return true;
    }

    // ---- bağlantılar ---------------------------------------------------------------------------

    // Kimliği doğrulanmış (geçerli Bearer) ve isteği süren bağlantı: gövdesi gelmekte olan ya da
    // sırada işlenmemiş isteği var.
    static bool dogrulanmis_suren(const Baglanti& x)
    {
        if (x.asama != Asama::baslik && x.cer_dogrulandi) {
            return true;
        }
        for (const auto& e : x.sira) {
            if (!e.islendi && e.dogrulandi) {
                return true;
            }
        }
        return false;
    }

    // EMNİYET (W4-K23 2''): sınır doluyken kapatılabilecek bağlantı yoksa, okuma fırsatı bulmamış
    // kimliği doğrulanmamış bağlantı varsa kabul o turda bırakılır (bekle); sınırı yalnız kimliği
    // doğrulanmış bağlantılar dolduruyorsa yeni bağlantı reddedilir (503).
    KabulKarari kabul_karari() const
    {
        if (kapaniyor) {
            return KabulKarari::reddet;
        }
        std::size_t http_sayisi = 0;
        bool aday = false;
        bool korunan = false;
        for (const auto& [id, bp] : bag) {
            (void)id;
            const Baglanti& x = *bp;
            if (x.ws) {
                continue;
            }
            ++http_sayisi;
            if (!x.kapanacak && dogrulanmis_suren(x)) {
                continue;
            }
            (x.okuma_firsati ? aday : korunan) = true;
        }
        if (ayar.azami_http == 0 || http_sayisi < ayar.azami_http || aday) {
            return KabulKarari::kabul;
        }
        return korunan ? KabulKarari::bekle : KabulKarari::reddet;
    }

    BagId ac(Zaman simdi, std::vector<BagId>& kapatilacak)
    {
        if (kapaniyor) {
            return 0;
        }
        std::size_t http_sayisi = 0;
        for (const auto& [id, b] : bag) {
            (void)id;
            if (!b->ws) {
                ++http_sayisi;
            }
        }
        if (http_sayisi >= ayar.azami_http && ayar.azami_http > 0) {
            // EMNİYET (W4-K23 2'): yer açmak için kimliği doğrulanmamış (ya da kapanmakta olan)
            // bağlantılar arasından yalnız yaş sırasıyla en eskisi kapatılır; sınıf önceliği yoktur.
            // Kabulden sonra henüz bir okuma fırsatı bulmamış bağlantı (aynı kabul turunda kabul
            // edilenler dahil) kapatılmaz. Kimliği doğrulanmış ve isteği süren bağlantı kapatılmaz;
            // kapatılacak yoksa yeni bağlantı reddedilir (çağıran 503 yazmayı dener ve kapatır).
            Baglanti* secilen = nullptr;
            for (const auto& [id, bp] : bag) {
                (void)id;
                const Baglanti& x = *bp;
                if (x.ws || !x.okuma_firsati || (!x.kapanacak && dogrulanmis_suren(x))) {
                    continue;
                }
                if (secilen == nullptr || x.olusma < secilen->olusma || (x.olusma == secilen->olusma && x.id < secilen->id)) {
                    secilen = bp.get();
                }
            }
            if (secilen == nullptr) {
                ++say.baglanti_reddi;
                return 0;
            }
            ++say.yer_acmak_icin_kapatilan;
            baglanti_birak(*secilen);
            kapatilacak.push_back(secilen->id);
            bag.erase(secilen->id);
        }
        auto b = std::make_unique<Baglanti>();
        b->id = sonraki_bag++;
        b->olusma = simdi;
        b->son_etkinlik = simdi;
        const BagId id = b->id;
        bag.emplace(id, std::move(b));
        return id;
    }

    void gelen(Baglanti& b, const std::uint8_t* veri, std::size_t n, Zaman simdi)
    {
        if (b.okuma_kapali || b.kapanacak) {
            return;
        }
        b.son_etkinlik = simdi;
        b.okuma_firsati = true;
        b.girdi.append(reinterpret_cast<const char*>(veri), n);
        if (b.ws) {
            ws_isle(b, simdi);
        } else {
            http_isle(b, simdi);
        }
    }

    // ---- HTTP ----------------------------------------------------------------------------------

    // EMNİYET (W4-K23 3): girdinin okunması ve isteklerin çerçevelenmesi (Content-Length ve chunked)
    // çıkış baskısından bağımsız sürer; çerçevelenen tam istekler sıraya girer. Sıra HTTP/1.1
    // sırasıyla işlenir; çıkış baskısı (> okuma eşiği) altında yalnız kısıtlayan emniyet istekleri
    // hemen işlenir (yanıtı saklanır, sırası gelince yazılır), ötekiler bekler. Sıra sınırı aşılırsa
    // bağlantı kapanır; o ana dek çerçevelenmiş kısıtlayanlar işlenmiş olur. Kapanışta (EOF dahil)
    // sırada kalan kısıtlayan istekler atılmadan işlenir.
    void http_isle(Baglanti& b, Zaman simdi)
    {
        // Her tam istekten sonra sıra işlenir: sıra yalnız işleme bekletildiğinde (çıkış baskısı)
        // büyür; ardışık (pipelined) istekler sıra sınırına takılmaz.
        while (cercevele(b, simdi, true)) {
            sirayi_isle(b, simdi);
            if (b.kapanacak || b.ws) {
                break;
            }
        }
        sirayi_isle(b, simdi);
    }

    static bool gevsetme_ucu(std::string_view yol)
    {
        return yol == "/api/v1/emniyet/degistir" || yol == "/api/v1/yer_istasyonu/komut";
    }

    static bool kapatma_istenir(const http::Istek& r)
    {
        bool kapat = r.kucuk_surum == 0;
        if (const std::string* c = r.baslik("connection")) {
            for (const auto& t : http::liste(*c)) {
                kapat = kapat || http::token_esit(t, "close");
            }
        }
        return kapat;
    }

    void cerceve_hatasi(Baglanti& b, std::string neden)
    {
        // W4-K23 (4'): kimliği doğrulanmış gevşetme isteği çerçeveleme hatasıyla biterse jetonu
        // okunamamıştır: istemcinin bekleyen bütün jetonları geçersizleşir.
        if (b.cer_dogrulandi && gevsetme_ucu(b.cer_istek.yol)) {
            istemci_jetonlarini_sil(b.cer_istek);
        }
        b.cer_dogrulandi = false;
        b.cer_istek = http::Istek{};
        Baglanti::Bekleyen e;
        e.hatali = true;
        e.hata = std::move(neden);
        b.sira.push_back(std::move(e));
        b.cerceve_durdu = true;
        b.okuma_kapali = true;
        b.istek_var = false;
        b.asama = Asama::baslik;
        b.girdi.clear();
    }

    // `tek`: bir istek sıraya girince durur ve true döndürür.
    bool cercevele(Baglanti& b, Zaman simdi, bool tek = false)
    {
        bool eklendi = false;
        while (!b.cerceve_durdu && !b.kapanacak && !b.ws) {
            if (tek && eklendi) {
                return true;
            }
            if (b.asama == Asama::baslik) {
                if (b.girdi.empty()) {
                    return eklendi;
                }
                if (!b.istek_var) {
                    b.istek_var = true;
                    b.istek_bas = simdi;
                    b.cer_dogrulandi = false;
                }
                http::Istek r;
                std::size_t blok = 0;
                std::string neden;
                const auto s = http::baslik_ayristir(b.girdi, r, blok, neden);
                if (s == http::Sonuc::eksik) {
                    return eklendi;
                }
                if (s == http::Sonuc::hata) {
                    cerceve_hatasi(b, "HTTP isteği bozuk: " + neden);
                    return true;
                }
                b.girdi.erase(0, blok);
                b.cer_istek = std::move(r);
                b.cer_dogrulandi = jeton_dogru(b.cer_istek);
                b.hic_dogrulandi = b.hic_dogrulandi || b.cer_dogrulandi;
                http::GovdeBilgisi g;
                if (!http::govde_bilgisi(b.cer_istek, g, neden)) {
                    cerceve_hatasi(b, "HTTP gövde çerçevelemesi: " + neden);
                    return true;
                }
                b.cer_govde.clear();
                b.cer_asildi = false;
                b.okunan_govde = 0;
                b.parcali.reset();
                b.govde_siniri = b.cer_istek.yol.rfind("/api/v1/senaryo", 0) == 0 ? sinir::senaryo_govde : sinir::govde;
                if (g.tur == http::GovdeBilgisi::Tur::yok) {
                    istek_tamamla(b);
                    eklendi = true;
                    continue;
                }
                if (g.tur == http::GovdeBilgisi::Tur::uzunluk) {
                    if (g.uzunluk > b.govde_siniri) {
                        // A3-K51: gövdeden en çok ilk 4096 bayt okunur; sınırın ötesi okunmaz ve
                        // yanıttan sonra bağlantı kapanır. Başlıktan sonra soketten gerçekten okunmuş
                        // bayt (okuma bütçesiyle ≤ 4096).
                        say.azami_asim_govde_bayti = std::max<std::uint64_t>(say.azami_asim_govde_bayti, b.girdi.size());
                        const std::size_t al = std::min<std::size_t>(
                            {b.girdi.size(), sinir::govde_onizleme, static_cast<std::size_t>(g.uzunluk)});
                        b.cer_govde.assign(b.girdi, 0, al);
                        b.okunan_govde = al;
                        b.girdi.clear();
                        b.cer_asildi = true;
                        b.okuma_kapali = true;
                        istek_tamamla(b);
                        eklendi = true;
                        continue;
                    }
                    b.kalan = g.uzunluk;
                    b.asama = Asama::govde_uzunluk;
                } else {
                    b.parcali = std::make_unique<http::ParcaliCozucu>(b.govde_siniri);
                    b.asama = Asama::govde_parcali;
                }
            }
            if (b.asama == Asama::govde_uzunluk) {
                const std::size_t al = std::min<std::size_t>(static_cast<std::size_t>(b.kalan), b.girdi.size());
                b.cer_govde.append(b.girdi, 0, al);
                b.girdi.erase(0, al);
                b.kalan -= al;
                b.okunan_govde += al;
                if (b.kalan > 0) {
                    return eklendi;
                }
                istek_tamamla(b);
                eklendi = true;
                continue;
            }
            if (b.asama == Asama::govde_parcali) {
                const std::size_t t = b.parcali->besle(b.girdi, b.cer_govde);
                b.girdi.erase(0, t);
                b.okunan_govde += t;
                if (b.parcali->asildi()) {
                    b.cer_asildi = true;
                    b.okuma_kapali = true;
                    b.girdi.clear();
                } else if (b.parcali->hata()) {
                    cerceve_hatasi(b, "chunked gövde bozuk");
                    return true;
                } else if (!b.parcali->bitti()) {
                    return eklendi;
                }
                istek_tamamla(b);
                eklendi = true;
                continue;
            }
            return eklendi;
        }
        return eklendi;
    }

    // Çerçevelenen isteği sıraya koyar. Yükseltme, kapatma ya da sınırı aşan istekten sonra
    // çerçeveleme durur (ardından gelen baytlar o istek işlenmeden yorumlanmaz).
    void istek_tamamla(Baglanti& b)
    {
        Baglanti::Bekleyen e;
        e.istek = std::move(b.cer_istek);
        e.govde = std::move(b.cer_govde);
        e.asildi = b.cer_asildi;
        e.dogrulandi = b.cer_dogrulandi;
        if (!e.asildi && e.istek.yontem == "POST" && gevsetme_ucu(e.istek.yol)) {
            json::Deger g;
            e.kisitlayan = json::coz(e.govde, g) && g.nesne_mi() && kisitlayan_bicim(e.istek.yol, g);
        }
        const bool durdur =
            e.asildi || e.istek.baslik("upgrade") != nullptr || e.istek.yol == "/ws" || kapatma_istenir(e.istek);
        b.sira_bayt += e.govde.size() + e.istek.hedef.size() + 64;
        b.sira.push_back(std::move(e));
        b.cer_istek = http::Istek{};
        b.cer_govde.clear();
        b.cer_asildi = false;
        b.cer_dogrulandi = false;
        b.parcali.reset();
        b.asama = Asama::baslik;
        b.istek_var = false;
        if (durdur) {
            b.cerceve_durdu = true;
        }
        if (b.sira.size() > sinir::http_sira || b.sira_bayt > sinir::http_sira_bayt) {
            b.sira_tasti = true;
            b.cerceve_durdu = true;
            b.okuma_kapali = true;
            ++say.http_sira_tasmasi;
        }
    }

    void sira_cikar(Baglanti& b)
    {
        const Baglanti::Bekleyen& e = b.sira.front();
        const std::size_t n = e.govde.size() + e.istek.hedef.size() + 64 + e.cikis.size();
        b.sira_bayt = b.sira_bayt > n ? b.sira_bayt - n : 0;
        b.sira.pop_front();
    }

    // Sıradaki isteği sırası gelmiş olarak işler.
    void sirali_isle(Baglanti& b, Baglanti::Bekleyen& e, Zaman simdi)
    {
        b.son_etkinlik = simdi;
        if (e.hatali) {
            b.istek = http::Istek{};
            b.govde.clear();
            b.govde_asildi = false;
            Yanit y = ret("gecersiz_istek", e.hata);
            y.kapat = true;
            yaz(b, y);
            return;
        }
        b.istek = std::move(e.istek);
        b.govde = std::move(e.govde);
        b.govde_asildi = e.asildi;
        istek_isle(b, simdi);
        if (!b.ws && (e.asildi || kapatma_istenir(b.istek))) {
            b.kapanacak = true;
            b.okuma_kapali = true;
        }
    }

    // EMNİYET: kısıtlayan isteği sırası gelmeden işler; yanıtı ayrı bir tampona yazılır ve
    // sırası gelince çıkışa eklenir (HTTP/1.1 yanıt sırası korunur). Bağlantının işleme bağlamı korunur.
    void one_isle(Baglanti& b, Baglanti::Bekleyen& e, Zaman simdi)
    {
        std::string asil_cikis;
        std::swap(asil_cikis, b.cikis);
        const std::size_t asil_bas = b.cikis_bas;
        b.cikis_bas = 0;
        http::Istek asil_istek = std::move(b.istek);
        std::string asil_govde = std::move(b.govde);
        const bool asil_asildi = b.govde_asildi;
        const bool asil_kapanacak = b.kapanacak;
        const bool asil_okuma = b.okuma_kapali;
        b.istek = e.istek;
        b.govde = e.govde;
        b.govde_asildi = false;
        b.kapanacak = false;
        istek_isle(b, simdi);
        e.cikis = std::move(b.cikis);
        e.kapat = kapatma_istenir(e.istek) || b.kapanacak;
        e.islendi = true;
        b.sira_bayt += e.cikis.size();  // saklanan yanıt sıra tavanına dahil (W4-K23 3')
        b.cikis = std::move(asil_cikis);
        b.cikis_bas = asil_bas;
        b.istek = std::move(asil_istek);
        b.govde = std::move(asil_govde);
        b.govde_asildi = asil_asildi;
        b.kapanacak = asil_kapanacak;
        b.okuma_kapali = asil_okuma;
        ++say.baskida_one_alinan;
    }

    void sirayi_isle(Baglanti& b, Zaman simdi)
    {
        while (!b.sira.empty() && !b.kapanacak && !b.ws) {
            Baglanti::Bekleyen& on = b.sira.front();
            if (on.islendi) {
                // Öne alınmış yanıt da çıkış eşiğine ve tavanına uyar (W4-K23 3').
                if (b.bekleyen_cikis() > sinir::okuma_esigi) {
                    ++say.http_baski_bekleme;
                    break;
                }
                if (!cikisa_ekle(b, on.cikis)) {
                    break;
                }
                const bool kapat = on.kapat;
                sira_cikar(b);
                b.son_etkinlik = simdi;
                if (kapat) {
                    b.kapanacak = true;
                    b.okuma_kapali = true;
                }
                continue;
            }
            if (b.bekleyen_cikis() > sinir::okuma_esigi) {
                ++say.http_baski_bekleme;
                break;
            }
            Baglanti::Bekleyen e = std::move(b.sira.front());
            sira_cikar(b);
            sirali_isle(b, e, simdi);
            if (b.ws) {
                if (!b.girdi.empty()) {
                    ws_isle(b, simdi);
                }
                return;
            }
            if (b.cerceve_durdu && b.sira.empty() && !b.kapanacak && !b.okuma_kapali && !b.sira_tasti) {
                // Başarısız yükseltme denemesi: bağlantı HTTP olarak sürer, çerçeveleme devam eder.
                b.cerceve_durdu = false;
                cercevele(b, simdi);
            }
        }
        if (b.ws) {
            return;
        }
        // EMNİYET: sırası gelmeden bekleyen (çıkış baskısı, kapanış ya da sıra taşması) kısıtlayan
        // istekler hemen işlenir; kapanışta yanıtları atılır, etkileri atılmaz. Yalnız kimliği
        // doğrulanmış (geçerli Bearer) istek öne alınır; doğrulanmamış olan olağan sırada reddedilir
        // (W4-K23 3').
        for (auto& e : b.sira) {
            if (!e.islendi && e.kisitlayan && e.dogrulandi) {
                one_isle(b, e, simdi);
            }
        }
        if (b.sira_bayt > sinir::http_sira_bayt && !b.sira_tasti) {
            b.sira_tasti = true;
            b.cerceve_durdu = true;
            b.okuma_kapali = true;
            ++say.http_sira_tasmasi;
        }
        if (b.sira_tasti && !b.kapanacak) {
            b.kapanacak = true;
            b.okuma_kapali = true;
        }
        if (b.kapanacak) {
            sira_birak(b);
        }
        if (b.eof && !b.kapanacak && b.sira.empty()) {
            b.kapanacak = true;
        }
    }

    // Kapanan bağlantının sırada işlenmemiş istekleri atılır; kimliği doğrulanmış gevşetme
    // isteklerinde jeton kuralı (W4-K23 4) uygulanır.
    void sira_birak(Baglanti& b)
    {
        for (auto& e : b.sira) {
            if (!e.islendi && !e.hatali && e.dogrulandi) {
                (void)jeton_sunumu(e.istek, e.govde, e.asildi);
            }
        }
        b.sira.clear();
        b.sira_bayt = 0;
    }

    // Bağlantı kapanırken gövdesi eksik kalan istek (W4-K23 4): kimliği doğrulanmış gevşetme
    // isteğinin jetonu okunamadığından istemcinin bekleyen bütün jetonları geçersizleşir.
    void eksik_istegi_birak(Baglanti& b)
    {
        if (b.eksik_islendi || b.ws) {
            return;
        }
        b.eksik_islendi = true;
        if (b.asama != Asama::baslik && b.cer_dogrulandi && gevsetme_ucu(b.cer_istek.yol)) {
            istemci_jetonlarini_sil(b.cer_istek);
        }
    }

    // Bağlantı silinirken (soket kapandı ya da yer açmak için kapatıldı).
    void baglanti_birak(Baglanti& b)
    {
        eksik_istegi_birak(b);
        sira_birak(b);
    }

    bool jeton_dogru(const http::Istek& r) const
    {
        if (r.baslik_sayisi("authorization") != 1) {
            return false;
        }
        const std::string& a = *r.baslik("authorization");
        if (a.size() < 7 || !http::token_esit(std::string_view(a).substr(0, 6), "bearer") || a[6] != ' ') {
            return false;
        }
        return sabit_sureli_esit(std::string_view(a).substr(7), jeton);
    }

    // A3-K12: reddedilen her istek sayılır; aynı neden için saniyede en çok bir olay.
    void erisim_reddi(Baglanti& b, Neden n, const std::string& yol, Zaman simdi)
    {
        ++say.erisim_reddi;
        RetSayaci& r = retler[static_cast<std::size_t>(n)];
        ++r.biriken;
        // Yol yalnız yol kısmıdır (sorgu yazılmaz); jeton yol içinde geçse bile gizlenir.
        std::string temiz = yol;
        for (std::size_t k = temiz.find(jeton); !jeton.empty() && k != std::string::npos; k = temiz.find(jeton)) {
            temiz.replace(k, jeton.size(), "[gizli]");
        }
        r.yol = kirp_utf8(temiz, 256);
        if (!r.hic || simdi - r.son >= sure::erisim_olayi) {
            erisim_olayi_yayinla(n, simdi);
        }
        yaz(b, ret("erisim_reddedildi", "erişim reddedildi"));
    }

    void erisim_olayi_yayinla(Neden n, Zaman simdi)
    {
        RetSayaci& r = retler[static_cast<std::size_t>(n)];
        json::Nesne a;
        if (!r.yol.empty()) {
            a.metin("yol", r.yol);
        }
        a.metin("neden", kNedenAdi[static_cast<std::size_t>(n)]).sayi_u("adet", r.biriken);
        olay_yayinla(son_sim_t_ns, 19, 1, 1, std::numeric_limits<std::uint64_t>::max(), 0, 0, a.bitir());
        r.biriken = 0;
        r.son = simdi;
        r.hic = true;
    }

    // EMNİYET — A3-K94, W4-K23 (4): Bearer doğrulamasından hemen sonra, yöntem, içerik türü,
    // Ida-Istemci, gövde boyu, şema, kimlik çakışması, run/revizyon ve alıcı retleri dahil her erken
    // retten önce, emniyet/degistir ve yer_istasyonu/komut gövdesindeki onay jetonu tüketilir:
    //   - gövde tam ve JSON nesnesi olarak çözülürse: biçimden kısıtlayan istekte jeton yok sayılır
    //     ve tüketilmez (A3-K92, K104); değilse `onay_jetonu` metni bekleyen bir jetonsa tüketilir ve
    //     işleyiciye geçirilir;
    //   - jeton okunamazsa (gövde sınırı aşıldı, JSON bozuk ya da nesne değil; eksik gövde
    //     `eksik_istegi_birak`ta): o istemcinin bekleyen bütün onay jetonları geçersizleşir. Gövde
    //     metninde jeton aranmaz.
    std::optional<Jeton> jeton_sunumu(const http::Istek& r, const std::string& govde, bool asildi)
    {
        std::uint32_t istemci = 0;
        if (!gevsetme_ucu(r.yol) || !istemci_basligi(r, istemci)) {
            return std::nullopt;  // istemci kimliği eksik/geçersiz: istek reddedilir, jetonlara dokunulmaz
        }
        json::Deger g;
        if (!asildi && json::coz(govde, g) && g.nesne_mi()) {
            if (kisitlayan_bicim(r.yol, g)) {
                return std::nullopt;
            }
            const json::Deger* jet = g.bul("onay_jetonu");
            if (jet != nullptr && jet->tur == json::Tur::metin) {
                // Jeton onu isteyen istemciye bağlıdır; yalnız aynı kimlikle sunulduğunda tüketilir.
                const auto it = jetonlar.find(jet->metin);
                if (it != jetonlar.end() && it->second.istemci == istemci) {
                    Jeton j = it->second;
                    jetonlar.erase(it);
                    return j;
                }
            }
            return std::nullopt;
        }
        istemci_jetonlarini_sil(r);
        return std::nullopt;
    }

    // `Ida-Istemci` ile tanımlanan istemcinin bekleyen bütün onay jetonları silinir. İstemci
    // kimliği eksik ya da geçersizse hiçbir jetona dokunulmaz (W4-K23 4': kimliksiz istek hiçbir
    // jetonu kullanamaz; bir istemcinin hatalı isteği ötekilerin onayını bozmaz).
    void istemci_jetonlarini_sil(const http::Istek& r)
    {
        std::uint32_t istemci = 0;
        if (!istemci_basligi(r, istemci)) {
            return;
        }
        for (auto it = jetonlar.begin(); it != jetonlar.end();) {
            it = it->second.istemci == istemci ? jetonlar.erase(it) : std::next(it);
        }
        ++say.jeton_toplu_gecersiz;
    }

    void onay_jetonu_tuket(Baglanti& b) { istek_jetonu = jeton_sunumu(b.istek, b.govde, b.govde_asildi); }

    // EMNİYET — W4-K23 (4''): geçerli Bearer ve geçerli istemci kimliği taşıyan gevşetme isteği
    // jetonu okunmadan bir erişim/erken retle (kapanış, Host, Origin) dönerse o istemcinin bekleyen
    // bütün jetonları geçersizleşir; ret yanıtı değişmez. Biçimden kısıtlayan istekte dokunulmaz.
    void erken_ret_jetonlari(const Baglanti& b)
    {
        const http::Istek& r = b.istek;
        if (!gevsetme_ucu(r.yol) || !jeton_dogru(r)) {
            return;
        }
        json::Deger g;
        if (!b.govde_asildi && json::coz(b.govde, g) && g.nesne_mi() && kisitlayan_bicim(r.yol, g)) {
            return;
        }
        istemci_jetonlarini_sil(r);
    }

    void istek_isle(Baglanti& b, Zaman simdi)
    {
        istek_jetonu.reset();
        const http::Istek& r = b.istek;
        if (kapaniyor) {
            erken_ret_jetonlari(b);
            Yanit y = ret("mesgul", "sunucu kapanıyor");
            y.kapat = true;
            yaz(b, y);
            return;
        }
        // EMNİYET — A3-K08: Host; A3-K09: Origin (varsa).
        if (r.baslik_sayisi("host") != 1 || izinli_host.count(*r.baslik("host")) == 0) {
            erken_ret_jetonlari(b);
            erisim_reddi(b, Neden::host, r.yol, simdi);
            return;
        }
        const std::size_t origin_sayisi = r.baslik_sayisi("origin");
        if (origin_sayisi > 1 || (origin_sayisi == 1 && izinli_origin.count(*r.baslik("origin")) == 0)) {
            erken_ret_jetonlari(b);
            erisim_reddi(b, Neden::origin, r.yol, simdi);
            return;
        }
        const std::string& yol = r.yol;
        if (yol == "/api" || yol.rfind("/api/", 0) == 0) {
            // EMNİYET — A3-K06: Authorization: Bearer <jeton>, sabit süreli karşılaştırma.
            if (!jeton_dogru(r)) {
                erisim_reddi(b, Neden::jeton, r.yol, simdi);
                return;
            }
            onay_jetonu_tuket(b);
            if (r.yontem == "OPTIONS") {
                yaz(b, ret("yontem_desteklenmiyor", "OPTIONS desteklenmez (A3-K10)"));
                return;
            }
            api_isle(b, simdi);
            return;
        }
        if (r.yontem == "OPTIONS") {
            yaz(b, ret("yontem_desteklenmiyor", "OPTIONS desteklenmez (A3-K10)"));
            return;
        }
        if (yol == "/ws") {
            if (r.yontem != "GET") {
                Yanit y = ret("yontem_desteklenmiyor", "/ws yalnız GET");
                y.ek.emplace_back("Allow", "GET");
                yaz(b, y);
                return;
            }
            ws_yukselt(b, simdi);
            return;
        }
        if (yol == "/" || yol.rfind("/panel/", 0) == 0) {
            if (r.yontem != "GET") {
                Yanit y = ret("yontem_desteklenmiyor", "statik yollar yalnız GET (A3-K11)");
                y.ek.emplace_back("Allow", "GET");
                yaz(b, y);
                return;
            }
            statik(b);
            return;
        }
        yaz(b, ret("bulunamadi", "yol yok"));
    }

    void statik(Baglanti& b)
    {
        const std::string& yol = b.istek.yol;
        if (b.govde_asildi) {
            Yanit y = ret("govde_cok_buyuk", "gövde sınırdan büyük");
            y.kapat = true;
            yaz(b, y);
            return;
        }
        const std::string goreli = yol == "/" ? "index.html" : yol.substr(1);
        bool gecerli = !ayar.statik_kok.empty() && !goreli.empty() && goreli.back() != '/' && goreli.size() < 512;
        char onceki = '/';
        for (const char c : goreli) {
            const bool izinli = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                c == '.' || c == '_' || c == '-' || c == '/';
            if (!izinli || (c == '/' && onceki == '/') || (c == '.' && onceki == '/')) {
                gecerli = false;
                break;
            }
            onceki = c;
        }
        if (!gecerli) {
            yaz(b, ret("bulunamadi", "dosya yok"));
            return;
        }
        // EMNİYET — A3-K11: açılan dosya kök altında kalır; yolda simgesel bağ/reparse noktası olan
        // hiçbir dosya sunulmaz (statik.hpp).
        std::string icerik;
        if (!statik_kok.oku(goreli, sinir::statik_azami, icerik)) {
            yaz(b, ret("bulunamadi", "dosya yok"));
            return;
        }
        yaz_ham(b, 200, icerik_turu(std::filesystem::path(goreli)), icerik);
    }

    static bool yol_eslesir(std::string_view kalip, std::string_view yol, std::vector<std::string>& parametreler)
    {
        const std::size_t soru = kalip.find('?');
        if (soru != std::string_view::npos) {
            kalip = kalip.substr(0, soru);
        }
        parametreler.clear();
        std::size_t i = 0;
        std::size_t j = 0;
        while (i <= kalip.size() && j <= yol.size()) {
            const std::size_t ki = kalip.find('/', i);
            const std::size_t yj = yol.find('/', j);
            const std::string_view kp = kalip.substr(i, ki == std::string_view::npos ? std::string_view::npos : ki - i);
            const std::string_view yp = yol.substr(j, yj == std::string_view::npos ? std::string_view::npos : yj - j);
            if (kp.size() >= 2 && kp.front() == '{' && kp.back() == '}') {
                if (yp.empty() || yp.size() > 128) {
                    return false;
                }
                for (const char c : yp) {
                    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                          c == '-' || c == '.')) {
                        return false;
                    }
                }
                parametreler.emplace_back(yp);
            } else if (kp != yp) {
                return false;
            }
            if ((ki == std::string_view::npos) != (yj == std::string_view::npos)) {
                return false;
            }
            if (ki == std::string_view::npos) {
                return true;
            }
            i = ki + 1;
            j = yj + 1;
        }
        return false;
    }

    bool istemci_basligi(const http::Istek& r, std::uint32_t& istemci) const
    {
        if (r.baslik_sayisi("ida-istemci") != 1) {
            return false;
        }
        std::uint64_t v = 0;
        if (!ondalik_u64(*r.baslik("ida-istemci"), 0xFFFFFFFFu, v) || v == 0) {
            return false;
        }
        istemci = static_cast<std::uint32_t>(v);
        return istemciler.count(istemci) != 0;
    }

    static bool json_icerik(const http::Istek& r)
    {
        const std::string* c = r.baslik("content-type");
        if (c == nullptr || r.baslik_sayisi("content-type") != 1) {
            return false;
        }
        std::string_view t = *c;
        const std::size_t nokta = t.find(';');
        t = t.substr(0, nokta);
        while (!t.empty() && (t.back() == ' ' || t.back() == '\t')) {
            t.remove_suffix(1);
        }
        return http::token_esit(t, "application/json");
    }

    // Gövdeyi JSON olarak çözer ve `$defs/<sema>`'ya göre doğrular; başarısızsa ret yanıtı.
    std::optional<Yanit> govde_coz(const Baglanti& b, std::string_view sema_adi, json::Deger& g, std::uint32_t no)
    {
        if (!json_icerik(b.istek)) {
            return ret("icerik_turu", "Content-Type application/json değil", {}, no);
        }
        std::string h;
        json::CozumSiniri s;
        if (!json::coz(b.govde, g, &h, s)) {
            return ret("gecersiz_istek", "JSON çözülemedi: " + h, {}, no);
        }
        const json::Deger* tanim = sema->tanim(sema_adi);
        if (tanim == nullptr || !sema->dogrula(*tanim, g, &h)) {
            return ret("gecersiz_istek", "şemaya uymuyor: " + h, {}, no);
        }
        return std::nullopt;
    }

    void api_isle(Baglanti& b, Zaman simdi)
    {
        const http::Istek& r = b.istek;
        if (r.yol.rfind("/api/v1/", 0) != 0) {
            yaz(b, ret("bulunamadi", "uç yok"));
            return;
        }
        std::vector<std::string> p;
        const UcTanimi* uc = nullptr;
        std::string izinli;
        for (const auto& u : uclar()) {
            std::vector<std::string> pp;
            if (!yol_eslesir(u.yol, r.yol, pp)) {
                continue;
            }
            if (u.yontem == r.yontem) {
                uc = &u;
                p = std::move(pp);
                break;
            }
            if (izinli.find(u.yontem) == std::string::npos) {
                izinli += izinli.empty() ? "" : ", ";
                izinli += u.yontem;
            }
        }
        if (uc == nullptr) {
            if (izinli.empty()) {
                yaz(b, ret("bulunamadi", "uç yok (A3-K99)"));
            } else {
                Yanit y = ret("yontem_desteklenmiyor", "uç bu yöntemi desteklemiyor");
                y.ek.emplace_back("Allow", izinli);
                yaz(b, y);
            }
            return;
        }
        if (uc->durum_degistirir) {
            komut_isle(b, *uc, p, simdi);
        } else {
            okuma_isle(b, *uc, p, simdi);
        }
    }

    // ---- okuma ve onay jetonu uçları -----------------------------------------------------------

    void okuma_isle(Baglanti& b, const UcTanimi& uc, const std::vector<std::string>& p, Zaman simdi)
    {
        if (b.govde_asildi) {
            Yanit y = ret("govde_cok_buyuk", "istek gövdesi sınırdan büyük (A3-K51)");
            y.kapat = true;
            yaz(b, y);
            return;
        }
        const std::string_view yol = uc.yol;
        if (yol == "/api/v1/surum") {
            json::Nesne n;
            n.metin("sonuc", "kabul")
                .metin("a3_surumu", kA3Surumu)
                .metin("katalog_ozeti", katalog_ozeti)
                .metin("ws_alt_protokolu", kWsAltProtokolu)
                .metin("a6_surumu", "1.0")
                .metin("sim_surumu", kirp_utf8(ayar.sim_surumu, 128))
#if defined(_WIN32)
                .metin("platform", "windows-x64");
#else
                .metin("platform", "macos-arm64");
#endif
            yaz_ham(b, 200, "application/json", n.bitir());
        } else if (yol == "/api/v1/katalog") {
            const auto kb = katalog_baytlari();
            yaz_ham(b, 200, "application/json", std::string_view(reinterpret_cast<const char*>(kb.data()), kb.size()));
        } else if (yol == "/api/v1/yer_istasyonu/komutlar") {
            const auto mb = komut_katalogu_baytlari();
            yaz_ham(b, 200, "application/json", std::string_view(reinterpret_cast<const char*>(mb.data()), mb.size()));
        } else if (yol == "/api/v1/istemciler") {
            json::Deger g;
            if (auto hata = govde_coz(b, uc.istek_semasi, g, 0)) {
                yaz(b, *hata);
                return;
            }
            if (istemciler.size() >= sinir::istemci) {
                yaz(b, ret("mesgul", "istemci oturumu sınırı dolu"));
                return;
            }
            std::uint32_t id = sonraki_istemci++;
            while (id == 0 || istemciler.count(id) != 0) {
                id = sonraki_istemci++;
            }
            istemciler.insert(id);
            json::Nesne n;
            n.metin("sonuc", "kabul").sayi_u("istemci_id", id);
            yaz_ham(b, 200, "application/json", n.bitir());
        } else if (yol == "/api/v1/istemciler/{istemci_id}") {
            std::uint64_t id = 0;
            if (!ondalik_u64(p[0], 0xFFFFFFFFu, id) || istemciler.erase(static_cast<std::uint32_t>(id)) == 0) {
                yaz(b, ret("bulunamadi", "istemci oturumu yok"));
                return;
            }
            istemci_unut(static_cast<std::uint32_t>(id));
            yaz_ham(b, 200, "application/json", R"({"sonuc":"kabul"})");
        } else if (yol == "/api/v1/durum") {
            yaz_ham(b, 200, "application/json", durum_json());
        } else if (yol == "/api/v1/emniyet") {
            yaz_ham(b, 200, "application/json", emniyet_json());
        } else if (yol == "/api/v1/komutlar/{komut_no}") {
            komut_sorgu(b, p[0]);
        } else if (yol == "/api/v1/olaylar?sonra={sira}&en_cok={n}") {
            olaylar_sorgu(b);
        } else if (yol == "/api/v1/emniyet/onay_jetonu") {
            yaz(b, onay_jetonu_emniyet(b, simdi));
        } else if (yol == "/api/v1/yer_istasyonu/onay_jetonu") {
            yaz(b, onay_jetonu_yi(b, simdi));
        } else {
            // Bu dilimin kapsamı dışında (W4-a3 "Kapsam dışı"): senaryo, ön denetim, arıza,
            // kayıt, izleme, süreçler. Sözleşmede "uygulanmadı" yanıtı yok; kısıtlayan seçim 404.
            yaz(b, ret("bulunamadi", "bu uç bu sürümde uygulanmadı (W4-a3 dilim 1)"));
        }
    }

    void istemci_unut(std::uint32_t id)
    {
        for (auto it = jetonlar.begin(); it != jetonlar.end();) {
            it = it->second.istemci == id ? jetonlar.erase(it) : std::next(it);
        }
        for (auto it = yinelemeler.begin(); it != yinelemeler.end();) {
            it = it->first.first == id ? yinelemeler.erase(it) : std::next(it);
        }
    }

    std::string durum_json()
    {
        const AnlikDurum a = alici.anlik();
        json::Nesne n;
        n.metin("sonuc", "kabul")
            .metin("durum", deger_adi("yuk_calistirma_durum", "durum", static_cast<std::uint32_t>(a.durum)))
            .metin("kip", deger_adi("yuk_calistirma_durum", "kip", static_cast<std::uint32_t>(a.kip)));
        if (a.run_id) {
            n.metin("run_id", *a.run_id);
        } else {
            n.bos("run_id");
        }
        n.sayi("sim_t_ns", a.sim_t_ns < 0 ? 0 : a.sim_t_ns).sayi_u("adim_no", a.adim_no).mantik("gecerli", a.gecerli)
            .mantik("gercek_zamani_kacirdi", a.gercek_zamani_kacirdi);
        std::string nedenler = "[";
        for (const auto& [bit, ad] : gecerlilik_nedenleri()) {
            if (bit < 32 && ((a.gecerlilik_nedenleri >> bit) & 1u) != 0) {
                if (nedenler.size() > 1) {
                    nedenler.push_back(',');
                }
                json::metin_yaz(ad, nedenler);
            }
        }
        nedenler.push_back(']');
        n.ham("gecerlilik_nedenleri", nedenler);
        json::Nesne s;
        for (const auto& [ad, v] : a.sayaclar) {
            s.sayi(ad, v);
        }
        n.ham("sayaclar", s.bitir());
        json::Nesne k;
        k.metin("gorunum", deger_adi("yuk_calistirma_durum", "kademe_gorunum", a.kademe_gorunum))
            .metin("kamera", deger_adi("yuk_calistirma_durum", "kademe_kamera", a.kademe_kamera))
            .metin("sonar", deger_adi("yuk_calistirma_durum", "kademe_sonar", a.kademe_sonar))
            .metin("kayit", deger_adi("yuk_calistirma_durum", "kademe_kayit", a.kademe_kayit));
        n.ham("kademeler", k.bitir()).sayi("yayin_tavani_hz", tavan);
        if (a.senaryo_ozeti) {
            n.metin("senaryo_ozeti", *a.senaryo_ozeti);
        } else {
            n.bos("senaryo_ozeti");
        }
        n.sayi_u("emniyet_rev", a.emniyet_rev);
        if (a.a1_run_id != 0) {
            n.sayi_u("a1_run_id", a.a1_run_id);
        } else {
            n.bos("a1_run_id");
        }
        n.sayi_u("durum_rev", a.durum_rev).sayi_u("son_komut_no", komut_no);
        return n.bitir();
    }

    std::string emniyet_json()
    {
        const AnlikDurum a = alici.anlik();
        auto ad = [](std::string_view alan, std::uint32_t kod) {
            return deger_adi("yuk_emniyet_girdiler", alan, kod);
        };
        json::Nesne fis;
        fis.metin("f1", ad("silahlandirma_fisi", a.silahlandirma_fisi[0]))
            .metin("f2", ad("silahlandirma_fisi", a.silahlandirma_fisi[1]));
        json::Nesne pim;
        pim.metin("f1", ad("emniyet_pimi", a.emniyet_pimi[0])).metin("f2", ad("emniyet_pimi", a.emniyet_pimi[1]));
        json::Nesne y;
        y.metin("acil_durdurma", ad("acil_durdurma", a.acil_durdurma))
            .ham("silahlandirma_fisi", fis.bitir())
            .ham("emniyet_pimi", pim.bitir());
        json::Nesne n;
        n.metin("sonuc", "kabul").ham("yetkili", y.bitir()).mantik("emniyet_rev_doydu", a.emniyet_rev_doydu);
        if (a.son_degisim_adimi) {
            n.sayi_u("son_degisim_adimi", *a.son_degisim_adimi);
        } else {
            n.bos("son_degisim_adimi");
        }
        n.sayi_u("emniyet_rev", a.emniyet_rev);
        return n.bitir();
    }

    void komut_sorgu(Baglanti& b, const std::string& p)
    {
        std::uint64_t no = 0;
        const auto it = ondalik_u64(p, 0xFFFFFFFFu, no) ? komutlar.find(static_cast<std::uint32_t>(no)) : komutlar.end();
        if (it == komutlar.end()) {
            yaz(b, ret("bulunamadi", "komut numarası yok ya da geçmişte değil (A3-K60)"));
            return;
        }
        const KomutKaydi& k = it->second;
        json::Nesne n;
        n.metin("sonuc", "kabul").sayi_u("komut_no", it->first);
        n.metin("durum", k.durum == KomutDurum::kuyrukta    ? "kuyrukta"
                         : k.durum == KomutDurum::uygulandi ? "uygulandi"
                                                            : "reddedildi");
        if (k.adim_var) {
            n.sayi_u("uygulama_adimi", k.adim);
        }
        if (!k.hata_kodu.empty()) {
            json::Nesne h;
            h.metin("kod", k.hata_kodu);
            if (!k.hata_ileti.empty()) {
                h.metin("ileti", kirp_utf8(k.hata_ileti, 1000));
            }
            n.ham("hata", h.bitir());
        }
        if (!k.sonuc_ayrinti.empty()) {
            n.ham("sonuc_ayrinti", k.sonuc_ayrinti);
        }
        yaz_ham(b, 200, "application/json", n.bitir());
    }

    void olaylar_sorgu(Baglanti& b)
    {
        std::optional<std::uint64_t> sonra;
        std::uint64_t en_cok = 1000;
        std::string_view q = b.istek.sorgu;
        bool bozuk = false;
        while (!q.empty()) {
            const std::size_t amp = q.find('&');
            const std::string_view parca = q.substr(0, amp);
            q = amp == std::string_view::npos ? std::string_view{} : q.substr(amp + 1);
            const std::size_t esit = parca.find('=');
            if (esit == std::string_view::npos) {
                bozuk = true;
                continue;
            }
            const std::string_view k = parca.substr(0, esit);
            const std::string_view v = parca.substr(esit + 1);
            std::uint64_t x = 0;
            if (k == "sonra") {
                if (!ondalik_u64(v, 0xFFFFFFFFu, x)) {
                    bozuk = true;
                } else {
                    sonra = x;
                }
            } else if (k == "en_cok") {
                if (!ondalik_u64(v, 0xFFFFFFFFu, x)) {
                    bozuk = true;
                } else {
                    en_cok = x;
                }
            }
        }
        if (bozuk || !sonra) {
            yaz(b, ret("gecersiz_istek", "sonra (≥ 0) zorunlu; en_cok 1…1000"));
            return;
        }
        if (en_cok < 1 || en_cok > 1000) {
            yaz(b, ret("gecersiz_deger", "en_cok 1…1000 (A3-K45)"));
            return;
        }
        std::string liste = "[";
        bool tamam = true;
        if (!gecmis.empty()) {
            const std::uint64_t en_eski = gecmis.front()->sira;
            if (*sonra + 1 < en_eski && *sonra < olay_sira) {
                yaz(b, ret("gecmis_yok",
                           "istenen olaylar bellekteki geçmişte yok (pencere: son 65 536 olay ya da 64 MiB, hangisi önce "
                           "dolarsa; W4-K22); kayıttan okunur (A3-K45)"));
                return;
            }
            std::size_t i = *sonra + 1 > en_eski ? static_cast<std::size_t>(*sonra + 1 - en_eski) : 0;
            std::uint64_t n = 0;
            for (; i < gecmis.size() && n < en_cok; ++i, ++n) {
                if (liste.size() > 1) {
                    liste.push_back(',');
                }
                liste += olay_json(*gecmis[i]);
            }
            tamam = i >= gecmis.size();
        }
        liste.push_back(']');
        json::Nesne n;
        n.metin("sonuc", "kabul").ham("olaylar", liste).mantik("tamam", tamam);
        yaz_ham(b, 200, "application/json", n.bitir());
    }

    // ---- onay jetonu (A3-K93, K94, K103) -------------------------------------------------------

    // Beklenen çalıştırma ve canlılık; jeton yalnız canlı çalıştırmanın run_id'sine bağlanır.
    std::optional<Yanit> jeton_on_kosulu(const json::Deger& g, const AnlikDurum& a)
    {
        const json::Deger* beklenen = g.bul("beklenen_run_id");
        if (!a.run_id || beklenen == nullptr || beklenen->tur == json::Tur::bos) {
            return ret("canli_calistirma_yok", "onay jetonu canlı çalıştırma gerektirir (A3-K54, K91)");
        }
        if (beklenen->metin != *a.run_id) {
            json::Nesne ay;
            ay.metin("guncel_run_id", *a.run_id)
                .metin("guncel_durum", deger_adi("yuk_calistirma_durum", "durum", static_cast<std::uint32_t>(a.durum)))
                .sayi_u("guncel_durum_rev", a.durum_rev);
            return ret("beklenen_durum_uyusmuyor", "beklenen_run_id güncel değil (A3-K54)", ay.bitir());
        }
        if (!canli(a.durum)) {
            return ret("canli_calistirma_yok", "onay jetonu canlı çalıştırma gerektirir (A3-K91)");
        }
        if (a.emniyet_rev_doydu) {
            return ret("emniyet_rev_doydu", "emniyet revizyonu doydu; gevşetme kabul edilmez (A3-K117)");
        }
        return std::nullopt;
    }

    Yanit jeton_ver(std::uint32_t istemci, const AnlikDurum& a, std::string islem, Zaman simdi)
    {
        for (auto it = jetonlar.begin(); it != jetonlar.end();) {
            it = simdi > it->second.son ? jetonlar.erase(it) : std::next(it);
        }
        if (jetonlar.size() >= sinir::onay_jetonu) {
            return ret("mesgul", "onay jetonu sınırı dolu");
        }
        const std::string t = rastgele_onaltilik(16);
        if (t.size() != 32) {
            return ret("sunucu_hatasi", "rastgele kaynak kullanılamadı");
        }
        Jeton j;
        j.istemci = istemci;
        j.run_id = *a.run_id;
        j.emniyet_rev = a.emniyet_rev;
        j.islem = std::move(islem);
        j.son = simdi + sure::onay_jetonu;
        jetonlar[t] = std::move(j);
        json::Nesne n;
        n.metin("sonuc", "kabul")
            .metin("onay_jetonu", t)
            .sayi("gecerlilik_ms", std::chrono::duration_cast<std::chrono::milliseconds>(sure::onay_jetonu).count())
            .metin("run_id", *a.run_id)
            .sayi_u("emniyet_rev", a.emniyet_rev);
        Yanit y;
        y.govde = n.bitir();
        return y;
    }

    // EMNİYET: jetonun bütün bağları tutmalı (A3-K94); biri tutmazsa geçersiz.
    static bool jeton_gecerli(const Jeton& j, std::uint32_t istemci, std::string_view islem, const AnlikDurum& a,
                              Zaman simdi)
    {
        return simdi <= j.son && j.istemci == istemci && j.islem == islem && a.run_id && *a.run_id == j.run_id &&
               a.emniyet_rev == j.emniyet_rev;
    }

    static bool emniyet_islemi(const json::Deger& g, EmniyetGirdisi& gi, GirdiDurumu& hd, std::int8_t& tup,
                               bool& kisitlayan, std::string& islem)
    {
        const std::string& girdi = g.bul("girdi")->metin;
        const std::string& hedef = g.bul("hedef_durum")->metin;
        const json::Deger* t = g.bul("tup");
        tup = t == nullptr ? std::int8_t{-1} : (t->metin == "f1" ? std::int8_t{0} : std::int8_t{1});
        if (girdi == "acil_durdurma") {
            gi = EmniyetGirdisi::acil_durdurma;
            if (hedef != "basili" && hedef != "serbest") {
                return false;
            }
            hd = hedef == "basili" ? GirdiDurumu::basili : GirdiDurumu::serbest;
            kisitlayan = hd == GirdiDurumu::basili;  // A3-K89: basılması emniyeti artırır
        } else {
            if (hedef != "takili" && hedef != "cikarilmis") {
                return false;
            }
            hd = hedef == "takili" ? GirdiDurumu::takili : GirdiDurumu::cikarilmis;
            if (girdi == "silahlandirma_fisi") {
                gi = EmniyetGirdisi::silahlandirma_fisi;
                kisitlayan = hd == GirdiDurumu::cikarilmis;  // fişin çıkarılması
            } else {
                gi = EmniyetGirdisi::emniyet_pimi;
                kisitlayan = hd == GirdiDurumu::takili;  // pimin takılması
            }
        }
        islem = "emniyet|" + girdi + "|" + (t == nullptr ? std::string("-") : t->metin) + "|" + hedef;
        return true;
    }

    Yanit onay_jetonu_emniyet(const Baglanti& b, Zaman simdi)
    {
        std::uint32_t istemci = 0;
        if (!istemci_basligi(b.istek, istemci)) {
            return ret("gecersiz_istek", "Ida-Istemci başlığı yok ya da kayıtlı değil (A3-K93)");
        }
        json::Deger g;
        if (auto h = govde_coz(b, "istek_onay_jetonu_emniyet", g, 0)) {
            return *h;
        }
        EmniyetGirdisi gi{};
        GirdiDurumu hd{};
        std::int8_t tup = -1;
        bool kisitlayan = false;
        std::string islem;
        if (!emniyet_islemi(g, gi, hd, tup, kisitlayan, islem)) {
            return ret("gecersiz_deger", "hedef_durum bu girdi için geçersiz (A3-K89)");
        }
        if (kisitlayan) {
            // Kısıtlayan değişiklik tek adımlıdır (A3-K92); jeton yalnız gevşetme içindir.
            return ret("gecersiz_deger", "kısıtlayan değişiklik onay jetonu istemez (A3-K92)");
        }
        const AnlikDurum a = alici.anlik();
        if (auto h = jeton_on_kosulu(g, a)) {
            return *h;
        }
        return jeton_ver(istemci, a, islem, simdi);
    }

    std::string yi_islemi(const std::string& komut, const json::Deger& parametreler) const
    {
        std::string kanonik;
        json::kanonik_yaz(parametreler, kanonik);
        const auto o = ozet::sha256(kanonik);
        return "yi|" + komut + "|" + ozet::onaltilik(o.data(), o.size());
    }

    Yanit onay_jetonu_yi(const Baglanti& b, Zaman simdi)
    {
        std::uint32_t istemci = 0;
        if (!istemci_basligi(b.istek, istemci)) {
            return ret("gecersiz_istek", "Ida-Istemci başlığı yok ya da kayıtlı değil (A3-K103)");
        }
        json::Deger g;
        if (auto h = govde_coz(b, "istek_onay_jetonu_yer_istasyonu", g, 0)) {
            return *h;
        }
        const std::string& komut = g.bul("komut")->metin;
        const auto it = yi_komutlar.find(komut);
        if (it == yi_komutlar.end()) {
            return ret("komut_listede_yok", "komut A1 kataloğunda yok (A3-K100)");
        }
        std::string h;
        if (!komut_sema->dogrula(*it->second.parametreler, *g.bul("parametreler"), &h)) {
            return ret("gecersiz_deger", "parametreler komut şemasına uymuyor: " + h);
        }
        if (!it->second.onay) {
            return ret("gecersiz_deger", "bu komut onay jetonu istemez (A3-K103, K104)");
        }
        const AnlikDurum a = alici.anlik();
        if (auto r = jeton_on_kosulu(g, a)) {
            return *r;
        }
        return jeton_ver(istemci, a, yi_islemi(komut, *g.bul("parametreler")), simdi);
    }

    // ---- komutlar (durum değiştiren uçlar; A3-K50…K60) -----------------------------------------

    static bool uygulanan_komut(std::string_view yol)
    {
        return yol.rfind("/api/v1/calistirma/", 0) == 0 ? yol != "/api/v1/calistirma/yeniden_kostur"
                                                         : (yol == "/api/v1/emniyet/degistir" ||
                                                            yol == "/api/v1/yer_istasyonu/komut" || yol == "/api/v1/yayin");
    }

    // Doğrulanmamış gövdede biçimden kısıtlayan mı (jeton tüketimi için; alan yoksa ya da tipi
    // yanlışsa kısıtlayan sayılmaz: jeton tüketilir, kısıtlayan okuma).
    bool kisitlayan_bicim(std::string_view yol, const json::Deger& g) const
    {
        if (yol == "/api/v1/emniyet/degistir") {
            const json::Deger* gi = g.bul("girdi");
            const json::Deger* hd = g.bul("hedef_durum");
            if (gi == nullptr || hd == nullptr || gi->tur != json::Tur::metin || hd->tur != json::Tur::metin) {
                return false;
            }
            return (gi->metin == "acil_durdurma" && hd->metin == "basili") ||
                   (gi->metin == "silahlandirma_fisi" && hd->metin == "cikarilmis") ||
                   (gi->metin == "emniyet_pimi" && hd->metin == "takili");
        }
        const json::Deger* km = g.bul("komut");
        if (km == nullptr || km->tur != json::Tur::metin) {
            return false;
        }
        const auto it = yi_komutlar.find(km->metin);
        return it != yi_komutlar.end() && it->second.sinif == KomutSinifi::daraltici;
    }

    // Kısıtlayan mı (yineleme sınırı istisnası için; asıl ayrım işleyicidedir).
    bool kisitlayan_mi(std::string_view yol, const json::Deger& g) const
    {
        if (yol == "/api/v1/emniyet/degistir") {
            EmniyetGirdisi gi{};
            GirdiDurumu hd{};
            std::int8_t tup = -1;
            bool k = false;
            std::string islem;
            return emniyet_islemi(g, gi, hd, tup, k, islem) && k;
        }
        if (yol == "/api/v1/yer_istasyonu/komut") {
            const auto it = yi_komutlar.find(g.bul("komut")->metin);
            return it != yi_komutlar.end() && it->second.sinif == KomutSinifi::daraltici;
        }
        return false;
    }

    void komut_isle(Baglanti& b, const UcTanimi& uc, const std::vector<std::string>& p, Zaman simdi)
    {
        (void)p;
        const http::Istek& r = b.istek;
        std::uint32_t istemci = 0;
        auto erken_ret = [&](std::string_view kod, std::string_view ileti, bool kapat = false) {
            const std::uint32_t no = komut_no_al();
            komut_kaydet(no, istemci, KomutDurum::reddedildi, kod, ileti);
            Yanit y = ret(kod, ileti, {}, no);
            y.kapat = kapat;
            yaz(b, y);
        };
        if (b.govde_asildi) {
            erken_ret("govde_cok_buyuk", "istek gövdesi sınırdan büyük (A3-K51)", true);
            return;
        }
        if (!istemci_basligi(r, istemci)) {
            istemci = 0;
            erken_ret("gecersiz_istek", "Ida-Istemci başlığı yok ya da istemci kayıtlı değil (A3-K50)");
            return;
        }
        if (!uygulanan_komut(uc.yol)) {
            erken_ret("bulunamadi", "bu uç bu sürümde uygulanmadı (W4-a3 dilim 1)");
            return;
        }
        if (!json_icerik(r)) {
            erken_ret("icerik_turu", "Content-Type application/json değil (A3-K51)");
            return;
        }
        json::Deger g;
        std::string h;
        if (!json::coz(b.govde, g, &h)) {
            erken_ret("gecersiz_istek", "JSON çözülemedi: " + h);
            return;
        }
        // EMNİYET — A3-K94: sunulan onay jetonu istek_isle'de, Bearer doğrulamasının hemen
        // ardından ve her erken retten önce tüketildi (onay_jetonu_tuket).
        std::optional<Jeton> sunulan = std::move(istek_jetonu);
        istek_jetonu.reset();
        // EMNİYET — W4-K19: kısıtlayan emniyet isteği ve daraltıcı yer istasyonu komutu
        // `beklenen_run_id` hiç verilmese de kabul edilir. Alan yoksa doğrulama için null konur;
        // istek kısıtlayan değilse şemanın zorunlu alan kuralı aynen uygulanır.
        const bool emniyet_ya_da_yi = uc.yol == "/api/v1/emniyet/degistir" || uc.yol == "/api/v1/yer_istasyonu/komut";
        bool run_id_eklendi = false;
        if (emniyet_ya_da_yi && g.nesne_mi() && g.bul("beklenen_run_id") == nullptr) {
            g.nesne.emplace_back("beklenen_run_id", json::Deger{});
            run_id_eklendi = true;
        }
        const json::Deger* tanim = sema->tanim(uc.istek_semasi);
        if (tanim == nullptr || !sema->dogrula(*tanim, g, &h)) {
            erken_ret("gecersiz_istek", "şemaya uymuyor: " + h);
            return;
        }
        if (run_id_eklendi && !kisitlayan_mi(uc.yol, g)) {
            erken_ret("gecersiz_istek", "şemaya uymuyor: /: zorunlu alan yok: beklenen_run_id");
            return;
        }
        // A3-K53: yineleme anahtarı (istemci, yöntem, yol, istek_kimligi) ve gövde özeti.
        const auto anahtar = std::make_pair(istemci, g.bul("istek_kimligi")->metin);
        const auto oz = ozet::sha256(b.govde);
        const auto yit = yinelemeler.find(anahtar);
        if (yit != yinelemeler.end()) {
            const YinelemeKaydi& k = yit->second;
            if (k.yontem == r.yontem && k.yol == r.yol && k.ozet == oz) {
                std::string govde = k.govde;
                if (govde.size() >= 2 && govde.front() == '{') {
                    govde.insert(1, "\"yineleme\":true,");
                }
                yaz_ham(b, k.http, "application/json", govde);
                return;
            }
            erken_ret("kimlik_cakismasi", "aynı istek_kimligi başka yöntem, yol ya da gövdeyle (A3-K53)");
            return;
        }
        const bool kisitlayan = kisitlayan_mi(uc.yol, g);
        if (yinelemeler.size() >= sinir::yineleme_kaydi && !kisitlayan) {
            erken_ret("mesgul", "yineleme kaydı sınırı dolu (A3-K53)");
            return;
        }
        const std::uint32_t no = komut_no_al();
        Yanit y;
        if (uc.yol == "/api/v1/emniyet/degistir") {
            y = emniyet_degistir(g, istemci, no, simdi, sunulan);
        } else if (uc.yol == "/api/v1/yer_istasyonu/komut") {
            y = yer_istasyonu_komut(g, istemci, no, simdi, sunulan);
        } else if (uc.yol == "/api/v1/yayin") {
            tavan = static_cast<std::uint8_t>(g.bul("yayin_tavani_hz")->sayi);
            komut_kaydet(no, istemci, KomutDurum::uygulandi);
            json::Nesne n;
            n.metin("sonuc", "kabul").sayi_u("komut_no", no);
            y.govde = n.bitir();
        } else {
            y = calistirma(uc.yol, g, istemci, no);
        }
        // Ret ise komut kaydı (kuyrukta olanlar alici_gonder'de kaydedildi).
        if (y.http >= 400) {
            const json::Deger* kod = nullptr;
            json::Deger d;
            if (json::coz(y.govde, d)) {
                const json::Deger* hh = d.bul("hata");
                kod = hh != nullptr ? hh->bul("kod") : nullptr;
                const json::Deger* il = hh != nullptr ? hh->bul("ileti") : nullptr;
                komut_kaydet(no, istemci, KomutDurum::reddedildi, kod != nullptr ? kod->metin : std::string("sunucu_hatasi"),
                             il != nullptr ? il->metin : std::string());
            }
        }
        if (yinelemeler.size() < sinir::yineleme_kaydi) {
            YinelemeKaydi k;
            k.yontem = r.yontem;
            k.yol = r.yol;
            k.ozet = oz;
            k.http = y.http;
            k.govde = y.govde;
            k.run_id = son_run_id;
            k.zaman = simdi;
            yinelemeler.emplace(anahtar, std::move(k));
        }
        yaz(b, y);
    }

    static std::optional<std::string> run_id_al(const json::Deger& g)
    {
        const json::Deger* r = g.bul("beklenen_run_id");
        if (r == nullptr || r->tur != json::Tur::metin) {
            return std::nullopt;
        }
        return r->metin;
    }

    // Alıcıya iletim. EMNİYET: kısıtlayan komut kuyruk doluluğu yüzünden reddedilmez: alıcı
    // doluysa sırayla bekletilir ve her döngüde yeniden denenir (A3-K59, K92, K104); bekleyen
    // kısıtlayan varken kısıtlaymayan komut alıcıya gitmez (gevşetenin arkasında beklememesi için).
    Yanit alici_gonder(Komut& k, std::uint32_t no)
    {
        k.komut_no = no;
        if (k.kisitlayan) {
            // EMNİYET — W4-K22 (a): hiçbir sınır yüzünden reddedilmez.
            if (dolu_mandal() > 0) {
                mandala_koy(k);
                return kuyrukta(no);
            }
            const AlimSonucu a = alici.al(k);
            if (a.tur == AlimSonucu::Tur::dolu) {
                mandala_koy(k);
                return kuyrukta(no);
            }
            return alim_yaniti(a, k, no);
        }
        if (dolu_mandal() > 0) {
            return ret("mesgul", "komut kuyruğu dolu (A3-K59)", {}, no);
        }
        const AlimSonucu a = alici.al(k);
        if (a.tur == AlimSonucu::Tur::dolu) {
            return ret("mesgul", "komut kuyruğu dolu (A3-K59)", {}, no);
        }
        return alim_yaniti(a, k, no);
    }

    Yanit kuyrukta(std::uint32_t no) const
    {
        json::Nesne n;
        n.metin("sonuc", "kuyrukta").sayi_u("komut_no", no);
        Yanit y;
        y.http = 202;
        y.govde = n.bitir();
        return y;
    }

    // EMNİYET — W4-K19: kısıtlayan emniyet isteği ve daraltıcı yer istasyonu komutu
    // `beklenen_run_id` güncel çalıştırmayla uyuşmasa da (ya da hiç verilmese de) alıcıya iletilir.
    // Uyuşmazlık `uyari` olayıyla kaydedilir (yalnız komut numarası ve iki run_id; jeton ve gövde
    // yazılmaz). Kabul yanıtı dondurulmuş 1.0 şemasındaki `yanit_kuyrukta`dır; şemada olmayan
    // `guncel_run_id` alanı taşınmaz (güncel run_id olayda ve `GET /durum`da).
    Yanit kisitlayan_gonder(Komut& k, std::uint32_t no, std::uint32_t istemci)
    {
        const AnlikDurum a = alici.anlik();
        const bool uyusmaz = a.run_id != k.beklenen_run_id;
        k.run_id_uyusmazligi = uyusmaz;
        if (uyusmaz) {
            const std::string ileti = "kısıtlayan komut " + std::to_string(no) + ": beklenen_run_id " +
                                      (k.beklenen_run_id ? *k.beklenen_run_id : std::string("null")) + ", güncel run_id " +
                                      (a.run_id ? *a.run_id : std::string("null")) + "; W4-K19 gereği iletildi";
            json::Nesne ay;
            ay.metin("kod", "kisitlayan_run_id_uyusmazligi").metin("ileti", ileti);
            olay_yayinla(son_sim_t_ns, 22, 1, 1, std::numeric_limits<std::uint64_t>::max(), no, istemci, ay.bitir());
        }
        return alici_gonder(k, no);
    }

    Yanit alim_yaniti(const AlimSonucu& a, const Komut& k, std::uint32_t no)
    {
        if (a.tur == AlimSonucu::Tur::kuyrukta) {
            komut_kaydet(no, k.istemci_id, KomutDurum::kuyrukta);
            return kuyrukta(no);
        }
        std::string ayrinti;
        if (!a.ayrinti_json.empty()) {
            json::Deger d;
            if (json::coz(a.ayrinti_json, d) && d.nesne_mi()) {
                ayrinti = a.ayrinti_json;
            }
        }
        if (hata_http(a.hata_kodu) == 0) {
            gunluk("A3: komut alıcısı katalogda olmayan hata kodu döndürdü");
            return ret("sunucu_hatasi", "komut alıcısı bilinmeyen kod döndürdü", {}, no);
        }
        return ret(a.hata_kodu, a.ileti.empty() ? std::string_view("komut reddedildi") : std::string_view(a.ileti),
                   ayrinti, no);
    }

    Yanit calistirma(std::string_view yol, const json::Deger& g, std::uint32_t istemci, std::uint32_t no)
    {
        Komut k;
        k.istemci_id = istemci;
        k.beklenen_run_id = run_id_al(g);
        k.beklenen_durum_rev = static_cast<std::uint64_t>(g.bul("beklenen_durum_rev")->sayi);
        const std::string_view ad = yol.substr(std::string_view("/api/v1/calistirma/").size());
        if (ad == "baslat") {
            k.tur = KomutTuru::calistirma_baslat;
            if (const auto* v = g.bul("kayit_kademesi")) {
                k.kayit_kademesi = v->metin;
            }
            if (const auto* v = g.bul("kamera_kareleri")) {
                k.kamera_kareleri = v->metin;
            }
        } else if (ad == "duraklat") {
            k.tur = KomutTuru::calistirma_duraklat;
            const auto* v = g.bul("gecersizligi_onayla");
            k.gecersizligi_onayla = v != nullptr && v->mantik;
        } else if (ad == "surdur") {
            k.tur = KomutTuru::calistirma_surdur;
        } else if (ad == "adimla") {
            k.tur = KomutTuru::calistirma_adimla;
            k.periyot_sayisi = static_cast<std::uint32_t>(g.bul("periyot_sayisi")->sayi);
        } else if (ad == "hiz") {
            k.tur = KomutTuru::calistirma_hiz;
            k.carpan = g.bul("carpan")->sayi;
            if (k.carpan != 0.0 && k.carpan < 0.01) {
                return ret("gecersiz_deger", "carpan 0 ya da [0,01; 100] (A3-K66)", {}, no);
            }
        } else if (ad == "durdur") {
            k.tur = KomutTuru::calistirma_durdur;
        } else {
            k.tur = KomutTuru::calistirma_sifirla;
        }
        return alici_gonder(k, no);
    }

    Yanit emniyet_degistir(const json::Deger& g, std::uint32_t istemci, std::uint32_t no, Zaman simdi,
                           const std::optional<Jeton>& sunulan)
    {
        Komut k;
        k.tur = KomutTuru::emniyet_degistir;
        k.istemci_id = istemci;
        k.beklenen_run_id = run_id_al(g);
        std::string islem;
        if (!emniyet_islemi(g, k.girdi, k.hedef_durum, k.tup, k.kisitlayan, islem)) {
            return ret("gecersiz_deger", "hedef_durum bu girdi için geçersiz (A3-K89)", {}, no);
        }
        if (k.kisitlayan) {
            // EMNİYET — A3-K92, K119: onay, jeton ve revizyon istenmez; varsa yok sayılır (jeton
            // tüketilmez); kuyruk doluluğu ve revizyon yüzünden reddedilmez.
            return kisitlayan_gonder(k, no, istemci);  // W4-K19: run_id bağı yok
        }
        // EMNİYET — A3-K93, K94, K118: gevşeten iki adımlıdır ve revizyon taşır. Jeton komut_isle'de
        // tüketildi (tek kullanımlık: ilk kullanımda, kabul ya da ret).
        const json::Deger* jet = g.bul("onay_jetonu");
        const json::Deger* onay = g.bul("onay");
        if (onay == nullptr || !onay->mantik || jet == nullptr) {
            return ret("onay_gerekli", "gevşeten değişiklik onay ve onay jetonu ister (A3-K93)", {}, no);
        }
        const json::Deger* rev = g.bul("beklenen_emniyet_rev");
        if (rev == nullptr) {
            return ret("gecersiz_istek", "gevşeten değişiklik beklenen_emniyet_rev ister (A3-K93, K118)", {}, no);
        }
        const AnlikDurum a = alici.anlik();
        if (!sunulan || !jeton_gecerli(*sunulan, istemci, islem, a, simdi)) {
            return ret("onay_jetonu_gecersiz", "onay jetonu yok, süresi geçmiş, kullanılmış ya da başka işleme ait (A3-K94)", {}, no);
        }
        if (a.emniyet_rev_doydu) {
            return ret("emniyet_rev_doydu", "emniyet revizyonu doydu (A3-K117)", {}, no);
        }
        const auto beklenen = static_cast<std::uint32_t>(rev->sayi);
        if (beklenen != a.emniyet_rev) {
            json::Nesne ay;
            ay.sayi_u("guncel_emniyet_rev", a.emniyet_rev);
            return ret("emniyet_rev_uyusmuyor", "beklenen_emniyet_rev güncel değil (A3-K118)", ay.bitir(), no);
        }
        k.beklenen_emniyet_rev = beklenen;
        return alici_gonder(k, no);
    }

    Yanit yer_istasyonu_komut(const json::Deger& g, std::uint32_t istemci, std::uint32_t no, Zaman simdi,
                              const std::optional<Jeton>& sunulan)
    {
        const std::string& komut = g.bul("komut")->metin;
        const json::Deger& par = *g.bul("parametreler");
        const auto it = yi_komutlar.find(komut);
        if (it == yi_komutlar.end()) {
            return ret("komut_listede_yok", "komut A1 kataloğunda yok (A3-K100)", {}, no);
        }
        std::string h;
        if (!komut_sema->dogrula(*it->second.parametreler, par, &h)) {
            return ret("gecersiz_deger", "parametreler komut şemasına uymuyor: " + h, {}, no);
        }
        Komut k;
        k.tur = KomutTuru::yer_istasyonu_komut;
        k.istemci_id = istemci;
        k.beklenen_run_id = run_id_al(g);
        k.yi_komut = komut;
        k.yi_sinif = it->second.sinif;
        json::kanonik_yaz(par, k.yi_parametreler_json);
        k.kisitlayan = it->second.sinif == KomutSinifi::daraltici;
        if (k.kisitlayan) {
            // EMNİYET — A3-K104, K119: daraltıcı tek adımlıdır; onay ve revizyon istemez.
            return kisitlayan_gonder(k, no, istemci);  // W4-K19: run_id bağı yok
        }
        const bool genisletici = it->second.sinif == KomutSinifi::genisletici;
        if (it->second.onay || genisletici) {
            // EMNİYET — A3-K103, K118: onaylı komut iki adımlıdır; genişletici revizyon taşır.
            const json::Deger* jet = g.bul("onay_jetonu");
            const json::Deger* onay = g.bul("onay");
            if (onay == nullptr || !onay->mantik || jet == nullptr) {
                return ret("onay_gerekli", "onaylı komut onay ve onay jetonu ister (A3-K103)", {}, no);
            }
            const json::Deger* rev = g.bul("beklenen_emniyet_rev");
            if (genisletici && rev == nullptr) {
                return ret("gecersiz_istek", "genişletici komut beklenen_emniyet_rev ister (A3-K118)", {}, no);
            }
            const AnlikDurum a = alici.anlik();
            if (!sunulan || !jeton_gecerli(*sunulan, istemci, yi_islemi(komut, par), a, simdi)) {
                return ret("onay_jetonu_gecersiz", "onay jetonu yok, süresi geçmiş, kullanılmış ya da başka işleme ait (A3-K94)", {}, no);
            }
            if (genisletici) {
                if (a.emniyet_rev_doydu) {
                    return ret("emniyet_rev_doydu", "emniyet revizyonu doydu (A3-K117)", {}, no);
                }
                const auto beklenen = static_cast<std::uint32_t>(rev->sayi);
                if (beklenen != a.emniyet_rev) {
                    json::Nesne ay;
                    ay.sayi_u("guncel_emniyet_rev", a.emniyet_rev);
                    return ret("emniyet_rev_uyusmuyor", "beklenen_emniyet_rev güncel değil (A3-K118)", ay.bitir(), no);
                }
                k.beklenen_emniyet_rev = beklenen;
            }
        }
        return alici_gonder(k, no);
    }

    static std::string etki_anahtari(const Komut& k)
    {
        if (k.tur == KomutTuru::yer_istasyonu_komut) {
            return "yi|" + k.yi_komut;
        }
        if (k.girdi == EmniyetGirdisi::acil_durdurma) {
            return "acil_durdurma";
        }
        return std::string(k.girdi == EmniyetGirdisi::silahlandirma_fisi ? "silahlandirma_fisi|" : "emniyet_pimi|") +
               (k.tup == 1 ? "1" : "0");
    }

    std::size_t dolu_mandal() const
    {
        std::size_t n = 0;
        for (const auto& [a, m] : mandallar) {
            (void)a;
            n += m.dolu ? 1u : 0u;
        }
        return n;
    }

    void mandala_koy(const Komut& k)
    {
        const auto it = mandallar.find(etki_anahtari(k));
        if (it == mandallar.end()) {
            // Anahtar kümesi sabittir (hazirla); buraya düşmek bir sunucu hatasıdır. Kısıtlayan yön:
            // etki yine iletilir (alıcı doluysa bir sonraki döngüde).
            gunluk("A3: bilinmeyen kısıtlayan etki anahtarı");
            mandallar[etki_anahtari(k)] = Mandal{};
            mandala_koy(k);
            return;
        }
        Mandal& m = it->second;
        if (m.dolu) {
            m.birlesen.push_back(m.k.komut_no);
            while (m.birlesen.size() > sinir::birlesen_kayit) {
                komutlar.erase(m.birlesen.front());  // en eski SONUÇ kaydı atılır; etki atılmaz
                m.birlesen.pop_front();
            }
        }
        m.k = k;
        m.dolu = true;
        komut_kaydet(k.komut_no, k.istemci_id, KomutDurum::kuyrukta);
        say.bekleyen_kisitlayan = dolu_mandal();
    }

    void bekleyenleri_isle()
    {
        for (;;) {
            Mandal* sonraki = nullptr;
            for (auto& [a, m] : mandallar) {
                (void)a;
                if (m.dolu && (sonraki == nullptr || m.k.komut_no < sonraki->k.komut_no)) {
                    sonraki = &m;
                }
            }
            if (sonraki == nullptr) {
                break;
            }
            const AlimSonucu a = alici.al(sonraki->k);
            if (a.tur == AlimSonucu::Tur::dolu) {
                break;
            }
            const std::uint32_t no = sonraki->k.komut_no;
            std::deque<std::uint32_t> birlesen = std::move(sonraki->birlesen);
            sonraki->birlesen.clear();
            sonraki->dolu = false;
            if (a.tur == AlimSonucu::Tur::ret) {
                // 202 verilmişti: nihai sonuç komut_sonucu olayıyla bildirilir (A3-K57).
                const std::string kod = hata_http(a.hata_kodu) == 0 ? std::string("sunucu_hatasi") : a.hata_kodu;
                komut_sonucu_yayinla(no, false, false, 0, son_sim_t_ns, std::numeric_limits<std::uint64_t>::max(), kod, {}, 1);
                for (const auto b : birlesen) {
                    komut_sonucu_yayinla(b, false, false, 0, son_sim_t_ns, std::numeric_limits<std::uint64_t>::max(), kod, {}, 1);
                }
            } else if (!birlesen.empty()) {
                birlesik_sonuc[no] = std::move(birlesen);
                while (birlesik_sonuc.size() > sinir::birlesik_sonuc) {
                    birlesik_sonuc.erase(birlesik_sonuc.begin());  // en eski sonuç kaydı
                }
            }
        }
        say.bekleyen_kisitlayan = dolu_mandal();
    }

    // ---- WebSocket -----------------------------------------------------------------------------

    void ws_yukselt(Baglanti& b, Zaman simdi)
    {
        const http::Istek& r = b.istek;
        // EMNİYET — A3-K07: alt protokol `ida-a3.v1` ve tek bir `ida-erisim.<jeton>`.
        bool v1 = false;
        std::size_t jeton_sayisi = 0;
        bool jeton_tamam = false;
        for (const auto& [ad, deger] : r.basliklar) {
            if (ad != "sec-websocket-protocol") {
                continue;
            }
            for (const auto& oge : http::liste(deger)) {
                if (oge == kWsAltProtokolu) {
                    v1 = true;
                } else if (oge.rfind("ida-erisim.", 0) == 0) {
                    ++jeton_sayisi;
                    jeton_tamam = sabit_sureli_esit(std::string_view(oge).substr(11), jeton);
                }
            }
        }
        if (!v1) {
            erisim_reddi(b, Neden::alt_protokol, r.yol, simdi);
            return;
        }
        if (jeton_sayisi != 1 || !jeton_tamam) {
            erisim_reddi(b, Neden::jeton, r.yol, simdi);
            return;
        }
        if (b.govde_asildi || !b.govde.empty()) {
            Yanit y = ret("gecersiz_istek", "WebSocket el sıkışması gövde taşımaz");
            y.kapat = true;
            yaz(b, y);
            return;
        }
        const std::string* upg = r.baslik("upgrade");
        const std::string* con = r.baslik("connection");
        bool upgrade_tamam = false;
        if (con != nullptr) {
            for (const auto& t : http::liste(*con)) {
                if (http::token_esit(t, "upgrade")) {
                    upgrade_tamam = true;
                }
            }
        }
        bool websocket = false;
        if (upg != nullptr) {
            for (const auto& t : http::liste(*upg)) {
                if (http::token_esit(t, "websocket")) {
                    websocket = true;
                }
            }
        }
        if (!upgrade_tamam || !websocket || r.kucuk_surum != 1) {
            yaz(b, ret("gecersiz_istek", "WebSocket yükseltme başlıkları eksik (RFC 6455 §4.2.1)"));
            return;
        }
        const std::string* surum = r.baslik("sec-websocket-version");
        if (surum == nullptr || *surum != "13" || r.baslik_sayisi("sec-websocket-version") != 1) {
            Yanit y = ret("gecersiz_istek", "Sec-WebSocket-Version 13 olmalı");
            y.http = 426;
            y.ek.emplace_back("Sec-WebSocket-Version", "13");
            yaz(b, y);
            return;
        }
        const std::string* anahtar = r.baslik("sec-websocket-key");
        std::string ham;
        if (anahtar == nullptr || r.baslik_sayisi("sec-websocket-key") != 1 || !ozet::base64_coz(*anahtar, ham) ||
            ham.size() != 16) {
            yaz(b, ret("gecersiz_istek", "Sec-WebSocket-Key geçersiz"));
            return;
        }
        std::size_t ws_sayisi = 0;
        for (const auto& [id, x] : bag) {
            (void)id;
            if (x->ws) {
                ++ws_sayisi;
            }
        }
        if (ws_sayisi >= ayar.azami_ws) {
            yaz(b, ret("mesgul", "WebSocket bağlantı sınırı dolu"));
            return;
        }
        // 101: yanıtta jeton yansıtılmaz, yalnız ida-a3.v1 seçilir (A3-K07). 1xx Content-Length taşımaz.
        std::string s = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n";
        s += "Sec-WebSocket-Accept: " + ozet::ws_kabul_anahtari(*anahtar) + "\r\n";
        s += "Sec-WebSocket-Protocol: ";
        s += kWsAltProtokolu;
        s += "\r\n\r\n";
        b.cikis += s;
        b.ws = true;
        b.ws_no = sonraki_ws++;
        if (sonraki_ws == 0) {
            sonraki_ws = 1;
        }
        b.son_alim = simdi;
        b.son_ping = simdi;
        b.istek = http::Istek{};
        b.govde.clear();
        // A3-K17: ilk ileti `merhaba`; A3-K29, K35: zorunlu durum kanallarının özeti ardından.
        const AnlikDurum a = alici.anlik();
        json::Nesne m;
        m.metin("tur", "merhaba")
            .metin("a3_surumu", kA3Surumu)
            .metin("katalog_ozeti", katalog_ozeti)
            .sayi_u("baglanti_id", b.ws_no);
        if (a.run_id) {
            m.metin("run_id", *a.run_id);
        } else {
            m.bos("run_id");
        }
        m.sayi_u("son_olay_sirasi", olay_sira).sayi("yayin_tavani_hz", tavan);
        ws_metin_gonder(b, m.bitir());
        std::vector<std::uint16_t> zorunlu;
        for (const auto& k : kanallar()) {
            if (k.zorunlu && k.sinif == Sinif::durum) {
                zorunlu.push_back(k.indeks);
            }
        }
        ozet_gonder(b, zorunlu);
    }

    void ws_metin_gonder(Baglanti& b, std::string_view metin)
    {
        ws::yaz(ws::Opkod::metin, reinterpret_cast<const std::uint8_t*>(metin.data()), metin.size(), b.cikis);
    }

    void ws_kapat(Baglanti& b, std::uint16_t kod, std::string_view neden)
    {
        if (b.ws_kapaniyor) {
            return;
        }
        ws::kapanis_yaz(kod, neden, b.cikis);
        b.ws_kapaniyor = true;
        b.kapanacak = true;
        b.okuma_kapali = true;
        b.olaylar.clear();
        b.dizi.clear();
        b.kuyruk_bayt = 0;
        if (kod == 4001) {
            ++say.olay_tasmasi_kapanis;
        }
    }

    void ws_isle(Baglanti& b, Zaman simdi)
    {
        std::size_t konum = 0;
        while (!b.ws_kapaniyor) {
            ws::Cerceve c;
            std::size_t t = 0;
            std::uint16_t kod = 1002;
            const std::size_t pay = b.ws_parcada ? sinir::ws_ileti - std::min(sinir::ws_ileti, b.ws_ileti.size())
                                                 : sinir::ws_ileti;
            const auto s = ws::oku(reinterpret_cast<const std::uint8_t*>(b.girdi.data()) + konum, b.girdi.size() - konum,
                                   t, c, kod, pay);
            if (s == ws::Okuma::eksik) {
                break;
            }
            if (s == ws::Okuma::hata) {
                ws_kapat(b, kod, kod == 1009 ? "ileti cok buyuk" : "protokol hatasi");
                break;
            }
            konum += t;
            b.son_alim = simdi;  // A3-K46: herhangi bir çerçeve canlılıktır
            ws_cerceve(b, c);
        }
        b.girdi.erase(0, konum);
        if (b.ws_kapaniyor) {
            b.girdi.clear();
        }
    }

    void ws_cerceve(Baglanti& b, ws::Cerceve& c)
    {
        switch (c.opkod) {
        case 0x9:  // ping → pong (RFC 6455 §5.5.2)
            ws::yaz(ws::Opkod::pong, reinterpret_cast<const std::uint8_t*>(c.yuk.data()), c.yuk.size(), b.cikis);
            return;
        case 0xA:
            return;
        case 0x8: {
            std::uint16_t kod = 1000;
            if (c.yuk.size() == 1) {
                kod = 1002;
            } else if (c.yuk.size() >= 2) {
                const auto gelen_kod = static_cast<std::uint16_t>((static_cast<unsigned char>(c.yuk[0]) << 8u) |
                                                                  static_cast<unsigned char>(c.yuk[1]));
                const bool gecerli = (gelen_kod >= 1000 && gelen_kod <= 1003) || (gelen_kod >= 1007 && gelen_kod <= 1011) ||
                                     (gelen_kod >= 3000 && gelen_kod <= 4999);
                if (!gecerli) {
                    kod = 1002;
                } else if (!json::utf8_gecerli(std::string_view(c.yuk).substr(2))) {
                    kod = 1007;
                } else {
                    kod = gelen_kod;
                }
            }
            ws_kapat(b, kod, "");
            return;
        }
        case 0x2:
            ws_kapat(b, 1003, "istemciden ikili ileti");  // A3-K47
            return;
        case 0x1:
            if (b.ws_parcada) {
                ws_kapat(b, 1002, "parcali ileti sirasinda yeni ileti");
                return;
            }
            if (!c.fin) {
                b.ws_parcada = true;
                b.ws_ileti = std::move(c.yuk);
                return;
            }
            ws_metin_isle(b, c.yuk);
            return;
        case 0x0:
            if (!b.ws_parcada) {
                ws_kapat(b, 1002, "beklenmeyen devam cercevesi");
                return;
            }
            b.ws_ileti += c.yuk;
            if (c.fin) {
                b.ws_parcada = false;
                std::string ileti = std::move(b.ws_ileti);
                b.ws_ileti.clear();
                ws_metin_isle(b, ileti);
            }
            return;
        default:
            ws_kapat(b, 1002, "bilinmeyen opkod");
            return;
        }
    }

    void ws_hata(Baglanti& b, std::optional<std::uint64_t> istek_no, std::string_view kod, std::string_view ileti)
    {
        json::Nesne n;
        n.metin("tur", "hata");
        if (istek_no) {
            n.sayi_u("istek_no", *istek_no);
        } else {
            n.bos("istek_no");
        }
        n.metin("kod", kod).metin("ileti", kirp_utf8(ileti, 1000));
        ws_metin_gonder(b, n.bitir());
    }

    void ws_metin_isle(Baglanti& b, const std::string& metin)
    {
        if (!json::utf8_gecerli(metin)) {
            ws_kapat(b, 1007, "gecersiz UTF-8");  // A3-K47
            return;
        }
        json::Deger d;
        std::string h;
        json::CozumSiniri s;
        s.azami_deger = 4096;
        s.azami_derinlik = 16;
        if (!json::coz(metin, d, &h, s)) {
            ws_hata(b, std::nullopt, "gecersiz_ileti", "JSON çözülemedi");  // A3-K48
            return;
        }
        if (!d.nesne_mi()) {
            ws_hata(b, std::nullopt, "gecersiz_ileti", "kök JSON nesnesi değil");
            return;
        }
        std::optional<std::uint64_t> istek_no;
        if (const json::Deger* n = d.bul("istek_no"); n != nullptr && n->tamsayi_mi() && n->sayi >= 1 &&
                                                       n->sayi <= 9007199254740991.0) {
            istek_no = static_cast<std::uint64_t>(n->sayi);
        }
        const json::Deger* tur = d.bul("tur");
        if (tur == nullptr || tur->tur != json::Tur::metin) {
            ws_hata(b, istek_no, "gecersiz_ileti", "tur alanı yok");
            return;
        }
        const bool abone = tur->metin == "abone_ol";
        if (!abone && tur->metin != "abonelik_birak") {
            ws_hata(b, istek_no, "bilinmeyen_tur", "bilinmeyen ileti türü");
            return;
        }
        // Yapı (`$defs/ws_abone_ol`, `ws_abonelik_birak`); `hz` değeri kümede değilse ileti değil
        // yalnız o kanal `gecersiz_hz` ile reddedilir (A3-K30).
        const json::Deger* liste = d.bul("kanallar");
        bool yapi = istek_no.has_value() && liste != nullptr && liste->tur == json::Tur::dizi && !liste->dizi.empty() &&
                    liste->dizi.size() <= 64;
        for (const auto& [ad, v] : d.nesne) {
            (void)v;
            if (ad != "tur" && ad != "istek_no" && ad != "kanallar") {
                yapi = false;
            }
        }
        if (yapi) {
            for (const auto& o : liste->dizi) {
                if (abone) {
                    const json::Deger* k = o.bul("kanal");
                    const json::Deger* hz = o.bul("hz");
                    bool tamam = o.nesne_mi() && k != nullptr && k->tur == json::Tur::metin &&
                                 (hz == nullptr || hz->tamsayi_mi());
                    if (tamam) {
                        std::vector<std::uint32_t> kp;
                        tamam = json::kod_noktalari(k->metin, kp) && kp.size() <= 64;
                    }
                    for (const auto& [ad, v] : o.nesne) {
                        (void)v;
                        if (ad != "kanal" && ad != "hz") {
                            tamam = false;
                        }
                    }
                    yapi = yapi && tamam;
                } else {
                    std::vector<std::uint32_t> kp;
                    yapi = yapi && o.tur == json::Tur::metin && json::kod_noktalari(o.metin, kp) && kp.size() <= 64;
                }
            }
        }
        if (!yapi) {
            ws_hata(b, istek_no, "gecersiz_ileti", "ileti şemaya uymuyor (A3-K48)");
            return;
        }
        std::string yanit = "[";
        std::vector<std::uint16_t> ozetlenecek;
        for (const auto& o : liste->dizi) {
            const std::string& ad = abone ? o.bul("kanal")->metin : o.metin;
            const KanalTanimi* k = kanal_bul(ad);
            json::Nesne n;
            n.metin("kanal", ad);
            if (k == nullptr) {
                n.metin("sonuc", "ret").metin("neden", "bilinmeyen_kanal");
            } else if (abone) {
                // Sol #4: hz önce double olarak izinli altı değerle karşılaştırılır; tamsayıya yalnız
                // izinliyse dönüştürülür (aralık dışı double → tamsayı dönüşümü tanımsızdır).
                // Dizi/olay sınıfında hz yok sayılır ve dönüştürülmez (A3-K30).
                const json::Deger* hz = o.bul("hz");
                bool hz_gecerli = hz == nullptr;
                std::uint32_t istenen = k->varsayilan_hz;
                if (hz != nullptr && k->sinif == Sinif::durum) {
                    for (const double izinli : {50.0, 25.0, 10.0, 5.0, 2.0, 1.0}) {
                        if (hz->sayi == izinli) {
                            hz_gecerli = true;
                            istenen = static_cast<std::uint32_t>(izinli);
                        }
                    }
                }
                if (k->sinif == Sinif::durum && !hz_gecerli) {
                    n.metin("sonuc", "ret").metin("neden", "gecersiz_hz");
                } else {
                    n.sayi("kanal_id", k->kanal_id).sayi("kanal_surumu", k->kanal_surumu);
                    if (k->sinif == Sinif::durum) {
                        const auto ist = k->zorunlu ? std::uint32_t{k->varsayilan_hz} : istenen;
                        n.sayi("etkin_hz", etkin_hz(*k, ist));
                        if (!k->zorunlu) {
                            b.abonelik[k->indeks] = static_cast<std::uint8_t>(ist);
                        }
                        ozetlenecek.push_back(k->indeks);
                    } else {
                        n.bos("etkin_hz");
                        if (!k->zorunlu) {
                            b.abonelik[k->indeks] = 0;
                        }
                    }
                    n.metin("sonuc", "kabul");
                }
            } else if (k->zorunlu) {
                n.metin("sonuc", "ret").metin("neden", "zorunlu_kanal");
            } else {
                b.abonelik.erase(k->indeks);
                const auto q = b.dizi.find(k->indeks);
                if (q != b.dizi.end()) {
                    for (const auto& x : q->second) {
                        b.kuyruk_bayt -= std::min(b.kuyruk_bayt, x->yuk.size() + kBaslikBayt);
                    }
                    b.dizi.erase(q);
                }
                for (auto it = b.gonderilen.begin(); it != b.gonderilen.end();) {
                    it = it->first.kanal == k->indeks ? b.gonderilen.erase(it) : std::next(it);
                }
                n.sayi("kanal_id", k->kanal_id).sayi("kanal_surumu", k->kanal_surumu).metin("sonuc", "kabul");
            }
            if (yanit.size() > 1) {
                yanit.push_back(',');
            }
            yanit += n.bitir();
        }
        yanit.push_back(']');
        json::Nesne n;
        n.metin("tur", "abonelik_yaniti").sayi_u("istek_no", *istek_no).ham("kanallar", yanit);
        ws_metin_gonder(b, n.bitir());
        ozet_gonder(b, ozetlenecek);  // A3-K35
    }

    std::uint32_t etkin_hz(const KanalTanimi& k, std::uint32_t istenen) const
    {
        return std::min({istenen, std::uint32_t{k.azami_hz}, std::uint32_t{tavan}});  // A3-K32
    }

    // ---- yayın ---------------------------------------------------------------------------------

    void kare_yaz(Baglanti& b, const KanalTanimi& k, const Kare& kr, std::uint16_t bayraklar, std::string& ileti)
    {
        CerceveBasligi h;
        h.t_ns = kr.t_ns < 0 ? 0 : kr.t_ns;
        h.sira = kr.sira;
        h.yuk_uzunlugu = static_cast<std::uint32_t>(kr.yuk.size());
        h.kanal_id = k.kanal_id;
        h.kanal_surumu = k.kanal_surumu;
        h.bayraklar = static_cast<std::uint16_t>(bayraklar | (k.yer_gercegi ? bayrak::yer_gercegi : 0u));
        std::uint8_t bas[kBaslikBayt];
        baslik_yaz(h, bas);
        ileti.append(reinterpret_cast<const char*>(bas), kBaslikBayt);
        const std::size_t yuk_bas = ileti.size();
        ileti.append(reinterpret_cast<const char*>(kr.yuk.data()), kr.yuk.size());
        if (k.indeks == idx_baglanti && kr.yuk.size() >= 20) {
            // Bağlantıya özgü alanlar (A3-K40, K41): son 20 bayt.
            std::uint8_t kuyruk[20];
            Yazici w(kuyruk, sizeof kuyruk);
            w.u64(b.ezilen);
            w.u64(b.dusen_dizi);
            w.u32(static_cast<std::uint32_t>(b.olaylar.size()));
            std::memcpy(ileti.data() + yuk_bas + kr.yuk.size() - 20, kuyruk, 20);
        }
    }

    void ikili_gonder(Baglanti& b, std::string& ileti)
    {
        if (!ileti.empty()) {
            ws::yaz(ws::Opkod::ikili, reinterpret_cast<const std::uint8_t*>(ileti.data()), ileti.size(), b.cikis);
            ileti.clear();
        }
    }

    void ozet_gonder(Baglanti& b, const std::vector<std::uint16_t>& kanal_indeksleri)
    {
        if (kanal_indeksleri.empty()) {
            return;
        }
        const auto tum = kanallar();
        std::string ileti;
        for (const auto& [ka, s] : son) {
            if (std::find(kanal_indeksleri.begin(), kanal_indeksleri.end(), ka.kanal) == kanal_indeksleri.end() || !s.kare) {
                continue;
            }
            if (ileti.size() + s.kare->yuk.size() + kBaslikBayt > sinir::ikili_ileti) {
                ikili_gonder(b, ileti);
            }
            kare_yaz(b, tum[ka.kanal], *s.kare, bayrak::ozet, ileti);
            b.gonderilen[ka] = s.uretim;
        }
        ikili_gonder(b, ileti);
    }

    void yayin_tiki()
    {
        ++say.tik;
        const auto tum = kanallar();
        std::size_t toplam_kuyruk = 0;
        for (auto& [id, bp] : bag) {
            (void)id;
            Baglanti& b = *bp;
            if (!b.ws || b.ws_kapaniyor) {
                continue;
            }
            toplam_kuyruk += b.kuyruk_bayt + b.bekleyen_cikis();
            if (b.bekleyen_cikis() > sinir::baski_esigi) {
                continue;  // A3-K39: bu bağlantı yazamıyor; veri kendi kuyruklarında bekler
            }
            std::string ileti;
            std::size_t eklenen = 0;
            auto yer = [&](std::size_t n) {
                if (!ileti.empty() && ileti.size() + n > sinir::ikili_ileti) {
                    ikili_gonder(b, ileti);
                }
                eklenen += n;
            };
            // Olaylar (kayıpsız, sıralı; A3-K34, K42)
            while (!b.olaylar.empty() && eklenen < sinir::ikili_ileti) {
                const OlayP& o = b.olaylar.front();
                yer(o->cerceve.size());
                ileti.append(reinterpret_cast<const char*>(o->cerceve.data()), o->cerceve.size());
                b.kuyruk_bayt -= std::min(b.kuyruk_bayt, o->cerceve.size());
                b.olaylar.pop_front();
            }
            // Dizi sınıfı (A3-K34, K41)
            for (auto& [ki, q] : b.dizi) {
                while (!q.empty() && eklenen < sinir::ikili_ileti) {
                    const KareP kr = q.front();
                    q.pop_front();
                    b.kuyruk_bayt -= std::min(b.kuyruk_bayt, kr->yuk.size() + kBaslikBayt);
                    const bool isaretli = b.dusme.erase(kr->ka) != 0;
                    const bool dustu = kr->oncesinde_dusme || isaretli;
                    yer(kr->yuk.size() + kBaslikBayt);
                    kare_yaz(b, tum[ki], *kr, dustu ? bayrak::oncesinde_dusme : std::uint16_t{0}, ileti);
                }
            }
            // Durum sınıfı (A3-K33, K40): her 50/h tikte, yeni değer varsa yalnız sonuncusu
            for (const auto& [ka, s] : son) {
                if (eklenen >= sinir::ikili_ileti) {
                    break;
                }
                const KanalTanimi& k = tum[ka.kanal];
                std::uint32_t istenen = 0;
                if (k.zorunlu) {
                    istenen = k.varsayilan_hz;
                } else {
                    const auto a = b.abonelik.find(ka.kanal);
                    if (a == b.abonelik.end()) {
                        continue;  // A3-K36
                    }
                    istenen = a->second;
                }
                const std::uint32_t h = etkin_hz(k, istenen);
                if (h == 0 || (say.tik % (50u / h)) != 0) {
                    continue;
                }
                std::uint64_t& g = b.gonderilen[ka];
                if (s.uretim <= g || !s.kare) {
                    continue;
                }
                if (g > 0 && s.uretim - g > 1) {
                    b.ezilen += s.uretim - g - 1;
                    say.ezilen_durum += s.uretim - g - 1;
                }
                g = s.uretim;
                yer(s.kare->yuk.size() + kBaslikBayt);
                kare_yaz(b, k, *s.kare, 0, ileti);
            }
            ikili_gonder(b, ileti);
            say.azami_cikis_bayt = std::max<std::uint64_t>(say.azami_cikis_bayt, b.bekleyen_cikis());
        }
        say.azami_bellek_tahmini = std::max<std::uint64_t>(say.azami_bellek_tahmini, toplam_kuyruk);
    }

    // ---- dünya → A3 halkaları ------------------------------------------------------------------

    void telemetri_isle()
    {
        const auto tum = kanallar();
        std::size_t n = 0;
        const std::uint8_t* p = nullptr;
        while ((p = yayin.telemetri.oku(n)) != nullptr) {
            if (n >= kTelemetriBaslik) {
                std::uint16_t idx = 0;
                std::uint16_t bayrak_ = 0;
                std::uint64_t anahtar = 0;
                std::int64_t t_ns = 0;
                std::memcpy(&idx, p, 2);
                std::memcpy(&bayrak_, p + 2, 2);
                std::memcpy(&anahtar, p + 8, 8);
                std::memcpy(&t_ns, p + 16, 8);
                if (static_cast<std::size_t>(idx) < tum.size() && tum[idx].sinif != Sinif::olay) {
                    std::vector<std::uint8_t> yuk(p + kTelemetriBaslik, p + n);
                    kare_al(tum[idx], KA{idx, anahtar}, t_ns, (bayrak_ & 1u) != 0, std::move(yuk));
                }
            }
            yayin.telemetri.birak();
        }
    }

    void kare_al(const KanalTanimi& k, KA ka, std::int64_t t_ns, bool dusme, std::vector<std::uint8_t> yuk)
    {
        if (k.sinif == Sinif::durum) {
            SonDeger& s = son[ka];
            ++s.uretim;
            auto kr = std::make_shared<Kare>();
            kr->ka = ka;
            kr->t_ns = t_ns;
            kr->sira = static_cast<std::uint32_t>(s.uretim);  // A3-K27
            kr->yuk = std::move(yuk);
            if (k.indeks == idx_calistirma) {
                calistirma_gozle(*kr);
            }
            s.kare = std::move(kr);
            return;
        }
        std::uint64_t& sayac = dizi_sayac[ka];
        ++sayac;
        auto kr = std::make_shared<Kare>();
        kr->ka = ka;
        kr->t_ns = t_ns;
        kr->sira = static_cast<std::uint32_t>(sayac);
        kr->oncesinde_dusme = dusme;
        kr->yuk = std::move(yuk);
        const KareP p = kr;
        for (auto& [id, bp] : bag) {
            (void)id;
            Baglanti& b = *bp;
            if (!b.ws || b.ws_kapaniyor || b.abonelik.count(k.indeks) == 0) {
                continue;  // A3-K36
            }
            auto& q = b.dizi[k.indeks];
            q.push_back(p);
            b.kuyruk_bayt += p->yuk.size() + kBaslikBayt;
            while (q.size() > k.kuyruk_siniri) {
                const KareP d = q.front();
                q.pop_front();
                b.kuyruk_bayt -= std::min(b.kuyruk_bayt, d->yuk.size() + kBaslikBayt);
                ++b.dusen_dizi;
                ++say.dusen_dizi;
                b.dusme.insert(d->ka);  // A3-K41
            }
        }
    }

    void calistirma_gozle(const Kare& kr)
    {
        yuk_calistirma_durum c;
        if (coz(kr.yuk.data(), kr.yuk.size(), c)) {
            son_durum = c.durum;
            son_run_id = c.run_id.empty() ? std::nullopt : std::optional<std::string>(std::string(c.run_id));
            son_sim_t_ns = kr.t_ns;
        }
    }

    void olay_halkasi_isle()
    {
        std::size_t n = 0;
        const std::uint8_t* p = nullptr;
        while ((p = yayin.olaylar.oku(n)) != nullptr) {
            if (n >= kOlayBaslik && p[0] == 0) {
                std::int64_t t_ns = 0;
                std::memcpy(&t_ns, p + 8, 8);
                yuk_olay o;
                if (coz(p + kOlayBaslik, n - kOlayBaslik, o)) {
                    const std::string ayrinti(o.ayrinti);
                    json::Deger d;
                    json::CozumSiniri s;
                    s.azami_deger = 8192;
                    if (json::coz(ayrinti, d, nullptr, s) && d.nesne_mi()) {
                        olay_yayinla(t_ns, o.tur, o.onem, o.kaynak, o.adim_no, o.komut_no, o.istemci_id, ayrinti);
                    } else {
                        ++gecersiz_olay;  // olay_kaybi olarak bildirilir
                    }
                } else {
                    ++gecersiz_olay;
                }
            } else if (n >= kSonucBaslik && p[0] == 1) {
                std::uint32_t no = 0;
                std::int64_t t_ns = 0;
                std::uint64_t adim = 0;
                std::uint64_t adim_no = 0;
                std::uint16_t kod_boy = 0;
                std::uint32_t ayr_boy = 0;
                std::memcpy(&no, p + 4, 4);
                std::memcpy(&t_ns, p + 8, 8);
                std::memcpy(&adim, p + 16, 8);
                std::memcpy(&adim_no, p + 24, 8);
                std::memcpy(&kod_boy, p + 32, 2);
                std::memcpy(&ayr_boy, p + 36, 4);
                if (kSonucBaslik + std::size_t{kod_boy} + std::size_t{ayr_boy} <= n) {
                    const std::string kod(reinterpret_cast<const char*>(p + kSonucBaslik), kod_boy);
                    const std::string ayr(reinterpret_cast<const char*>(p + kSonucBaslik + kod_boy), ayr_boy);
                    komut_sonucu_yayinla(no, p[1] != 0, p[2] != 0, adim, t_ns, adim_no, kod, ayr, 0);
                    // W4-K22 (a): mandalda birleşmiş isteklerin sonucu iletilenin sonucudur.
                    const auto bs = birlesik_sonuc.find(no);
                    if (bs != birlesik_sonuc.end()) {
                        const std::deque<std::uint32_t> birlesen = std::move(bs->second);
                        birlesik_sonuc.erase(bs);
                        for (const auto b : birlesen) {
                            komut_sonucu_yayinla(b, p[1] != 0, p[2] != 0, adim, t_ns, adim_no, kod, ayr, 0);
                        }
                    }
                } else {
                    ++gecersiz_olay;
                }
            } else {
                ++gecersiz_olay;
            }
            yayin.olaylar.birak();
        }
    }

    void komut_sonucu_yayinla(std::uint32_t no, bool uygulandi, bool adim_var, std::uint64_t adim, std::int64_t t_ns,
                              std::uint64_t adim_no, const std::string& kod, const std::string& ayrinti,
                              std::uint8_t kaynak)
    {
        std::uint32_t istemci = 0;
        const auto it = komutlar.find(no);
        std::string sonuc_ayrinti;
        if (!ayrinti.empty()) {
            json::Deger d;
            const json::Deger* sn = sema->tanim("sinirli_nesne");
            if (json::coz(ayrinti, d) && sn != nullptr && sema->dogrula(*sn, d)) {
                sonuc_ayrinti = ayrinti;
            } else {
                gunluk("A3: komut sonucu ayrıntısı $defs/sinirli_nesne'ye uymuyor; ayrıntı atıldı");
            }
        }
        if (it != komutlar.end()) {
            istemci = it->second.istemci;
            it->second.durum = uygulandi ? KomutDurum::uygulandi : KomutDurum::reddedildi;
            it->second.adim_var = adim_var;
            it->second.adim = adim;
            it->second.hata_kodu = kod;
            it->second.sonuc_ayrinti = sonuc_ayrinti;
        }
        json::Nesne n;
        n.sayi_u("komut_no", no == 0 ? 1u : no).metin("sonuc", uygulandi ? "uygulandi" : "reddedildi");
        if (adim_var) {
            n.sayi_u("uygulama_adimi", std::min<std::uint64_t>(adim, 9007199254740991ull));
        }
        if (!kod.empty()) {
            n.metin("hata_kodu", kod);
        }
        if (!sonuc_ayrinti.empty()) {
            n.ham("sonuc_ayrinti", sonuc_ayrinti);
        }
        olay_yayinla(t_ns, 5, 0, kaynak, adim_no, no, istemci, n.bitir());
    }

    // A3-K45: saklanan çerçeveden `yuk_olay`'ın JSON karşılığı.
    static std::string olay_json(const OlayKaydi& k)
    {
        CerceveBasligi h;
        yuk_olay o;
        if (!baslik_oku(k.cerceve.data(), k.cerceve.size(), h) ||
            !coz(k.cerceve.data() + kBaslikBayt, k.cerceve.size() - kBaslikBayt, o)) {
            return "{}";  // kendi kodladığımız çerçeve: olmaz
        }
        json::Nesne j;
        const OlayTuru* ot = olay_turu_bul(o.tur);
        const std::string_view onem_adi = deger_adi("yuk_olay", "onem", o.onem);
        const std::string_view kaynak_adi = deger_adi("yuk_olay", "kaynak", o.kaynak);
        j.sayi_u("sira", h.sira).sayi("t_ns", h.t_ns);
        j.metin("tur", ot != nullptr ? std::string(ot->ad) : "tur_" + std::to_string(o.tur));
        j.metin("onem", onem_adi.empty() ? std::to_string(o.onem) : std::string(onem_adi));
        j.metin("kaynak", kaynak_adi.empty() ? std::to_string(o.kaynak) : std::string(kaynak_adi));
        if (o.adim_no == std::numeric_limits<std::uint64_t>::max()) {
            j.bos("adim_no");
        } else {
            j.sayi_u("adim_no", o.adim_no);
        }
        j.sayi_u("komut_no", o.komut_no).sayi_u("istemci_id", o.istemci_id).ham("ayrinti", o.ayrinti);
        return j.bitir();
    }

    void olay_yayinla(std::int64_t t_ns, std::uint16_t tur, std::uint8_t onem, std::uint8_t kaynak, std::uint64_t adim_no,
                      std::uint32_t komut, std::uint32_t istemci, const std::string& ayrinti)
    {
        yuk_olay o;
        o.tur = tur;
        o.onem = onem;
        o.kaynak = kaynak;
        o.adim_no = adim_no;
        o.komut_no = komut;
        o.istemci_id = istemci;
        o.ayrinti = ayrinti;
        const std::size_t boy = boyut(o);
        auto kayit = std::make_shared<OlayKaydi>();
        kayit->cerceve.resize(kBaslikBayt + boy);
        std::size_t yazilan = 0;
        if (boy > kYukAzamiBayt || !kodla(o, kayit->cerceve.data() + kBaslikBayt, boy, yazilan)) {
            ++gecersiz_olay;
            return;
        }
        ++olay_sira;
        if (olay_sira == 0) {
            olay_sira = 1;
        }
        kayit->sira = olay_sira;
        CerceveBasligi h;
        h.t_ns = t_ns < 0 ? 0 : t_ns;
        h.sira = olay_sira;
        h.yuk_uzunlugu = static_cast<std::uint32_t>(boy);
        h.kanal_id = olay_kanali->kanal_id;
        h.kanal_surumu = olay_kanali->kanal_surumu;
        baslik_yaz(h, kayit->cerceve.data());
        // W4-K22 (b): geçmiş tek gösterimle (çerçeve) saklanır; pencere 65 536 olay ya da 64 MiB,
        // hangisi önce dolarsa (A3-K45'in küçük sürüm notu).
        const OlayP op = kayit;
        gecmis.push_back(op);
        gecmis_bayt += op->cerceve.size();
        while (gecmis.size() > sinir::olay_gecmisi || gecmis_bayt > sinir::olay_gecmisi_bayt) {
            gecmis_bayt -= gecmis.front()->cerceve.size();
            gecmis.pop_front();
        }
        for (auto& [id, bp] : bag) {
            (void)id;
            Baglanti& b = *bp;
            if (!b.ws || b.ws_kapaniyor) {
                continue;
            }
            if (b.olaylar.size() >= sinir::olay_kuyrugu) {
                ws_kapat(b, 4001, "olay kuyrugu tasti");  // A3-K42
                continue;
            }
            b.olaylar.push_back(op);
            b.kuyruk_bayt += op->cerceve.size();
        }
        say.olay_sirasi = olay_sira;
    }

    void olay_kaybi_denetle()
    {
        const std::uint64_t d = yayin.olay_dusen.load(std::memory_order_relaxed);
        const std::uint64_t yeni = d - gorulen_olay_dusen + gecersiz_olay;
        if (yeni > 0) {
            gorulen_olay_dusen = d;
            gecersiz_olay = 0;
            json::Nesne a;
            a.sayi_u("kayip_sayisi", std::min<std::uint64_t>(yeni, 9007199254740991ull));
            // A3-K44: düşen olaylara sıra verilmez; kayıp bu olayla görünür.
            olay_yayinla(son_sim_t_ns, 18, 2, 1, std::numeric_limits<std::uint64_t>::max(), 0, 0, a.bitir());
        }
    }

    // ---- zaman aşımları ve bakım ---------------------------------------------------------------

    // Çıkış baskısı yüzünden sırada bekleyen HTTP istekleri, baskı kalkınca işlenir.
    void bekleyen_girdileri_isle(Zaman simdi)
    {
        std::vector<Baglanti*> l;
        for (auto& [id, bp] : bag) {
            (void)id;
            if (!bp->ws && !bp->kapanacak && !bp->sira.empty() && bp->bekleyen_cikis() <= sinir::okuma_esigi) {
                l.push_back(bp.get());
            }
        }
        for (Baglanti* b : l) {
            sirayi_isle(*b, simdi);
        }
    }

    void zaman_asimlari(Zaman simdi)
    {
        for (auto& [id, bp] : bag) {
            (void)id;
            Baglanti& b = *bp;
            if (b.yazildi) {
                b.yazildi = false;
                b.son_yazma = simdi;
            }
            if (b.kapanacak) {
                // Kapanacak bağlantının çıkışını okumayan karşı taraf: kapanma kararından (ya da son
                // yazma ilerlemesinden) bu yana süre dolunca çıkış atılır, bağlantı kapatılır.
                if (!b.kapanma_goruldu) {
                    b.kapanma_goruldu = true;
                    b.kapanma_bas = simdi;
                }
                if (b.bekleyen_cikis() > 0 && simdi - std::max(b.kapanma_bas, b.son_yazma) > sure::kapanis) {
                    b.cikis.clear();
                    b.cikis_bas = 0;
                }
                continue;
            }
            if (b.ws) {
                if (simdi - b.son_alim > sure::canlilik) {
                    ws_kapat(b, 4004, "canlilik yok");  // A3-K46
                } else if (simdi - b.son_ping >= sure::ping) {
                    ws::yaz(ws::Opkod::ping, nullptr, 0, b.cikis);
                    b.son_ping = simdi;
                }
                continue;
            }
            const bool baslikta = b.asama == Asama::baslik;
            if ((baslikta && b.istek_var && simdi - b.istek_bas > sure::baslik) ||
                (!baslikta && simdi - b.istek_bas > sure::govde) ||
                (baslikta && !b.istek_var && b.sira.empty() && b.bekleyen_cikis() == 0 &&
                 simdi - b.son_etkinlik > sure::bosta) ||
                (b.eof && simdi - std::max(b.eof_bas, b.son_yazma) > sure::kapanis)) {
                eksik_istegi_birak(b);
                b.kapanacak = true;
                b.okuma_kapali = true;
                sirayi_isle(b, simdi);
            }
        }
    }

    void bakim(Zaman simdi)
    {
        for (auto it = jetonlar.begin(); it != jetonlar.end();) {
            it = simdi > it->second.son ? jetonlar.erase(it) : std::next(it);
        }
        const bool bitti = son_durum == 5 || son_durum == 6 || son_durum == 7;
        for (auto it = yinelemeler.begin(); it != yinelemeler.end();) {
            const YinelemeKaydi& k = it->second;
            const bool sure_doldu = simdi - k.zaman > sure::yineleme;
            const bool calistirma_bitti = k.run_id && (son_run_id != k.run_id || bitti);
            it = sure_doldu || calistirma_bitti ? yinelemeler.erase(it) : std::next(it);
        }
    }
};

// ---- Cekirdek ----------------------------------------------------------------------------------

Cekirdek::Cekirdek(const SunucuAyari& ayar, KomutAlicisi& alici, Yayinci::Ic& yayin, std::string jeton, std::uint16_t port)
    : ic_(std::make_unique<Ic>(ayar, alici, yayin, std::move(jeton), port))
{
}

Cekirdek::~Cekirdek() = default;

bool Cekirdek::hazirla(std::string& hata) { return ic_->hazirla(hata); }

BagId Cekirdek::ac(Zaman simdi, std::vector<BagId>& kapatilacak) { return ic_->ac(simdi, kapatilacak); }

void Cekirdek::gelen(BagId id, const std::uint8_t* veri, std::size_t n, Zaman simdi)
{
    if (Baglanti* b = ic_->bul(id)) {
        ic_->gelen(*b, veri, n, simdi);
    }
}

void Cekirdek::karsi_kapatti(BagId id, Zaman simdi)
{
    Baglanti* b = ic_->bul(id);
    if (b == nullptr) {
        return;
    }
    b->okuma_kapali = true;
    if (b->ws) {
        b->kapanacak = true;
        return;
    }
    // W4-K23 (3): gönderim yönü kapanınca sıradaki tam istekler işlenmeyi sürdürür (kısıtlayanlar
    // hemen); sıra boşalınca bağlantı kapanır. Eksik kalan istek atılır (jeton kuralı).
    if (!b->eof) {
        b->eof = true;
        b->eof_bas = simdi;
    }
    ic_->eksik_istegi_birak(*b);
    ic_->sirayi_isle(*b, simdi);
}

std::string_view Cekirdek::cikis(BagId id) const
{
    const Baglanti* b = ic_->bul(id);
    if (b == nullptr) {
        return {};
    }
    return std::string_view(b->cikis).substr(b->cikis_bas);
}

void Cekirdek::yazildi(BagId id, std::size_t n)
{
    Baglanti* b = ic_->bul(id);
    if (b == nullptr) {
        return;
    }
    if (n > 0) {
        b->yazildi = true;
    }
    b->cikis_bas += std::min(n, b->bekleyen_cikis());
    if (b->cikis_bas == b->cikis.size()) {
        b->cikis.clear();
        b->cikis_bas = 0;
    } else if (b->cikis_bas > (std::size_t{1} << 20u)) {
        b->cikis.erase(0, b->cikis_bas);
        b->cikis_bas = 0;
    }
}

bool Cekirdek::okumali(BagId id) const
{
    const Baglanti* b = ic_->bul(id);
    if (b == nullptr || b->okuma_kapali || b->kapanacak) {
        return false;
    }
    if (b->ws) {
        return b->bekleyen_cikis() < sinir::okuma_esigi && b->girdi.size() < sinir::senaryo_govde + http::kAzamiBaslikBlogu;
    }
    // W4-K23 (3): HTTP girdisi çıkış baskısından bağımsız okunur (çerçeveleme sürer); sıra
    // sınırıyla ve çerçeveleme durunca (yükseltme/kapatma bekleniyor) okunmaz.
    return !b->cerceve_durdu && !b->sira_tasti && b->girdi.size() < sinir::senaryo_govde + http::kAzamiBaslikBlogu;
}

bool Cekirdek::kapatilmali(BagId id) const
{
    const Baglanti* b = ic_->bul(id);
    return b == nullptr || (b->kapanacak && b->bekleyen_cikis() == 0);
}

void Cekirdek::kapandi(BagId id)
{
    if (Baglanti* b = ic_->bul(id)) {
        ic_->baglanti_birak(*b);
    }
    ic_->bag.erase(id);
}

KabulKarari Cekirdek::kabul_karari() const { return ic_->kabul_karari(); }

void Cekirdek::okuma_firsati_ver(BagId id)
{
    if (Baglanti* b = ic_->bul(id)) {
        b->okuma_firsati = true;
    }
}

std::string Cekirdek::dolu_yaniti() const
{
    return http::yanit(503, "application/json", ic_->ret_govde("mesgul", "bağlantı sınırı dolu", {}, 0), true);
}

std::vector<BagId> Cekirdek::baglantilar() const
{
    std::vector<BagId> r;
    r.reserve(ic_->bag.size());
    for (const auto& [id, b] : ic_->bag) {
        (void)b;
        r.push_back(id);
    }
    return r;
}

void Cekirdek::dongu(Zaman simdi)
{
    Ic& ic = *ic_;
    ic.telemetri_isle();
    ic.olay_halkasi_isle();
    ic.olay_kaybi_denetle();
    ic.bekleyenleri_isle();
    for (std::size_t n = 0; n < ic.retler.size(); ++n) {
        if (ic.retler[n].biriken > 0 && simdi - ic.retler[n].son >= sure::erisim_olayi) {
            ic.erisim_olayi_yayinla(static_cast<Neden>(n), simdi);
        }
    }
    ic.bekleyen_girdileri_isle(simdi);
    ic.zaman_asimlari(simdi);
    if (simdi >= ic.sonraki_bakim) {
        ic.bakim(simdi);
        ic.sonraki_bakim = simdi + std::chrono::seconds(1);
    }
    if (ic.dis_tik) {
        return;  // tik sunucunun tik iş parçacığından gelir (Cekirdek::tik)
    }
    if (!ic.tik_basladi) {
        ic.tik_basladi = true;
        ic.sonraki_tik = simdi;
    }
    if (simdi >= ic.sonraki_tik) {
        ic.yayin_tiki();
        ic.sonraki_tik += sure::yayin_tiki;
        if (simdi - ic.sonraki_tik > std::chrono::milliseconds(100)) {
            ic.sonraki_tik = simdi + sure::yayin_tiki;  // uzun takılmadan sonra birikmiş tikleri koşma
        }
    }
}

void Cekirdek::dis_tik(bool acik) { ic_->dis_tik = acik; }

void Cekirdek::tik() { ic_->yayin_tiki(); }

std::chrono::milliseconds Cekirdek::tike_kalan(Zaman simdi) const
{
    if (ic_->dis_tik) {
        return sure::yayin_tiki;  // yalnız yedek uyanma; tik uyandırma soketinden gelir
    }
    if (!ic_->tik_basladi || simdi >= ic_->sonraki_tik) {
        return std::chrono::milliseconds(0);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(ic_->sonraki_tik - simdi) + std::chrono::milliseconds(1);
}

void Cekirdek::kapaniyor(Zaman simdi)
{
    (void)simdi;
    Ic& ic = *ic_;
    ic.kapaniyor = true;
    for (auto& [id, b] : ic.bag) {
        (void)id;
        if (b->ws) {
            ic.ws_kapat(*b, 1001, "sunucu kapaniyor");  // A3-K49
        } else if (!b->istek_var && b->sira.empty()) {
            b->kapanacak = true;
            b->okuma_kapali = true;
        }
    }
}

SunucuSayaclari Cekirdek::sayaclar() const
{
    SunucuSayaclari s = ic_->say;
    for (const auto& [id, b] : ic_->bag) {
        (void)id;
        if (b->ws) {
            ++s.ws_baglanti;
        } else {
            ++s.http_baglanti;
        }
    }
    return s;
}

std::size_t Cekirdek::okuma_butcesi(BagId id) const
{
    // Sol #5 (A3-K51): soketten bir seferde okunacak en çok bayt, ayrıştırma aşamasına bağlı.
    // Başlık aşamasında 4096: başlığı tamamlayan okuma, başlık sonundan en çok 4096 bayt gövde
    // getirebilir; sınırı aşan Content-Length'te ardından okuma durur.
    const Baglanti* b = ic_->bul(id);
    if (b == nullptr || !okumali(id)) {
        return 0;
    }
    constexpr std::size_t kParca = 65536;
    if (b->ws) {
        return kParca;
    }
    switch (b->asama) {
    case Asama::govde_uzunluk:
        return static_cast<std::size_t>(std::min<std::uint64_t>(kParca, std::max<std::uint64_t>(b->kalan, 1)));
    case Asama::govde_parcali: {
        const std::size_t tavan = b->govde_siniri + sinir::govde_onizleme;
        return b->okunan_govde >= tavan ? std::size_t{1} : std::min(kParca, tavan - b->okunan_govde);
    }
    default:
        return sinir::govde_onizleme;
    }
}

std::size_t Cekirdek::okunan_govde_bayti(BagId id) const
{
    const Baglanti* b = ic_->bul(id);
    return b == nullptr ? 0 : b->okunan_govde;
}

}  // namespace ida::a3
