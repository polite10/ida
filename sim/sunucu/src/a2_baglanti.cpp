// A2 sunucusunun `ida_sim`'e bağlanması (a2_baglanti.hpp).
#include "a2_baglanti.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

namespace ida::sunucu {

namespace a2b {

fizik::Vek3 govdeden_ned(const fizik::Kuaterniyon& q_nb, const fizik::Vek3& v_b) noexcept
{
    // Birim kuaterniyonun dönüş matrisi (Hamilton; gövdeden NED'e): v_n = q ⊗ (0, v_b) ⊗ q*.
    // Satırlar NED eksenleri (N, E, D), sütunlar gövde eksenleri (x ileri, y sancak, z aşağı).
    const double w = q_nb[0], x = q_nb[1], y = q_nb[2], z = q_nb[3];
    return {(1.0 - 2.0 * (y * y + z * z)) * v_b[0] + 2.0 * (x * y - w * z) * v_b[1] + 2.0 * (x * z + w * y) * v_b[2],
            2.0 * (x * y + w * z) * v_b[0] + (1.0 - 2.0 * (x * x + z * z)) * v_b[1] + 2.0 * (y * z - w * x) * v_b[2],
            2.0 * (x * z - w * y) * v_b[0] + 2.0 * (y * z + w * x) * v_b[1] + (1.0 - 2.0 * (x * x + y * y)) * v_b[2]};
}

a2::GovdeAni govde_ani(std::uint32_t kimlik, const fizik::DurumAnlik& d) noexcept
{
    a2::GovdeAni a;
    a.kimlik = kimlik;
    a.gecerli = true;
    a.p_n = d.konum_n;                                        // O_b, NED, m
    a.q_nb = d.yonelim;                                       // gövdeden NED'e (w, x, y, z)
    a.v_n = govdeden_ned(d.yonelim, d.dogrusal_hiz_b);        // v_n = R(q_nb) · v_b
    a.w_b = d.acisal_hiz_b;                                   // gövde çerçevesinde, rad/s
    return a;
}

std::uint32_t yeni_govde_kimligi() noexcept
{
    static std::atomic<std::uint32_t> sayac{0};
    return sayac.fetch_add(1u, std::memory_order_relaxed) + 1u;
}

std::uint8_t dolu_tup_maskesi(const std::array<bool, 2>& dolu) noexcept
{
    return static_cast<std::uint8_t>((dolu[0] ? 1u : 0u) | (dolu[1] ? 2u : 0u));
}

namespace {

using senaryo::Deger;

bool sayi_oku(const Deger& c, std::string_view yol, double& x)
{
    const Deger* d = c.yol_bul(yol);
    if (d == nullptr || !d->sayisal_mi()) {
        return false;
    }
    x = d->cift();
    return std::isfinite(x);
}

template <std::size_t N>
bool dizi_oku(const Deger* d, double (&cikti)[N])
{
    if (d == nullptr || !d->dizi_mi() || d->dizi.size() != N) {
        return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
        if (!d->dizi[i].sayisal_mi() || !std::isfinite(d->dizi[i].cift())) {
            return false;
        }
        cikti[i] = d->dizi[i].cift();
    }
    return true;
}

// Sıfırdan farklı sonlu vektörü birim boya getirir (A5 "birim" der; A2 §3.4 1e-6 ister).
template <std::size_t N>
bool birimle(double (&v)[N])
{
    double n2 = 0.0;
    for (double x : v) {
        n2 += x * x;
    }
    const double n = std::sqrt(n2);
    if (!std::isfinite(n) || !(n > 0.0)) {
        return false;
    }
    for (double& x : v) {
        x /= n;
    }
    return true;
}

int onaltilik(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool ozet_coz(std::string_view hex, std::array<std::uint8_t, 32>& cikti)
{
    if (hex.size() != 64) {
        return false;
    }
    for (std::size_t i = 0; i < 32; ++i) {
        const int a = onaltilik(hex[2 * i]);
        const int b = onaltilik(hex[2 * i + 1]);
        if (a < 0 || b < 0) {
            return false;
        }
        cikti[i] = static_cast<std::uint8_t>((a << 4) | b);
    }
    return true;
}

// `dizi_adi` dizisinde `kimlik` alanı `kimlik` olan ilk öğenin indeksi; yoksa −1.
int kimlik_indeksi(const nlohmann::json& manifest, const char* dizi_adi, const std::string& kimlik)
{
    const auto it = manifest.find(dizi_adi);
    if (it == manifest.end() || !it->is_array()) {
        return -1;
    }
    int i = 0;
    for (const nlohmann::json& o : *it) {
        const auto k = o.is_object() ? o.find("kimlik") : o.end();
        if (o.is_object() && k != o.end() && k->is_string() && k->get_ref<const std::string&>() == kimlik) {
            return i;
        }
        ++i;
    }
    return -1;
}

}  // namespace

TanimSonucu tanim_kur(const senaryo::DogrulanmisSenaryo& cozumlenmis, const senaryo::DogrulamaRaporu& rapor,
                      const senaryo::DosyaKaynagi& kaynak, const std::string& depo_koku, Tanim& tanim, std::string& hata)
{
    const Deger& c = cozumlenmis.agac();
    tanim = Tanim{};

    // ---- Kademe (A5 §3.12 kalite.gorunum; A2 §6) ----
    const Deger* gorunum = c.yol_bul("kalite.gorunum");
    if (gorunum == nullptr || !gorunum->dizgi_mi()) {
        hata = "kalite.gorunum okunamadı";
        return TanimSonucu::hata;
    }
    if (gorunum->dizgi == "kapali") {
        return TanimSonucu::gorunum_kapali;
    }
    if (gorunum->dizgi == "tam") {
        tanim.kademe = a2::Kademe::tam;
    } else if (gorunum->dizgi == "sade") {
        tanim.kademe = a2::Kademe::sade;
    } else {
        hata = "kalite.gorunum bilinmeyen değer: " + gorunum->dizgi;
        return TanimSonucu::hata;
    }

    // ---- A7 manifesti: yol ve özet (A5-K115, A7-K05) ----
    const Deger* kume = c.yol_bul("varliklar.kume");
    if (kume == nullptr || !kume->dizgi_mi() || kume->dizgi.empty()) {
        hata = "varliklar.kume okunamadı";
        return TanimSonucu::hata;
    }
    const std::string yol = "assets/" + kume->dizgi + "/manifest.json";
    const std::optional<std::string> bayt = kaynak.oku(yol);
    if (!bayt) {
        hata = "A7 manifesti okunamadı: " + yol;
        return TanimSonucu::hata;
    }
    const std::string ozet = senaryo::sha256_hex(*bayt);
    const auto ro = rapor.kaynak_ozetleri.find(yol);
    if (ro == rapor.kaynak_ozetleri.end() || ro->second != ozet) {
        hata = "A7 manifesti doğrulama raporundaki özetle aynı değil (A5-K115): " + yol;
        return TanimSonucu::hata;
    }
    if (!ozet_coz(ozet, tanim.manifest_sha256)) {
        hata = "A7 manifest özeti çözülemedi: " + yol;
        return TanimSonucu::hata;
    }
    {
        std::error_code ec;
        const std::filesystem::path mutlak =
            std::filesystem::absolute(std::filesystem::path(depo_koku) / std::filesystem::path(yol), ec);
        if (ec) {
            hata = "A7 manifestinin mutlak yolu kurulamadı: " + yol;
            return TanimSonucu::hata;
        }
        const std::u8string u = mutlak.lexically_normal().u8string();
        tanim.manifest_yolu.assign(u.begin(), u.end());
        if (tanim.manifest_yolu.empty() || tanim.manifest_yolu.size() >= 512u) {   // A2 §3.3: 512 B, NUL ile biter
            hata = "A7 manifestinin mutlak yolu A2 alanına sığmıyor (< 512 bayt olmalı): " + yol;
            return TanimSonucu::hata;
        }
    }
    const nlohmann::json manifest = nlohmann::json::parse(*bayt, nullptr, false);
    if (manifest.is_discarded() || !manifest.is_object()) {
        hata = "A7 manifesti ayrıştırılamadı: " + yol;
        return TanimSonucu::hata;
    }

    // ---- Araç varlığı (A5 arac.varlik → A7 varliklar[] indeksi) ----
    const Deger* arac = c.yol_bul("arac.varlik");
    const int ai = arac != nullptr && arac->dizgi_mi() ? kimlik_indeksi(manifest, "varliklar", arac->dizgi) : -1;
    if (ai < 0 || ai > 0xFFFE) {
        hata = "arac.varlik A7 manifestinde bulunamadı: " + yol;
        return TanimSonucu::hata;
    }
    tanim.arac_varlik = static_cast<std::uint16_t>(ai);

    // ---- Ortam (A5 §3.9 → A2 §3.4) ----
    a2::Ortam& o = tanim.ortam;
    const Deger* tur = c.yol_bul("ortam.tur");
    if (tur == nullptr || !tur->dizgi_mi() || (tur->dizgi != "havuz" && tur->dizgi != "acik_su")) {
        hata = "ortam.tur okunamadı";
        return TanimSonucu::hata;
    }
    o.ortam_turu = static_cast<std::uint8_t>(tur->dizgi == "havuz" ? a2::OrtamTuru::havuz : a2::OrtamTuru::acik_su);
    o.statik_varlik_indeksi = 0xFFFFu;
    o.statik_q_nb[0] = 1.0;
    if (const Deger* sahne = c.yol_bul("ortam.sahne"); sahne != nullptr && sahne->dizgi_mi() && !sahne->dizgi.empty()) {
        const int si = kimlik_indeksi(manifest, "varliklar", sahne->dizgi);
        if (si < 0 || si > 0xFFFE) {
            hata = "ortam.sahne A7 manifestinde bulunamadı: " + sahne->dizgi;
            return TanimSonucu::hata;
        }
        o.statik_varlik_indeksi = static_cast<std::uint16_t>(si);
        // sahne_poz çözümlenmiş senaryoda varsayılanıyla yazılıdır (A5-K99); yoksa birim poz.
        if (const Deger* p = c.yol_bul("ortam.sahne_poz.konum"); p != nullptr && !dizi_oku(p, o.statik_p_n)) {
            hata = "ortam.sahne_poz.konum okunamadı";
            return TanimSonucu::hata;
        }
        if (const Deger* q = c.yol_bul("ortam.sahne_poz.yonelim");
            q != nullptr && !(dizi_oku(q, o.statik_q_nb) && birimle(o.statik_q_nb))) {
            hata = "ortam.sahne_poz.yonelim okunamadı";
            return TanimSonucu::hata;
        }
    }
    if (!(dizi_oku(c.yol_bul("ortam.isik.gunes_yonu_n"), o.gunes_yonu_n) && birimle(o.gunes_yonu_n)) ||
        !sayi_oku(c, "ortam.isik.gunes_isinimi_w_m2", o.gunes_isinimi_w_m2) ||
        !dizi_oku(c.yol_bul("ortam.isik.ortam_isigi_rgb_dogrusal"), o.ortam_isigi_rgb_dogrusal)) {
        hata = "ortam.isik okunamadı";
        return TanimSonucu::hata;
    }
    double surum = 0.0;
    if (!dizi_oku(c.yol_bul("ortam.su_optigi.beta_d_m"), o.beta_d_m) ||
        !dizi_oku(c.yol_bul("ortam.su_optigi.beta_b_m"), o.beta_b_m) ||
        !dizi_oku(c.yol_bul("ortam.su_optigi.b_sonsuz_rgb_dogrusal"), o.b_sonsuz_rgb_dogrusal) ||
        !sayi_oku(c, "ortam.su_optigi.bulaniklik_olcegi", o.bulaniklik_olcegi) ||
        !sayi_oku(c, "ortam.su_optigi.surum", surum) || !(surum >= 1.0 && surum <= 4294967295.0)) {
        hata = "ortam.su_optigi okunamadı";
        return TanimSonucu::hata;
    }
    o.su_optigi_surumu = static_cast<std::uint32_t>(surum);

    // ---- Malzeme optiği: A5 geçersiz kılmaları (kalibrasyon.malzeme.<kimlik>.optik; A7-K30) ----
    if (const Deger* ml = c.yol_bul("kalibrasyon.malzeme"); ml != nullptr && ml->tablo_mu()) {
        for (const auto& [kimlik, m] : ml->tablo) {
            const Deger* optik = !senaryo::deneysel_mi(kimlik) && m.tablo_mu() ? m.bul("optik") : nullptr;
            if (optik == nullptr || !optik->tablo_mu()) {
                continue;
            }
            a2::MalzemeOptikGirdisi g{};
            if (const Deger* r = optik->bul("taban_rengi_dogrusal"); r != nullptr) {
                if (!dizi_oku(r, g.taban_rengi_dogrusal)) {
                    hata = "kalibrasyon.malzeme." + kimlik + ".optik.taban_rengi_dogrusal okunamadı";
                    return TanimSonucu::hata;
                }
                g.maske = static_cast<std::uint16_t>(g.maske | 0x1u);
            }
            if (const Deger* r = optik->bul("metaliklik"); r != nullptr && r->sayisal_mi()) {
                g.metaliklik = r->cift();
                g.maske = static_cast<std::uint16_t>(g.maske | 0x2u);
            }
            if (const Deger* r = optik->bul("puruzluluk"); r != nullptr && r->sayisal_mi()) {
                g.puruzluluk = r->cift();
                g.maske = static_cast<std::uint16_t>(g.maske | 0x4u);
            }
            if (g.maske == 0u) {
                continue;
            }
            const int mi = kimlik_indeksi(manifest, "malzemeler", kimlik);
            if (mi < 0 || mi > 0xFFFF) {
                hata = "kalibrasyon.malzeme." + kimlik + " A7 manifestinde bulunamadı";
                return TanimSonucu::hata;
            }
            g.malzeme_indeksi = static_cast<std::uint16_t>(mi);
            tanim.malzeme.push_back(g);
        }
        if (tanim.malzeme.size() > a2::kAzamiMalzemeOptik) {
            hata = "optik malzeme geçersiz kılması A2 sınırını aşıyor";
            return TanimSonucu::hata;
        }
    }
    return TanimSonucu::tamam;
}

}  // namespace a2b

namespace {

using Saat = std::chrono::steady_clock;

std::int64_t gecen_ns(Saat::time_point t0) noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - t0).count();
}

std::atomic<std::uint64_t> g_baslatma{0};   // yalnız sınama (a2_baslatma_sayisi)

}  // namespace

std::uint64_t a2_baslatma_sayisi() noexcept { return g_baslatma.load(std::memory_order_relaxed); }

std::unique_ptr<A2Baglanti> A2Baglanti::baslat(const A2BaglantiAyari& ayar, a2b::Tanim tanim, std::string& hata)
{
    a2::SunucuAyari sa;
    sa.port = ayar.port;
    sa.sinama_bos_port = ayar.sinama_bos_port;
    sa.kilit_dosyasi = ayar.kilit_dosyasi;
    if (ayar.gonderme_kuyrugu_bayt != 0u) {
        sa.gonderme_kuyrugu_bayt = ayar.gonderme_kuyrugu_bayt;
    }
    sa.gunluk = ayar.gunluk;
    g_baslatma.fetch_add(1u, std::memory_order_relaxed);
    std::unique_ptr<a2::Sunucu> sv = a2::Sunucu::baslat(sa, hata);
    if (!sv) {
        return nullptr;   // A2-K93: geçersiz port; süreç başlamaz
    }
    std::unique_ptr<A2Baglanti> b(new A2Baglanti());
    b->tanim_ = std::move(tanim);
    b->ilk_durum_ = sv->durum();
    b->ilk_aciklama_ = sv->durum_aciklamasi();
    b->ilk_port_ = sv->port();
    b->etkin_ = b->ilk_durum_ == a2::A2Durumu::dinliyor;
    b->sv_ = std::move(sv);
    return b;
}

A2Baglanti::~A2Baglanti()
{
    if (sv_ && !kapandi_) {
        A2Ozeti atilan;
        kapat(atilan);
    }
}

std::uint16_t A2Baglanti::port() const noexcept { return ilk_port_; }

const char* A2Baglanti::durum_adi() const noexcept { return a2::a2_durumu_adi(ilk_durum_); }

std::string A2Baglanti::aciklama() const { return ilk_aciklama_; }

bool A2Baglanti::calistirma_basladi(Kip kip, const fizik::OrtamParametreleri& ortam, const fizik::DurumAnlik& d,
                                    const std::array<bool, 2>& dolu) noexcept
{
    if (!etkin_) {
        return false;
    }
    arac_kimligi_ = a2b::yeni_govde_kimligi();   // W5-K32
    const std::array<a2::GovdeKaydi, 1> govdeler{
        a2::GovdeKaydi{arac_kimligi_, tanim_.arac_varlik, a2::GovdeTuru::arac, a2b::dolu_tup_maskesi(dolu)}};
    const std::array<a2::GovdeAni, 1> anlar{a2b::govde_ani(arac_kimligi_, d)};

    a2::CalistirmaTanimi t;
    t.sim_kipi = kip == Kip::hil ? a2::SimKipi::hil_1x : a2::SimKipi::sil;
    t.kademe_3b = tanim_.kademe;
    t.manifest_yolu = tanim_.manifest_yolu;
    t.manifest_sha256 = tanim_.manifest_sha256;
    t.adim_no = d.adim_no;
    t.calisma_durumu = a2::CalismaDurumu::calisiyor;
    // Dalga kümesi: fizikte dalga modeli yok (ortam.dalga.spektrum yalnız `yok`): 0 bileşen. g ve
    // rho fiziğin kullandığı değerler; su derinliği sabitse (> 0) sonlu derinlik bildirilir.
    t.dalga.tohum = 0;   // küme üretilmedi
    t.dalga.g = ortam.yercekimi;
    t.dalga.rho = ortam.su.yogunluk;
    t.dalga.derin_su = !(std::isfinite(ortam.derinlik) && ortam.derinlik > 0.0);
    t.dalga.derinlik_m = t.dalga.derin_su ? 0.0 : ortam.derinlik;
    t.ortam = tanim_.ortam;
    t.malzeme = tanim_.malzeme;
    t.govdeler = govdeler;
    t.anlar = anlar;
    tanimli_ = sv_->yayinci().yeni_calistirma(t);
    if (tanimli_) {
        son_adim_ = d.adim_no;
    } else {
        ++reddedilen_;
    }
    return tanimli_;
}

void A2Baglanti::adim_siniri(const fizik::DurumAnlik& d) noexcept
{
    if (!tanimli_ || d.adim_no <= son_adim_) {
        return;   // A2'siz, tanımsız, ya da bu sınır zaten verildi (tanımdaki başlangıç sınırı dahil)
    }
    const Saat::time_point t0 = Saat::now();
    const std::array<a2::GovdeAni, 1> anlar{a2b::govde_ani(arac_kimligi_, d)};
    const bool uygulandi = sv_->yayinci().adim_siniri(d.adim_no, anlar);
    const std::int64_t ns = gecen_ns(t0);
    son_adim_ = d.adim_no;
    ++adim_siniri_;
    cagri_toplam_ns_ += ns;
    if (ns > cagri_en_kotu_ns_) {
        cagri_en_kotu_ns_ = ns;
    }
    if (!uygulandi) {
        ++reddedilen_;
    }
}

void A2Baglanti::duraklat() noexcept
{
    if (tanimli_ && !sv_->yayinci().duraklat()) {
        ++reddedilen_;
    }
}

void A2Baglanti::surdur() noexcept
{
    if (tanimli_ && !sv_->yayinci().surdur()) {
        ++reddedilen_;
    }
}

void A2Baglanti::yeni_tanim(a2b::Tanim tanim)
{
    tanim_ = std::move(tanim);
    tanimli_ = false;
    durdu_ = false;
    arac_kimligi_ = 0;
    son_adim_ = 0;
    adim_siniri_ = 0;
    reddedilen_ = 0;
    cagri_toplam_ns_ = 0;
    cagri_en_kotu_ns_ = 0;
}

void A2Baglanti::kapat(A2Ozeti& ozet, bool son) noexcept
{
    try {
        if (etkin_ && tanimli_ && !durdu_ && !kapandi_) {
            durdu_ = true;
            (void)sv_->yayinci().calistirma_durdu();   // son sınırın durumu "durdu" ile bir kez daha gider
        }
        if (son && !kapandi_) {
            kapandi_ = true;
            if (etkin_) {
                a2::Yayinci& y = sv_->yayinci();
                (void)y.kapaniyor();              // bağlı oturuma veda(sim_kapaniyor) (A2-K67)
                // G/Ç iş parçacığı kapanış kaydını işlesin (son durum ve veda sırasıyla gitsin):
                // dinleyicinin kapandığı görülene kadar, en çok 250 ms (G/Ç tiki ≤ 20 ms). Dünya
                // döngüsü ve A1 kapanışı bu noktada bitmiştir.
                const Saat::time_point bitis = Saat::now() + std::chrono::milliseconds(250);
                while (sv_->durum() != a2::A2Durumu::durduruldu && Saat::now() < bitis) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            sv_->durdur();
        }
        ozet.durum = a2::a2_durumu_adi(ilk_durum_);
        ozet.aciklama = ilk_aciklama_;
        ozet.port = ilk_port_;
        ozet.calistirma_tanimlandi = tanimli_;
        ozet.arac_kimligi = arac_kimligi_;
        ozet.adim_siniri = adim_siniri_;
        ozet.reddedilen_cagri = reddedilen_;
        ozet.cagri_toplam_ns = cagri_toplam_ns_;
        ozet.cagri_en_kotu_ns = cagri_en_kotu_ns_;
        ozet.sayac = sv_->sayaclar();
    } catch (...) {
        // Kapanış özeti yazılamadı (bellek): durum alanları eldeki haliyle kalır.
    }
}

}  // namespace ida::sunucu
