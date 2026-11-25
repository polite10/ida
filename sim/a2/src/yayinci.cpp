// yayinci.cpp — bkz. ida/a2/yayinci.hpp ve yayinci_ic.hpp.
//
// Bu dosyadaki bütün Yayinci işlevleri dünya iş parçacığında koşar: döngüleri sabit sınırlıdır
// (≤ 64 gövde, ≤ 192 olay, ≤ 256 bileşen/girdi), bellek ayırmaz, kilit almaz, sistem çağrısı yapmaz.
#include "ida/a2/yayinci.hpp"

#include "takvim.hpp"
#include "yayinci_ic.hpp"

#include <cmath>
#include <cstring>

namespace ida::a2 {

namespace {

using ic::Anlik;
using ic::KayitBasligi;
using ic::KayitTuru;

constexpr std::uint8_t kCalisiyor = static_cast<std::uint8_t>(CalismaDurumu::calisiyor);
constexpr std::uint8_t kDuraklatildi = static_cast<std::uint8_t>(CalismaDurumu::duraklatildi);
constexpr std::uint8_t kDurdu = static_cast<std::uint8_t>(CalismaDurumu::durdu);

template <std::size_t N>
bool hepsi_sonlu(const double (&d)[N]) noexcept
{
    for (double x : d) {
        if (!std::isfinite(x)) {
            return false;
        }
    }
    return true;
}

bool sonlu3(const std::array<double, 3>& d) noexcept
{
    return std::isfinite(d[0]) && std::isfinite(d[1]) && std::isfinite(d[2]);
}

// §3.4: |‖q‖ − 1| ≤ 1e-6; toplam binary64'te w, x, y, z sırasıyla.
bool birim4(const double (&q)[4]) noexcept
{
    if (!hepsi_sonlu(q)) {
        return false;
    }
    const double n2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    return std::fabs(std::sqrt(n2) - 1.0) <= kKuaterniyonNormToleransi;
}

bool birim3(const double (&v)[3]) noexcept
{
    if (!hepsi_sonlu(v)) {
        return false;
    }
    return std::fabs(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) - 1.0) <= 1e-6;
}

// §3.4 `Ortam` geçerlilik aralıkları (geçersiz ortam görselleştiricide protokol hatasıdır).
bool ortam_gecerli(const Ortam& o) noexcept
{
    if (o.ortam_turu < 1 || o.ortam_turu > 2) {
        return false;
    }
    if (!hepsi_sonlu(o.statik_p_n) || !birim4(o.statik_q_nb) || !birim3(o.gunes_yonu_n)) {
        return false;
    }
    if (!std::isfinite(o.gunes_isinimi_w_m2) || o.gunes_isinimi_w_m2 < 0.0) {
        return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        const double isik = o.ortam_isigi_rgb_dogrusal[i];
        const double bd = o.beta_d_m[i];
        const double bb = o.beta_b_m[i];
        const double bs = o.b_sonsuz_rgb_dogrusal[i];
        if (!std::isfinite(isik) || isik < 0.0 || !std::isfinite(bd) || bd < 0.0 || !std::isfinite(bb) || bb < 0.0 ||
            !std::isfinite(bs) || bs < 0.0 || bs > 1.0) {
            return false;
        }
    }
    return std::isfinite(o.bulaniklik_olcegi) && o.bulaniklik_olcegi >= 0.0;
}

bool dalga_gecerli(const DalgaGirdisi& d) noexcept
{
    if (d.bilesenler.size() > kAzamiDalgaBileseni) {
        return false;
    }
    if (!std::isfinite(d.g) || !(d.g > 0.0) || !std::isfinite(d.rho) || !(d.rho > 0.0)) {
        return false;
    }
    if (!d.derin_su && (!std::isfinite(d.derinlik_m) || !(d.derinlik_m > 0.0))) {
        return false;
    }
    for (const DalgaBileseni& b : d.bilesenler) {
        if (!std::isfinite(b.a) || b.a < 0.0 || !std::isfinite(b.omega) || !(b.omega > 0.0) || !std::isfinite(b.k) ||
            !(b.k > 0.0) || !std::isfinite(b.beta) || !std::isfinite(b.phi)) {
            return false;
        }
    }
    return true;
}

bool birim_aralik(double x) noexcept { return std::isfinite(x) && x >= 0.0 && x <= 1.0; }

bool malzeme_gecerli(std::span<const MalzemeOptikGirdisi> g) noexcept
{
    if (g.size() > kAzamiMalzemeOptik) {
        return false;
    }
    for (const MalzemeOptikGirdisi& m : g) {
        if ((m.maske & 0xFFF8u) != 0) {
            return false;
        }
        if ((m.maske & 0x1u) != 0) {
            for (double c : m.taban_rengi_dogrusal) {
                if (!birim_aralik(c)) {
                    return false;
                }
            }
        }
        if ((m.maske & 0x2u) != 0 && !birim_aralik(m.metaliklik)) {
            return false;
        }
        if ((m.maske & 0x4u) != 0 && !birim_aralik(m.puruzluluk)) {
            return false;
        }
    }
    return true;
}

bool govde_kaydi_gecerli(const GovdeKaydi& g) noexcept
{
    const auto t = static_cast<std::uint8_t>(g.tur);
    return t >= 1 && t <= 3;
}

void govde_durumu_doldur(GovdeDurumu& h, const Yayinci::Ic::Govde& g, const GovdeAni& a) noexcept
{
    h = GovdeDurumu{};
    h.govde_kimligi = g.kimlik;
    h.sureklilik_no = g.sureklilik;
    h.govde_turu = g.tur;
    h.dolu_tupler = g.tupler;
    h.bayraklar = a.gecerli ? kGovdeGecerli : std::uint16_t{0};
    for (std::size_t i = 0; i < 3; ++i) {
        h.p_n[i] = a.p_n[i];
        h.v_n[i] = a.v_n[i];
        h.w_b[i] = a.w_b[i];
    }
    for (std::size_t i = 0; i < 4; ++i) {
        h.q_nb[i] = a.q_nb[i];
    }
}

// Ne yayınlanacağı (kesit her zaman yayınlanır; kayıtlar aşağıdaki sırayla yazılır).
struct Plan {
    bool onceki_durum = false;        // 1. T − h: önceki kesitin durumu, korunan (A2-K21)
                                      // 2. bekleyen küme değişimleri
    int durum_once = 0;               // 3. durum (0 yok, 1 takvimli/atılabilir, 2 korunan)
    std::uint8_t durum_once_cd = kCalisiyor;
    bool olaylar = false;             // 4. bu sınırın bekleyen olayları
    const SahneOlayi* ek_olay = nullptr;  // 5. tek olay (duraklat, devam, sifirlama)
    int durum_sonra = 0;              // 6. durum
    std::uint8_t durum_sonra_cd = kCalisiyor;
    KayitTuru ek = KayitTuru::olay;   // 7. yeni_calistirma ya da kapanis (olay = yok)
};

struct Yazar {
    Yayinci::Ic& y;

    void kayip(std::uint64_t s) noexcept
    {
        y.kayip_korunan.fetch_add(1, std::memory_order_relaxed);
        y.kayip_sira.store(s, std::memory_order_release);
    }

    // Başlık + yük için yer ayırır; yer yoksa nullptr (korunan kayıtta kayıp işaretlenir).
    std::uint8_t* ac(KayitTuru tur, std::uint16_t bayrak, std::size_t yuk, bool atilabilir) noexcept
    {
        const std::uint64_t s = y.sira++;
        std::uint8_t* p = y.halka.ayir(sizeof(KayitBasligi) + yuk, atilabilir ? y.pay : 0);
        if (p == nullptr) {
            if (atilabilir) {
                y.dusen_durum.fetch_add(1, std::memory_order_relaxed);
            } else {
                kayip(s);
            }
            return nullptr;
        }
        KayitBasligi kb{};
        kb.sira = s;
        kb.tur = static_cast<std::uint16_t>(tur);
        kb.bayrak = bayrak;
        std::memcpy(p, &kb, sizeof kb);
        return p + sizeof kb;
    }

    void kanca() noexcept
    {
        if (y.sinama_kancasi != nullptr) {
            y.sinama_kancasi(y.sinama_baglami);
        }
    }

    void kapat() noexcept
    {
        y.halka.yayinla();
        y.yazilan_kayit.fetch_add(1, std::memory_order_relaxed);
        kanca();
    }

    void durum(const Anlik& k, std::uint8_t cd, bool korunan) noexcept
    {
        const std::size_t n = k.govde_sayisi;
        std::uint8_t* p = ac(KayitTuru::durum, korunan ? ic::kKayitKorunan : std::uint16_t{0},
                             sizeof(Durum) + n * sizeof(GovdeDurumu), !korunan);
        if (p == nullptr) {
            return;
        }
        Durum d{};
        d.t_ns = static_cast<std::int64_t>(k.adim_no) * kAdimNs;
        d.adim_no = k.adim_no;
        d.gonderim_duvar_ns = 0;  // G/Ç iş parçacığı kuyruğa koyarken doldurur (A2-K08)
        d.dalga_kumesi_surumu = k.dalga.surum;
        d.govde_sayisi = k.govde_sayisi;
        d.calisma_durumu = cd;
        d.bayraklar = ic::takvimde(k.adim_no) ? std::uint8_t{0} : kDurumTakvimDisi;
        std::memcpy(p, &d, sizeof d);
        if (n > 0) {
            std::memcpy(p + sizeof d, k.govde.data(), n * sizeof(GovdeDurumu));
        }
        kapat();
    }

    void olay(const SahneOlayi& o) noexcept
    {
        std::uint8_t* p = ac(KayitTuru::olay, 0, sizeof o, false);
        if (p == nullptr) {
            return;
        }
        std::memcpy(p, &o, sizeof o);
        kapat();
    }

    void kumeler() noexcept
    {
        if (y.dalga_bekliyor) {
            y.dalga_bekliyor = false;
            const std::size_t m = y.dalga.bilesen_sayisi;
            std::uint8_t* p = ac(KayitTuru::dalga, 0, sizeof(DalgaKumesi) + m * sizeof(DalgaBileseni), false);
            if (p != nullptr) {
                std::memcpy(p, &y.dalga, sizeof y.dalga);
                if (m > 0) {
                    std::memcpy(p + sizeof y.dalga, y.bilesen.data(), m * sizeof(DalgaBileseni));
                }
                kapat();
            }
        }
        if (y.ortam_bekliyor) {
            y.ortam_bekliyor = false;
            std::uint8_t* p = ac(KayitTuru::ortam, 0, sizeof(Ortam), false);
            if (p != nullptr) {
                std::memcpy(p, &y.ortam, sizeof y.ortam);
                kapat();
            }
        }
        if (y.malzeme_bekliyor) {
            y.malzeme_bekliyor = false;
            const std::size_t k = y.malzeme.girdi_sayisi;
            std::uint8_t* p = ac(KayitTuru::malzeme, 0, sizeof(MalzemeOptik) + k * sizeof(MalzemeOptikGirdisi), false);
            if (p != nullptr) {
                std::memcpy(p, &y.malzeme, sizeof y.malzeme);
                if (k > 0) {
                    std::memcpy(p + sizeof y.malzeme, y.girdi.data(), k * sizeof(MalzemeOptikGirdisi));
                }
                kapat();
            }
        }
    }

    // Yuvanın gövde dışındaki bütün alanlarını güncel değerlerle doldurur. Büyük kümeler yalnız
    // yuvadaki etiket bayatsa kopyalanır (olağan adımda kopyalanmaz).
    void ortak(Anlik& s) noexcept
    {
        s.tanimli = y.calistirma_var;
        s.calisma_durumu = y.calisma_durumu;
        s.nesil = y.nesil;
        s.adim_no = y.adim_no;
        if (s.tanim_surumu != y.tanim_surumu) {
            s.tanim_surumu = y.tanim_surumu;
            s.sim_kipi = y.sim_kipi;
            s.kademe_3b = y.kademe_3b;
            std::memcpy(s.manifest_sha256, y.manifest_sha256, sizeof s.manifest_sha256);
            std::memcpy(s.manifest_yolu, y.manifest_yolu, sizeof s.manifest_yolu);
        }
        if (s.ortam.ortam_surumu != y.ortam.ortam_surumu) {
            s.ortam = y.ortam;
        }
        if (s.dalga.surum != y.dalga.surum) {
            s.dalga = y.dalga;
            const std::size_t m = y.dalga.bilesen_sayisi;
            if (m > 0) {
                std::memcpy(s.bilesen.data(), y.bilesen.data(), m * sizeof(DalgaBileseni));
            }
        }
        if (s.malzeme.surum != y.malzeme.surum) {
            s.malzeme = y.malzeme;
            const std::size_t k = y.malzeme.girdi_sayisi;
            if (k > 0) {
                std::memcpy(s.girdi.data(), y.girdi.data(), k * sizeof(MalzemeOptikGirdisi));
            }
        }
    }

    // Gövdeleri yayındaki kesitten aynen alır (gövde durumu değişmeyen yayınlar için).
    void govdeleri_kopyala(Anlik& s, const Anlik& e) noexcept
    {
        const std::size_t n = e.govde_sayisi;
        s.govde_sayisi = e.govde_sayisi;
        if (n > 0) {
            std::memcpy(s.govde.data(), e.govde.data(), n * sizeof(GovdeDurumu));
            std::memcpy(s.varlik.data(), e.varlik.data(), n * sizeof(std::uint16_t));
        }
    }

    // Kesiti yayına alır, sonra kayıtları yazar (değişmez: yayinci_ic.hpp başı).
    void yayinla(Anlik& s, const Plan& p) noexcept
    {
        // Bu partinin kullanabileceği en büyük sıra: sıra numaraları yoğun olmak zorunda değildir.
        s.sira = y.sira + y.bekleyen_n + 16u;
        const Anlik& eski = y.anlik.yayindaki();  // yeni yuva yayına girince de bir sonraki yayına kadar değişmez
        y.anlik.yayinla();
        kanca();
        if (y.bekleyen_tasti) {
            y.bekleyen_tasti = false;
            kayip(s.sira);  // bu sınırın bir olayı tutulamadı: bağlı oturum yeniden eşitlenmeli
        }
        if (p.onceki_durum) {
            durum(eski, eski.calisma_durumu, true);
        }
        kumeler();
        if (p.durum_once != 0) {
            durum(s, p.durum_once_cd, p.durum_once == 2);
        }
        if (p.olaylar) {
            for (std::size_t i = 0; i < y.bekleyen_n; ++i) {
                olay(y.bekleyen[i]);
            }
            y.bekleyen_n = 0;
        }
        if (p.ek_olay != nullptr) {
            olay(*p.ek_olay);
        }
        if (p.durum_sonra != 0) {
            durum(s, p.durum_sonra_cd, p.durum_sonra == 2);
        }
        if (p.ek != KayitTuru::olay) {
            if (ac(p.ek, 0, 0, false) != nullptr) {
                kapat();
            }
        }
        y.sira = s.sira + 1u;
    }

    // Gövde durumu değişmeyen yayın: yeni kesit = güncel başlık + yayındaki gövdeler.
    void durumsuz_yayinla(const Plan& p) noexcept
    {
        Anlik& s = y.anlik.yazilacak();
        const Anlik& e = y.anlik.yayindaki();
        ortak(s);
        govdeleri_kopyala(s, e);
        yayinla(s, p);
    }

    bool reddet() noexcept
    {
        y.gecersiz_cagri.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    int bul(std::uint32_t kimlik) const noexcept
    {
        for (std::size_t i = 0; i < y.n; ++i) {
            if (y.tablo[i].kimlik == kimlik) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // Olay çağrılarının ortak ön koşulu.
    bool olay_kabul(std::uint64_t adim_no) noexcept
    {
        if (!y.etkin) {
            return false;
        }
        if (!y.calistirma_var || adim_no < y.asgari_adim) {
            return reddet();
        }
        return true;
    }

    SahneOlayi olay_basi(std::uint64_t adim_no, SahneOlayiTuru tur) const noexcept
    {
        SahneOlayi o{};
        o.t_ns = static_cast<std::int64_t>(adim_no) * kAdimNs;
        o.adim_no = adim_no;
        o.tur = static_cast<std::uint16_t>(tur);
        o.calisma_nesli = y.nesil;
        return o;
    }

    void beklet(const SahneOlayi& o, std::uint64_t adim_no) noexcept
    {
        if (adim_no > y.bekleyen_azami_adim) {
            y.bekleyen_azami_adim = adim_no;
        }
        if (y.bekleyen_n >= y.bekleyen.size()) {
            y.bekleyen_tasti = true;
            y.olay_tasmasi.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        y.bekleyen[y.bekleyen_n++] = o;
    }

    void dalga_al(const DalgaGirdisi& d) noexcept
    {
        const std::uint32_t surum = y.dalga.surum + 1u;
        y.dalga = DalgaKumesi{};
        y.dalga.surum = surum;
        y.dalga.bilesen_sayisi = static_cast<std::uint16_t>(d.bilesenler.size());
        y.dalga.derin_su = d.derin_su ? std::uint8_t{1} : std::uint8_t{0};
        y.dalga.tohum = d.tohum;
        y.dalga.g = d.g;
        y.dalga.rho = d.rho;
        // derin_su = 1 iken alan yok sayılır (§3.4); tel üzerinde yine sonlu ve pozitif kalsın.
        y.dalga.derinlik_m = (std::isfinite(d.derinlik_m) && d.derinlik_m > 0.0) ? d.derinlik_m : 1.0;
        for (std::size_t i = 0; i < d.bilesenler.size(); ++i) {
            y.bilesen[i] = d.bilesenler[i];
        }
    }

    void ortam_al(const Ortam& o) noexcept
    {
        const std::uint32_t surum = y.ortam.ortam_surumu + 1u;
        y.ortam = o;
        y.ortam.ortam_surumu = surum;
        y.ortam.dolgu0 = 0;
        y.ortam.dolgu1 = 0;
    }

    void malzeme_al(std::span<const MalzemeOptikGirdisi> g) noexcept
    {
        const std::uint32_t surum = y.malzeme.surum + 1u;
        y.malzeme = MalzemeOptik{};
        y.malzeme.surum = surum;
        y.malzeme.girdi_sayisi = static_cast<std::uint16_t>(g.size());
        for (std::size_t i = 0; i < g.size(); ++i) {
            // Maskede olmayan alanlar kullanılmaz (§3.4); tel üzerinde sıfır yazılır.
            MalzemeOptikGirdisi m{};
            m.malzeme_indeksi = g[i].malzeme_indeksi;
            m.maske = g[i].maske;
            if ((m.maske & 0x1u) != 0) {
                for (std::size_t c = 0; c < 4; ++c) {
                    m.taban_rengi_dogrusal[c] = g[i].taban_rengi_dogrusal[c];
                }
            }
            if ((m.maske & 0x2u) != 0) {
                m.metaliklik = g[i].metaliklik;
            }
            if ((m.maske & 0x4u) != 0) {
                m.puruzluluk = g[i].puruzluluk;
            }
            y.girdi[i] = m;
        }
    }
};

}  // namespace

namespace {

// Yayın halkası: en az 256 KiB (en büyük kayıt ≈ 14,4 KiB ve ayrılmış pay sığsın), en çok 256 MiB.
std::size_t halka_boyu(std::size_t istenen, bool etkin) noexcept
{
    if (!etkin) {
        return 4096;  // A2'siz: hiçbir şey yazılmaz
    }
    const std::size_t asgari = std::size_t{256} << 10u;
    const std::size_t azami = std::size_t{256} << 20u;
    return istenen < asgari ? asgari : (istenen > azami ? azami : istenen);
}

}  // namespace

Yayinci::Ic::Ic(std::size_t halka_bayt, bool etkin_) : etkin(etkin_), halka(halka_boyu(halka_bayt, etkin_))
{
    const std::size_t ceyrek = halka.kapasite() / 4;
    pay = ceyrek < (std::size_t{1} << 20u) ? ceyrek : (std::size_t{1} << 20u);
}

Yayinci::Yayinci(std::size_t halka_bayt, bool etkin) : ic_(std::make_unique<Ic>(halka_bayt, etkin)) {}
Yayinci::~Yayinci() = default;

std::uint32_t Yayinci::calisma_nesli() const noexcept { return ic_->nesil_a.load(std::memory_order_relaxed); }

YayinSayaclari Yayinci::sayaclar() const noexcept
{
    YayinSayaclari s;
    s.yazilan_kayit = ic_->yazilan_kayit.load(std::memory_order_relaxed);
    s.dusen_durum = ic_->dusen_durum.load(std::memory_order_relaxed);
    s.kayip_korunan = ic_->kayip_korunan.load(std::memory_order_relaxed);
    s.olay_tasmasi = ic_->olay_tasmasi.load(std::memory_order_relaxed);
    s.gecersiz_cagri = ic_->gecersiz_cagri.load(std::memory_order_relaxed);
    s.eksik_govde = ic_->eksik_govde.load(std::memory_order_relaxed);
    return s;
}

bool Yayinci::yeni_calistirma(const CalistirmaTanimi& t) noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    // Önce bütün girdi denetlenir; geçersizse hiçbir şey değişmez.
    const auto kip = static_cast<std::uint8_t>(t.sim_kipi);
    const auto kademe = static_cast<std::uint8_t>(t.kademe_3b);
    const auto cd = static_cast<std::uint8_t>(t.calisma_durumu);
    if (kip < 1 || kip > 4 || kademe < 1 || kademe > 2 || cd < 1 || cd > 3) {
        return w.reddet();
    }
    if (t.manifest_yolu.empty() || t.manifest_yolu.size() >= sizeof y.manifest_yolu ||
        t.manifest_yolu.find('\0') != std::string_view::npos) {
        return w.reddet();
    }
    if (!dalga_gecerli(t.dalga) || !ortam_gecerli(t.ortam) || !malzeme_gecerli(t.malzeme)) {
        return w.reddet();
    }
    if (t.govdeler.size() > kAzamiGovde || t.govdeler.size() != t.anlar.size()) {
        return w.reddet();
    }
    for (std::size_t i = 0; i < t.govdeler.size(); ++i) {
        if (!govde_kaydi_gecerli(t.govdeler[i]) || t.anlar[i].kimlik != t.govdeler[i].kimlik) {
            return w.reddet();
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (t.govdeler[j].kimlik == t.govdeler[i].kimlik) {
                return w.reddet();
            }
        }
    }

    if (y.calistirma_var) {
        ++y.nesil;  // A2-K80 (1): süreçteki ilk çalıştırma nesil 1'dir, sonrakiler 1 artırır
    }
    y.nesil_a.store(y.nesil, std::memory_order_relaxed);
    y.calistirma_var = true;
    y.calisma_durumu = cd;
    y.adim_no = t.adim_no;
    y.adim_var = true;
    y.asgari_adim = t.adim_no + 1u;
    y.onceki_gecerli = true;
    ++y.tanim_surumu;
    y.sim_kipi = kip;
    y.kademe_3b = kademe;
    std::memcpy(y.manifest_sha256, t.manifest_sha256.data(), sizeof y.manifest_sha256);
    std::memset(y.manifest_yolu, 0, sizeof y.manifest_yolu);
    std::memcpy(y.manifest_yolu, t.manifest_yolu.data(), t.manifest_yolu.size());
    w.dalga_al(t.dalga);
    w.ortam_al(t.ortam);
    w.malzeme_al(t.malzeme);
    // Yeni oturum kümeleri kesitten alır; eski oturum kapanır: değişim kaydı gerekmez.
    y.dalga_bekliyor = false;
    y.ortam_bekliyor = false;
    y.malzeme_bekliyor = false;
    y.bekleyen_n = 0;
    y.bekleyen_tasti = false;
    y.bekleyen_azami_adim = 0;
    y.sinir = false;

    Anlik& s = y.anlik.yazilacak();
    y.n = static_cast<std::uint16_t>(t.govdeler.size());
    for (std::size_t i = 0; i < t.govdeler.size(); ++i) {
        const GovdeKaydi& g = t.govdeler[i];
        Ic::Govde& h = y.tablo[i];
        h = Ic::Govde{};
        h.kimlik = g.kimlik;
        h.varlik = g.varlik_indeksi;
        h.tur = static_cast<std::uint8_t>(g.tur);
        h.tupler = g.tur == GovdeTuru::arac ? g.dolu_tupler : std::uint8_t{0};
        govde_durumu_doldur(s.govde[i], h, t.anlar[i]);
        s.varlik[i] = h.varlik;
    }
    s.govde_sayisi = y.n;
    w.ortak(s);
    Plan p;
    p.ek = KayitTuru::yeni_calistirma;
    w.yayinla(s, p);
    return true;
}

bool Yayinci::calistirma_durdu() noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || y.calisma_durumu == kDurdu) {
        return w.reddet();
    }
    y.calisma_durumu = kDurdu;
    Plan p;
    if (y.adim_var) {
        p.durum_sonra = 2;  // son durum, "durdu" ile
        p.durum_sonra_cd = kDurdu;
    }
    w.durumsuz_yayinla(p);
    return true;
}

bool Yayinci::govde_dogdu(std::uint64_t adim_no, const GovdeKaydi& g, std::uint8_t kaynak_tup) noexcept
{
    Ic& y = *ic_;
    Yazar w{y};
    if (!w.olay_kabul(adim_no)) {
        return false;
    }
    if (!govde_kaydi_gecerli(g) || kaynak_tup > 2 || y.n >= kAzamiGovde || w.bul(g.kimlik) >= 0) {
        return w.reddet();
    }
    Ic::Govde& h = y.tablo[y.n++];
    h = Ic::Govde{};
    h.kimlik = g.kimlik;
    h.varlik = g.varlik_indeksi;
    h.tur = static_cast<std::uint8_t>(g.tur);
    h.tupler = g.tur == GovdeTuru::arac ? g.dolu_tupler : std::uint8_t{0};
    h.kirildi = true;  // ilk numara 0'dır; aynı sınırdaki başka bir neden onu artırmaz (A2-K21)
    SahneOlayi o = w.olay_basi(adim_no, SahneOlayiTuru::govde_dogdu);
    o.govde_kimligi = h.kimlik;
    o.varlik_indeksi = h.varlik;
    o.govde_turu = h.tur;
    o.sureklilik_no = 0;
    o.kaynak_tup = kaynak_tup;
    w.beklet(o, adim_no);
    y.sinir = true;
    return true;
}

bool Yayinci::govde_oldu(std::uint64_t adim_no, std::uint32_t kimlik, OlumNedeni neden) noexcept
{
    Ic& y = *ic_;
    Yazar w{y};
    if (!w.olay_kabul(adim_no)) {
        return false;
    }
    const int i = w.bul(kimlik);
    const auto nd = static_cast<std::uint8_t>(neden);
    if (i < 0 || nd < 1 || nd > 4) {
        return w.reddet();
    }
    for (std::size_t j = static_cast<std::size_t>(i); j + 1 < y.n; ++j) {
        y.tablo[j] = y.tablo[j + 1];
    }
    --y.n;
    SahneOlayi o = w.olay_basi(adim_no, SahneOlayiTuru::govde_oldu);
    o.govde_kimligi = kimlik;
    o.neden = nd;
    w.beklet(o, adim_no);
    y.sinir = true;
    return true;
}

bool Yayinci::kutle_durumu(std::uint64_t adim_no, std::uint32_t kimlik, std::uint8_t yeni_tupler) noexcept
{
    Ic& y = *ic_;
    Yazar w{y};
    if (!w.olay_kabul(adim_no)) {
        return false;
    }
    const int i = w.bul(kimlik);
    if (i < 0 || y.tablo[static_cast<std::size_t>(i)].tur != static_cast<std::uint8_t>(GovdeTuru::arac)) {
        return w.reddet();
    }
    Ic::Govde& h = y.tablo[static_cast<std::size_t>(i)];
    if (!h.kirildi) {
        ++h.sureklilik;
        h.kirildi = true;
    }
    SahneOlayi o = w.olay_basi(adim_no, SahneOlayiTuru::kutle_durumu);
    o.govde_kimligi = kimlik;
    o.eski_tupler = h.tupler;
    o.yeni_tupler = yeni_tupler;
    o.sureklilik_no = h.sureklilik;
    h.tupler = yeni_tupler;
    w.beklet(o, adim_no);
    y.sinir = true;
    return true;
}

bool Yayinci::sureklilik_kir(std::uint64_t adim_no, std::uint32_t kimlik) noexcept
{
    Ic& y = *ic_;
    Yazar w{y};
    if (!w.olay_kabul(adim_no)) {
        return false;
    }
    const int i = w.bul(kimlik);
    if (i < 0) {
        return w.reddet();
    }
    Ic::Govde& h = y.tablo[static_cast<std::size_t>(i)];
    if (!h.kirildi) {
        ++h.sureklilik;
        h.kirildi = true;
    }
    if (adim_no > y.bekleyen_azami_adim) {
        y.bekleyen_azami_adim = adim_no;
    }
    y.sinir = true;
    return true;
}

bool Yayinci::isabet(std::uint64_t adim_no, std::uint32_t torpido, std::uint32_t hedef,
                     const std::array<double, 3>& nokta_n, const std::array<double, 3>& normal_n,
                     const std::array<double, 3>& bagil_hiz_n) noexcept
{
    Ic& y = *ic_;
    Yazar w{y};
    if (!w.olay_kabul(adim_no)) {
        return false;
    }
    if (!sonlu3(nokta_n) || !sonlu3(normal_n) || !sonlu3(bagil_hiz_n)) {
        return w.reddet();
    }
    SahneOlayi o = w.olay_basi(adim_no, SahneOlayiTuru::isabet);
    o.govde_kimligi = torpido;
    o.diger_govde_kimligi = hedef;
    for (std::size_t i = 0; i < 3; ++i) {
        o.nokta_n[i] = nokta_n[i];
        o.normal_n[i] = normal_n[i];
        o.bagil_hiz_n[i] = bagil_hiz_n[i];
    }
    w.beklet(o, adim_no);
    return true;
}

bool Yayinci::adim_siniri(std::uint64_t adim_no, std::span<const GovdeAni> anlar) noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    // Duraklatılmışken durum gitmez (A2-K24). Tek istisna sıfırlamadan sonraki ilk sınırdır
    // (A2-K29: yaşayan gövdelerin doğumu ve durumu yeni sim zamanında bildirilir).
    const bool sifirlama_sonrasi = !y.adim_var;
    if (!y.calistirma_var || adim_no < y.asgari_adim || adim_no < y.bekleyen_azami_adim ||
        (y.calisma_durumu != kCalisiyor && !sifirlama_sonrasi) || y.calisma_durumu == kDurdu) {
        return w.reddet();
    }
    Anlik& s = y.anlik.yazilacak();
    const Anlik& e = y.anlik.yayindaki();
    const bool onceki = y.sinir && y.onceki_gecerli && e.adim_no + 1u == adim_no;
    y.adim_no = adim_no;
    w.ortak(s);

    // Tablodaki her gövdenin durumu: önce aynı indekse bakılır (olağan durum), sonra aranır.
    std::size_t eslesen = 0;
    for (std::size_t i = 0; i < y.n; ++i) {
        Ic::Govde& g = y.tablo[i];
        const GovdeAni* a = nullptr;
        if (i < anlar.size() && anlar[i].kimlik == g.kimlik) {
            a = &anlar[i];
        } else {
            for (const GovdeAni& x : anlar) {
                if (x.kimlik == g.kimlik) {
                    a = &x;
                    break;
                }
            }
        }
        if (a != nullptr) {
            govde_durumu_doldur(s.govde[i], g, *a);
            ++eslesen;
        } else {
            // Durumu verilmedi: son bilinen pozla, geçersiz işaretli (A2-K27).
            GovdeAni bos;
            bos.kimlik = g.kimlik;
            bos.gecerli = false;
            for (std::size_t j = 0; j < e.govde_sayisi; ++j) {
                if (e.govde[j].govde_kimligi == g.kimlik) {
                    for (std::size_t c = 0; c < 3; ++c) {
                        bos.p_n[c] = e.govde[j].p_n[c];
                    }
                    for (std::size_t c = 0; c < 4; ++c) {
                        bos.q_nb[c] = e.govde[j].q_nb[c];
                    }
                    break;
                }
            }
            govde_durumu_doldur(s.govde[i], g, bos);
            y.eksik_govde.fetch_add(1, std::memory_order_relaxed);
        }
        s.varlik[i] = g.varlik;
        g.kirildi = false;
    }
    s.govde_sayisi = y.n;
    if (eslesen != anlar.size()) {
        y.gecersiz_cagri.fetch_add(1, std::memory_order_relaxed);  // tabloda olmayan gövdenin durumu verildi
    }

    Plan p;
    p.onceki_durum = onceki;
    p.olaylar = true;
    p.durum_sonra_cd = y.calisma_durumu;
    p.durum_sonra = y.sinir ? 2 : (ic::takvimde(adim_no) ? 1 : 0);
    w.yayinla(s, p);

    y.sinir = false;
    y.adim_var = true;
    y.asgari_adim = adim_no + 1u;
    y.bekleyen_azami_adim = 0;
    y.onceki_gecerli = true;
    return true;
}

bool Yayinci::dalga_kumesi(const DalgaGirdisi& d) noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || !dalga_gecerli(d)) {
        return w.reddet();
    }
    w.dalga_al(d);
    y.dalga_bekliyor = true;
    if (y.calisma_durumu != kCalisiyor) {
        w.durumsuz_yayinla(Plan{});
    }
    return true;
}

bool Yayinci::ortam(const Ortam& o) noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || !ortam_gecerli(o)) {
        return w.reddet();
    }
    w.ortam_al(o);
    y.ortam_bekliyor = true;
    if (y.calisma_durumu != kCalisiyor) {
        w.durumsuz_yayinla(Plan{});
    }
    return true;
}

bool Yayinci::malzeme_optik(std::span<const MalzemeOptikGirdisi> girdiler) noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || !malzeme_gecerli(girdiler)) {
        return w.reddet();
    }
    w.malzeme_al(girdiler);
    y.malzeme_bekliyor = true;
    if (y.calisma_durumu != kCalisiyor) {
        w.durumsuz_yayinla(Plan{});
    }
    return true;
}

bool Yayinci::duraklat() noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || y.calisma_durumu != kCalisiyor) {
        return w.reddet();
    }
    y.calisma_durumu = kDuraklatildi;
    const SahneOlayi o = w.olay_basi(y.adim_no, SahneOlayiTuru::duraklat);
    Plan p;
    if (y.adim_var) {
        // Duraklatma sınırının durumu olaydan önce gider; olaydan sonra durum yoktur (A2-K24).
        p.durum_once = 2;
        p.durum_once_cd = kCalisiyor;
    }
    p.ek_olay = &o;
    w.durumsuz_yayinla(p);
    return true;
}

bool Yayinci::surdur() noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || y.calisma_durumu != kDuraklatildi) {
        return w.reddet();
    }
    y.calisma_durumu = kCalisiyor;
    const SahneOlayi o = w.olay_basi(y.adim_no, SahneOlayiTuru::devam);
    Plan p;
    p.ek_olay = &o;
    w.durumsuz_yayinla(p);
    return true;
}

bool Yayinci::sifirla(std::uint64_t yeni_adim_no) noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    if (!y.calistirma_var || y.calisma_durumu == kDurdu) {
        return w.reddet();
    }
    ++y.nesil;  // A2-K80 (2)
    y.nesil_a.store(y.nesil, std::memory_order_relaxed);
    y.n = 0;    // A2-K29: bütün gövdeler ölmüş sayılır
    y.bekleyen_n = 0;  // eski zaman çizgisinin bekleyen olayları geçersiz
    y.bekleyen_azami_adim = 0;
    y.adim_no = yeni_adim_no;
    y.adim_var = false;
    y.asgari_adim = yeni_adim_no;
    y.onceki_gecerli = false;
    y.sinir = true;  // sıfırlamadan sonraki ilk durum korunur (takvimde olmasa da gider)
    const SahneOlayi o = w.olay_basi(yeni_adim_no, SahneOlayiTuru::sifirlama);
    Anlik& s = y.anlik.yazilacak();
    w.ortak(s);
    s.govde_sayisi = 0;
    Plan p;
    p.ek_olay = &o;
    w.yayinla(s, p);
    return true;
}

bool Yayinci::kapaniyor() noexcept
{
    Ic& y = *ic_;
    if (!y.etkin) {
        return false;
    }
    Yazar w{y};
    Plan p;
    p.ek = KayitTuru::kapanis;
    w.durumsuz_yayinla(p);
    return true;
}

}  // namespace ida::a2
