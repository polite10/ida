// cekirdek.cpp — bkz. cekirdek.hpp.
#include "cekirdek.hpp"

#include "takvim.hpp"

#include <array>
#include <cstring>
#include <span>

namespace ida::a2::ic {

namespace {

constexpr std::uint8_t kDuraklatildi = static_cast<std::uint8_t>(CalismaDurumu::duraklatildi);
constexpr std::uint8_t kCalisiyor = static_cast<std::uint8_t>(CalismaDurumu::calisiyor);

void azami_yaz(std::atomic<std::uint64_t>& a, std::uint64_t v) noexcept
{
    if (v > a.load(std::memory_order_relaxed)) {
        a.store(v, std::memory_order_relaxed);  // tek yazar (G/Ç iş parçacığı)
    }
}

void artir(std::atomic<std::uint64_t>& a, std::uint64_t n = 1) noexcept { a.fetch_add(n, std::memory_order_relaxed); }

}  // namespace

namespace {

// A2-K26: sınır en çok 8 MiB'dir (sınamalar küçültebilir); el sıkışma iletilerinin sığması için en az 64 KiB.
std::size_t kuyruk_siniri_sinirla(std::size_t istenen) noexcept
{
    const auto azami = static_cast<std::size_t>(kGondermeKuyruguAzamiBayt);
    if (istenen < 65536) {
        return 65536;
    }
    return istenen > azami ? azami : istenen;
}

}  // namespace

Cekirdek::Cekirdek(Yayinci& y, std::size_t kuyruk_siniri) : y_(y.ic()), kuyruk_(kuyruk_siniri_sinirla(kuyruk_siniri))
{
    is_.reserve(kBaslikBoyutu + kAzamiMalzemeOptikYuku);
}

SunucuSayaclari Cekirdek::sayaclar() const
{
    const auto o = [](const std::atomic<std::uint64_t>& a) { return a.load(std::memory_order_relaxed); };
    SunucuSayaclari r;
    r.baglanti_sayisi = o(s_.baglanti_sayisi);
    r.mesgul_reddi = o(s_.mesgul_reddi);
    r.biten_oturum = o(s_.biten_oturum);
    r.son_kopma_nedeni = static_cast<KopmaNedeni>(s_.son_kopma.load(std::memory_order_relaxed));
    r.son_karsi_veda_nedeni = s_.son_karsi_veda.load(std::memory_order_relaxed);
    r.protokol_hatasi = o(s_.protokol_hatasi);
    r.seyreltilen_durum = o(s_.seyreltilen_durum);
    r.kuyrukta_atilan_durum = o(s_.kuyrukta_atilan_durum);
    r.kuyruk_tasmasi = o(s_.kuyruk_tasmasi);
    r.yayin_tasmasi_kapanis = o(s_.yayin_tasmasi_kapanis);
    r.gonderilen_ileti = o(s_.gonderilen_ileti);
    r.gonderilen_durum = o(s_.gonderilen_durum);
    r.gonderilen_olay = o(s_.gonderilen_olay);
    r.gonderilen_nabiz = o(s_.gonderilen_nabiz);
    r.alinan_ileti = o(s_.alinan_ileti);
    r.alinan_nabiz = o(s_.alinan_nabiz);
    r.azami_kuyruk_bayt = o(s_.azami_kuyruk_bayt);
    r.tam_durum_bekleme = o(s_.tam_durum_bekleme);
    r.oturum_var = s_.oturum_var.load(std::memory_order_relaxed);
    r.hazir_alindi = s_.hazir_alindi.load(std::memory_order_relaxed);
    r.oturum_kimligi = o(s_.oturum_kimligi);
    r.oturum_surum_alt = s_.oturum_surum_alt.load(std::memory_order_relaxed);
    r.son_uygulanan_t_ns = s_.son_uygulanan_t_ns.load(std::memory_order_relaxed);
    r.cizim_hizi_mhz = s_.cizim_hizi_mhz.load(std::memory_order_relaxed);
    r.yayin.yazilan_kayit = o(y_.yazilan_kayit);
    r.yayin.dusen_durum = o(y_.dusen_durum);
    r.yayin.kayip_korunan = o(y_.kayip_korunan);
    r.yayin.olay_tasmasi = o(y_.olay_tasmasi);
    r.yayin.gecersiz_cagri = o(y_.gecersiz_cagri);
    r.yayin.eksik_govde = o(y_.eksik_govde);
    return r;
}

void Cekirdek::mesgul_reddi(Baytlar& c)
{
    artir(s_.mesgul_reddi);
    kodla_ret(c, 0, RetNedeni::mesgul, "başka bir görselleştirici bağlı (A2-K03)");
}

bool Cekirdek::baglanti_acildi(std::int64_t simdi_ns, std::uint64_t oturum_kimligi)
{
    if (asama_ != Asama::yok || kapanis_ || kapandi_) {
        return false;
    }
    kuyruk_.temizle();
    okuyucu_.temizle();
    asama_ = Asama::merhaba_bekleniyor;
    oturum_kimligi_ = oturum_kimligi != 0 ? oturum_kimligi : 1;  // §3.3: ≠ 0
    surum_alt_ = 0;
    baglanti_ns_ = simdi_ns;
    hosgeldin_ns_ = simdi_ns;
    son_alim_ns_ = simdi_ns;
    son_nabiz_ns_ = simdi_ns;
    sira_ = 0;
    ilk_alim_ = true;
    son_alinan_sira_ = 0;
    sd_var_ = false;
    sd_sonrasi_degisim_ = false;
    artir(s_.baglanti_sayisi);
    s_.oturum_var.store(true, std::memory_order_relaxed);
    s_.hazir_alindi.store(false, std::memory_order_relaxed);
    s_.oturum_kimligi.store(oturum_kimligi_, std::memory_order_relaxed);
    s_.oturum_surum_alt.store(0, std::memory_order_relaxed);
    return true;
}

void Cekirdek::baglanti_koptu()
{
    kuyruk_.temizle();
    kapanis_ = false;
    if (asama_ == Asama::yok) {
        return;  // oturumu biz bitirmiştik; soket kapanış aktarılmadan gitti
    }
    asama_ = Asama::yok;
    artir(s_.biten_oturum);
    s_.son_kopma.store(static_cast<std::uint8_t>(KopmaNedeni::karsi_kapatti), std::memory_order_relaxed);
    s_.oturum_var.store(false, std::memory_order_relaxed);
}

Baytlar Cekirdek::kapanis_al()
{
    kapanis_ = false;
    return kuyruk_.kalanlari_al();
}

void Cekirdek::bitir_(KopmaNedeni neden, bool hepsini_at, Son son, std::uint32_t kod, const char* ayrinti)
{
    // Gönderilmemiş takvimli durumlar artık gereksizdir. Kalan çoksa istemci okumuyordur: son
    // iletinin ulaşabilmesi için öncekiler de atılır (kısmen yazılmış baş korunur). Hiç gönderilmemiş
    // iletilerin sırası yeniden kullanılır: son ileti, gönderilmiş son iletinin hemen ardındaki
    // sırayı alır (oturumun ilk iletisi de atıldıysa 0'ı; A2-K07).
    if (hepsini_at || kuyruk_.bayt() > kKapanisKalanSiniri) {
        std::uint32_t ilk = sira_;
        if (kuyruk_.hepsini_at(ilk) > 0) {
            sira_ = ilk;
        }
    } else {
        kuyruk_.atilabilirleri_at();
    }
    is_.clear();
    if (son == Son::veda) {
        kodla_veda(is_, sira_, static_cast<VedaNedeni>(kod));
    } else if (son == Son::ret) {
        kodla_ret(is_, sira_, static_cast<RetNedeni>(kod), ayrinti);
    }
    if (!is_.empty()) {
        kuyruk_.ekle(is_.data(), is_.size(), sira_, false, true);
        ++sira_;
        artir(s_.gonderilen_ileti);
        is_.clear();
    }
    asama_ = Asama::yok;
    kapanis_ = true;
    artir(s_.biten_oturum);
    s_.son_kopma.store(static_cast<std::uint8_t>(neden), std::memory_order_relaxed);
    s_.oturum_var.store(false, std::memory_order_relaxed);
}

void Cekirdek::protokol_hatasi_()
{
    // A2-K06, K07 / H01: veda(protokol_hatasi), kapat.
    artir(s_.protokol_hatasi);
    veda_(VedaNedeni::protokol_hatasi, KopmaNedeni::protokol_hatasi);
}

void Cekirdek::veda_(VedaNedeni neden, KopmaNedeni kopma)
{
    bitir_(kopma, false, Son::veda, static_cast<std::uint32_t>(neden), "");
}

void Cekirdek::ret_(RetNedeni neden, const char* ayrinti, KopmaNedeni kopma)
{
    bitir_(kopma, false, Son::ret, static_cast<std::uint32_t>(neden), ayrinti);
}

void Cekirdek::tasma_(KopmaNedeni kopma)
{
    // A2-K26: diğer iletilere yer kalmadı: veda(tampon_tasti), kapat.
    artir(kopma == KopmaNedeni::tampon_tasti ? s_.kuyruk_tasmasi : s_.yayin_tasmasi_kapanis);
    bitir_(kopma, true, Son::veda, static_cast<std::uint32_t>(VedaNedeni::tampon_tasti), "");
}

void Cekirdek::sim_kapaniyor()
{
    kapandi_ = true;
    if (asama_ != Asama::yok) {
        veda_(VedaNedeni::sim_kapaniyor, KopmaNedeni::sim_kapaniyor);
    }
}

bool Cekirdek::kuyruga_(bool atilabilir)
{
    Baslik b{};
    std::memcpy(&b, is_.data(), sizeof b);  // is_ her zaman tam bir ileti taşır
    if (!kuyruk_.ekle(is_.data(), is_.size(), b.sira, atilabilir)) {
        // A2-K26: kuyruk sınırda: gönderilmemiş takvimli durumlar atılır (her biri sıra boşluğu).
        artir(s_.kuyrukta_atilan_durum, kuyruk_.atilabilirleri_at());
        if (!kuyruk_.ekle(is_.data(), is_.size(), b.sira, atilabilir)) {
            if (atilabilir) {
                artir(s_.kuyrukta_atilan_durum);
                is_.clear();
                return false;
            }
            tasma_(KopmaNedeni::tampon_tasti);
            return false;
        }
    }
    is_.clear();
    artir(s_.gonderilen_ileti);
    azami_yaz(s_.azami_kuyruk_bayt, kuyruk_.bayt());
    return true;
}

// ---------------------------------------------------------------------------------------------
// Gelen iletiler
// ---------------------------------------------------------------------------------------------

void Cekirdek::veri_geldi(const std::uint8_t* p, std::size_t n, std::int64_t simdi_ns)
{
    // A2-K67: oturum bittikten (veda) sonra gelenler yok sayılır.
    while (n > 0 && asama_ != Asama::yok) {
        const GelenOkuyucu::Sonuc r = okuyucu_.besle(p, n);
        if (r == GelenOkuyucu::Sonuc::eksik) {
            return;
        }
        if (r == GelenOkuyucu::Sonuc::hata) {
            protokol_hatasi_();
            return;
        }
        ileti_isle_(okuyucu_.baslik(), okuyucu_.yuk(), simdi_ns);
    }
}

void Cekirdek::ileti_isle_(const Baslik& b, const std::uint8_t* yuk, std::int64_t simdi_ns)
{
    artir(s_.alinan_ileti);
    // A2-K07: ilk ileti 0, sonra tam 1 artar; görselleştirici → sim yönünde boşluk da hatadır.
    if (ilk_alim_ ? b.sira != 0u : b.sira != son_alinan_sira_ + 1u) {
        protokol_hatasi_();
        return;
    }
    ilk_alim_ = false;
    son_alinan_sira_ = b.sira;
    son_alim_ns_ = simdi_ns;

    const auto tur = static_cast<IletiTuru>(b.tur);
    if (tur == IletiTuru::veda) {
        Veda v{};
        std::memcpy(&v, yuk, sizeof v);
        if (v.neden > static_cast<std::uint32_t>(VedaNedeni::yeni_calistirma)) {
            protokol_hatasi_();  // §3.3: neden 0–5
            return;
        }
        s_.son_karsi_veda.store(v.neden, std::memory_order_relaxed);
        bitir_(KopmaNedeni::karsi_veda, false, Son::yok, 0, "");
        return;
    }
    if (asama_ == Asama::merhaba_bekleniyor) {
        if (tur != IletiTuru::merhaba) {
            protokol_hatasi_();  // A2-K10: ilk ileti merhaba
            return;
        }
        merhaba_isle_(yuk, simdi_ns);
        return;
    }
    switch (tur) {
        case IletiTuru::goruntuleyici_nabzi: {
            GoruntuleyiciNabzi g{};
            std::memcpy(&g, yuk, sizeof g);
            artir(s_.alinan_nabiz);
            s_.son_uygulanan_t_ns.store(g.son_uygulanan_t_ns, std::memory_order_relaxed);
            s_.cizim_hizi_mhz.store(g.cizim_hizi_mhz, std::memory_order_relaxed);
            return;
        }
        case IletiTuru::hazir:
            if (asama_ != Asama::hazir_bekleniyor) {
                protokol_hatasi_();  // hosgeldin'den önce ya da ikinci hazir
                return;
            }
            hazir_isle_(yuk);
            return;
        case IletiTuru::merhaba:      // ikinci merhaba
        case IletiTuru::kare_hazir:   // W5-K30: bu dilimde kamera isteği gönderilmez; bu üç ileti
        case IletiTuru::kare_dustu:   // istenmemiş yanıttır (kısıtlayan okuma: protokol hatası)
        case IletiTuru::halka_eslendi:
        default:
            protokol_hatasi_();
            return;
    }
}

void Cekirdek::merhaba_isle_(const std::uint8_t* yuk, std::int64_t simdi_ns)
{
    Merhaba m{};
    std::memcpy(&m, yuk, sizeof m);
    // A2-K11: ana sürüm farkı öteki alanlardan önce değerlendirilir (düzen farklı olabilir).
    if (m.surum_ana != kSurumAna) {
        ret_(RetNedeni::surum, "A2 ana sürümü uyuşmuyor (A2-K11)", KopmaNedeni::surum_reddi);
        return;
    }
    // §3.3: platform 1–2, çizici 1, metinler NUL ile biter; geçersizse ret(protokol).
    if (m.platform < 1 || m.platform > 2 || m.cizici != static_cast<std::uint8_t>(Cizici::forward_plus) ||
        !metin_nul_var(m.godot_surumu, sizeof m.godot_surumu) ||
        !metin_nul_var(m.uzanti_surumu, sizeof m.uzanti_surumu) || !metin_nul_var(m.gpu_adi, sizeof m.gpu_adi)) {
        artir(s_.protokol_hatasi);
        ret_(RetNedeni::protokol, "merhaba alanı geçersiz (A2 §3.3)", KopmaNedeni::protokol_reddi);
        return;
    }
    surum_alt_ = m.surum_alt < kSurumAlt ? m.surum_alt : kSurumAlt;  // A2-K11: en küçüğü
    s_.oturum_surum_alt.store(surum_alt_, std::memory_order_relaxed);
    asama_ = Asama::tam_durum_bekleniyor;
    (void)oturum_baslat_(simdi_ns);
}

void Cekirdek::hazir_isle_(const std::uint8_t* yuk)
{
    Hazir h{};
    std::memcpy(&h, yuk, sizeof h);
    if (h.durum > static_cast<std::uint32_t>(HazirDurumu::halka_duzeni_uyusmaz)) {
        protokol_hatasi_();  // §3.3: durum 0–7
        return;
    }
    // A2-K14 / H03–H05
    if (h.durum == static_cast<std::uint32_t>(HazirDurumu::halka_eslenemedi) ||
        h.durum == static_cast<std::uint32_t>(HazirDurumu::halka_duzeni_uyusmaz)) {
        ret_(RetNedeni::halka, "hazir.durum halka hatası bildiriyor", KopmaNedeni::halka_reddi);
        return;
    }
    if (h.durum != static_cast<std::uint32_t>(HazirDurumu::basarili)) {
        ret_(RetNedeni::manifest, "hazir.durum başarısız (A2-K14)", KopmaNedeni::manifest_reddi);
        return;
    }
    if (std::memcmp(h.manifest_sha256, manifest_, sizeof manifest_) != 0) {
        ret_(RetNedeni::manifest, "manifest özeti uyuşmuyor (A2-K14)", KopmaNedeni::manifest_reddi);
        return;
    }
    asama_ = Asama::akis;
    s_.hazir_alindi.store(true, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------------------------
// El sıkışmanın sim tarafı: tam durum (A2-K12, K65)
// ---------------------------------------------------------------------------------------------

bool Cekirdek::oturum_baslat_(std::int64_t simdi_ns)
{
    const Anlik& a = y_.anlik.tuttur();
    if (!a.tanimli) {
        y_.anlik.birak();
        artir(s_.tam_durum_bekleme);
        return false;
    }
    const std::size_t n = a.govde_sayisi;
    std::array<GovdeBilgisi, kAzamiGovde> bilgi{};
    for (std::size_t i = 0; i < n; ++i) {
        bilgi[i].govde_kimligi = a.govde[i].govde_kimligi;
        bilgi[i].sureklilik_no = a.govde[i].sureklilik_no;
        bilgi[i].varlik_indeksi = a.varlik[i];
        bilgi[i].govde_turu = a.govde[i].govde_turu;
        bilgi[i].dolu_tupler = a.govde[i].dolu_tupler;
    }
    Hosgeldin h{};
    h.oturum_kimligi = oturum_kimligi_;
    h.surum_ana = kSurumAna;
    h.surum_alt = surum_alt_;
    h.sim_kipi = a.sim_kipi;
    h.kademe_3b = a.kademe_3b;
    h.t_ns = static_cast<std::int64_t>(a.adim_no) * kAdimNs;
    h.adim_no = a.adim_no;
    h.adim_ns = kAdimNs;
    h.durum_hiz_pay = kDurumHizPay;
    h.durum_hiz_payda = kDurumHizPayda;
    std::memcpy(h.manifest_sha256, a.manifest_sha256, sizeof h.manifest_sha256);
    std::memcpy(h.manifest_yolu, a.manifest_yolu, sizeof h.manifest_yolu);
    h.calisma_nesli = a.nesil;

    // Akış konumu ve oturumun tabanı: bu kesitten sonraki kayıtlar iletilecek.
    anlik_sira_ = a.sira;
    nesil_ = a.nesil;
    t_ns_ = h.t_ns;
    adim_no_ = a.adim_no;
    calisma_durumu_ = a.calisma_durumu;
    sim_kipi_ = a.sim_kipi;
    dalga_surumu_ = a.dalga.surum;
    ortam_surumu_ = a.ortam.ortam_surumu;
    malzeme_surumu_ = a.malzeme.surum;
    std::memcpy(manifest_, a.manifest_sha256, sizeof manifest_);

    // A2-K12: hosgeldin → dalga_kumesi → ortam → malzeme_optik; ardından akış.
    bool tamam = true;
    is_.clear();
    kodla_hosgeldin(is_, sira_++, h, std::span<const GovdeBilgisi>(bilgi.data(), n));
    tamam = tamam && kuyruga_(false);
    if (tamam) {
        kodla_dalga(is_, sira_++, a.dalga, std::span<const DalgaBileseni>(a.bilesen.data(), a.dalga.bilesen_sayisi));
        tamam = kuyruga_(false);
    }
    if (tamam) {
        kodla_ortam(is_, sira_++, a.ortam);
        tamam = kuyruga_(false);
    }
    if (tamam) {
        kodla_malzeme(is_, sira_++, a.malzeme,
                      std::span<const MalzemeOptikGirdisi>(a.girdi.data(), a.malzeme.girdi_sayisi));
        tamam = kuyruga_(false);
    }
    if (tamam) {
        // Kesitin adım sınırındaki durum hemen gider (takvimde değilse takvim dışı): oturum koşunun
        // ortasında ya da duraklatılmışken açılsa da gövdelerin pozu hosgeldin tablosuyla aynı
        // sınırdandır (A2-K65).
        Durum d{};
        d.t_ns = h.t_ns;
        d.adim_no = a.adim_no;
        d.gonderim_duvar_ns = simdi_ns > 0 ? simdi_ns : 1;
        d.dalga_kumesi_surumu = a.dalga.surum;
        d.calisma_durumu = a.calisma_durumu;
        d.bayraklar = takvimde(a.adim_no) ? std::uint8_t{0} : kDurumTakvimDisi;
        kodla_durum(is_, sira_++, d, std::span<const GovdeDurumu>(a.govde.data(), n));
        tamam = kuyruga_(false);
        if (tamam) {
            artir(s_.gonderilen_durum);
            sd_var_ = true;
            sd_sonrasi_degisim_ = false;
            sd_nesil_ = a.nesil;
            sd_adim_ = a.adim_no;
            sd_cd_ = a.calisma_durumu;
        }
    }
    y_.anlik.birak();
    if (!tamam) {
        return false;  // kuyruk sınırı el sıkışmaya bile yetmedi: oturum tasma_ ile bitti
    }
    asama_ = Asama::hazir_bekleniyor;
    hosgeldin_ns_ = simdi_ns;
    son_nabiz_ns_ = simdi_ns;
    kova_ns_ = kSeyreltmeKovaNs;
    kova_guncelleme_ns_ = simdi_ns;
    gorulen_dusen_ = y_.dusen_durum.load(std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Zaman ve yayın halkası
// ---------------------------------------------------------------------------------------------

void Cekirdek::tik(std::int64_t simdi_ns)
{
    if (asama_ == Asama::merhaba_bekleniyor) {
        // A2-K10: 5 s içinde merhaba gelmezse bağlantı kapatılır (ileti gönderilmez).
        if (simdi_ns - baglanti_ns_ > kMerhabaZamanAsimiNs) {
            bitir_(KopmaNedeni::merhaba_zaman_asimi, true, Son::yok, 0, "");
        }
    } else if (asama_ != Asama::yok) {
        // A2-K62 (W5-K28: hazir penceresinde de): 3 s boyunca hiçbir ileti gelmedi.
        if (simdi_ns - son_alim_ns_ > kNabizZamanAsimiNs) {
            veda_(VedaNedeni::nabiz_zaman_asimi, KopmaNedeni::nabiz_zaman_asimi);
        } else if (asama_ == Asama::hazir_bekleniyor && simdi_ns - hosgeldin_ns_ > kHazirZamanAsimiNs) {
            // A2-K15
            ret_(RetNedeni::zaman_asimi, "hazir 120 s içinde gelmedi (A2-K15)", KopmaNedeni::hazir_zaman_asimi);
        }
    }
    if (asama_ == Asama::tam_durum_bekleniyor) {
        (void)oturum_baslat_(simdi_ns);
    }
    halkayi_isle_(simdi_ns);
    if (akista_() && simdi_ns - son_nabiz_ns_ >= kNabizAraligiNs) {
        // A2-K62: 1 s aralıkla sim_nabzi (duraklatılmışken de: A2-K24).
        son_nabiz_ns_ = simdi_ns;
        SimNabzi nb{};
        nb.t_ns = t_ns_;
        nb.adim_no = adim_no_;
        nb.gonderim_duvar_ns = simdi_ns > 0 ? simdi_ns : 1;
        nb.calisma_durumu = calisma_durumu_;
        nb.sim_kipi = sim_kipi_;
        is_.clear();
        kodla_sim_nabzi(is_, sira_++, nb);
        if (kuyruga_(false)) {
            artir(s_.gonderilen_nabiz);
        }
    }
}

bool Cekirdek::jeton_al_(std::int64_t simdi_ns)
{
    const std::int64_t gecen = simdi_ns - kova_guncelleme_ns_;
    if (gecen > 0) {
        kova_ns_ = gecen >= kSeyreltmeKovaNs - kova_ns_ ? kSeyreltmeKovaNs : kova_ns_ + gecen;
        kova_guncelleme_ns_ = simdi_ns;
    }
    if (kova_ns_ < kSeyreltmeBirimNs) {
        return false;
    }
    kova_ns_ -= kSeyreltmeBirimNs;
    return true;
}

void Cekirdek::halkayi_isle_(std::int64_t simdi_ns)
{
    if (akista_()) {
        // Dünya tarafında halkaya sığmadığı için atılan takvimli durumlar da sıra boşluğudur.
        const std::uint64_t dusen = y_.dusen_durum.load(std::memory_order_relaxed);
        sira_ += static_cast<std::uint32_t>(dusen - gorulen_dusen_);
        gorulen_dusen_ = dusen;
    }
    // Bir tikte en çok bir halka dolusu kayıt işlenir (dünya çok hızlı yazsa da tik sonlanır).
    std::size_t islenen = 0;
    const std::size_t tavan = y_.halka.kapasite();
    std::size_t n = 0;
    for (const std::uint8_t* p = y_.halka.oku(n); p != nullptr; p = y_.halka.oku(n)) {
        if (n >= sizeof(KayitBasligi)) {
            KayitBasligi kb{};
            std::memcpy(&kb, p, sizeof kb);
            kayit_isle_(kb, p + sizeof kb, n - sizeof kb, simdi_ns);
        }
        y_.halka.birak();
        islenen += n + 8;
        if (islenen >= tavan) {
            break;
        }
    }
    // Atılamayan bir kayıt (olay, takvim dışı durum, küme) dünya tarafında kaybolduysa bu oturumun
    // akışı artık tam değildir: veda(tampon_tasti); istemci yeniden bağlanınca tam durumu alır.
    if (akista_() && y_.kayip_sira.load(std::memory_order_acquire) > anlik_sira_) {
        tasma_(KopmaNedeni::yayin_tasmasi);
    }
}

void Cekirdek::kayit_isle_(const KayitBasligi& kb, const std::uint8_t* yuk, std::size_t n, std::int64_t simdi_ns)
{
    const auto tur = static_cast<KayitTuru>(kb.tur);
    if (tur == KayitTuru::kapanis) {
        kapandi_ = true;  // oturum olsun olmasın: dinleyici kapanır
        if (asama_ != Asama::yok) {
            veda_(VedaNedeni::sim_kapaniyor, KopmaNedeni::sim_kapaniyor);
        }
        return;
    }
    // Oturum akışta değilse kayıt atılır (yeni oturum tam durumu kesitten alır). Akıştaysa kesitin
    // kapsadığı kayıtlar atılır (yayinci_ic.hpp başındaki değişmez).
    if (!akista_() || kb.sira <= anlik_sira_) {
        return;
    }
    switch (tur) {
        case KayitTuru::olay: {
            if (n != sizeof(SahneOlayi)) {
                return;
            }
            SahneOlayi o{};
            std::memcpy(&o, yuk, sizeof o);
            t_ns_ = o.t_ns;
            adim_no_ = o.adim_no;
            const auto ot = static_cast<SahneOlayiTuru>(o.tur);
            if (ot == SahneOlayiTuru::duraklat) {
                calisma_durumu_ = kDuraklatildi;
            } else if (ot == SahneOlayiTuru::devam) {
                calisma_durumu_ = kCalisiyor;
            } else if (ot == SahneOlayiTuru::sifirlama) {
                nesil_ = o.calisma_nesli;
            }
            is_.clear();
            kodla_sahne_olayi(is_, sira_++, o);
            sd_sonrasi_degisim_ = true;
            if (kuyruga_(false)) {
                artir(s_.gonderilen_olay);
            }
            return;
        }
        case KayitTuru::durum: {
            if (n < sizeof(Durum)) {
                return;
            }
            Durum d{};
            std::memcpy(&d, yuk, sizeof d);
            const std::size_t govde = d.govde_sayisi;
            if (govde > kAzamiGovde || n != sizeof(Durum) + govde * sizeof(GovdeDurumu)) {
                return;
            }
            const bool korunan = (kb.bayrak & kKayitKorunan) != 0;
            t_ns_ = d.t_ns;
            adim_no_ = d.adim_no;
            calisma_durumu_ = d.calisma_durumu;
            // Aynı sınırın durumu az önce gönderildiyse (takvimli iletinin ardından gelen korunan
            // yineleme: A2-K21'in T − h iletisi, duraklatma ya da durma sınırı) ikinci kez gitmez.
            if (korunan && sd_var_ && !sd_sonrasi_degisim_ && sd_nesil_ == nesil_ && sd_adim_ == d.adim_no &&
                sd_cd_ == d.calisma_durumu) {
                return;
            }
            // A2-K25: SIL'de takvimli durum duvar saatine göre seyreltilir; takvim dışı/korunan seyreltilmez.
            if (!korunan && sim_kipi_ == static_cast<std::uint8_t>(SimKipi::sil) && !jeton_al_(simdi_ns)) {
                ++sira_;  // A2-K07: seyreltme sıra boşluğu olarak görünür
                artir(s_.seyreltilen_durum);
                return;
            }
            if (korunan && d.bayraklar == 0 && sim_kipi_ == static_cast<std::uint8_t>(SimKipi::sil)) {
                // Takvim adımına denk gelen sınır iletisi (A2-K21) seyreltilmez ama kovadan düşülür
                // (borç): uzun dönem hızı yine sınırlı kalır.
                if (!jeton_al_(simdi_ns) && kova_ns_ > -kSeyreltmeKovaNs) {
                    kova_ns_ -= kSeyreltmeBirimNs;
                }
            }
            d.gonderim_duvar_ns = simdi_ns > 0 ? simdi_ns : 1;  // A2-K08: kuyruğa konduğu an
            // Gövde durumları halkadan (8 bayta hizalı; GovdeDurumu doğal hizalı) kopyalanarak kodlanır.
            is_.clear();
            Durum sabit = d;
            const std::size_t bas = is_.size();
            is_.resize(bas + kBaslikBoyutu + sizeof(Durum) + govde * sizeof(GovdeDurumu));
            Baslik b{};
            b.sihirli = kSihirliSayi;
            b.tur = static_cast<std::uint16_t>(IletiTuru::durum);
            b.yuk_bayt = static_cast<std::uint32_t>(sizeof(Durum) + govde * sizeof(GovdeDurumu));
            b.sira = sira_++;
            std::memcpy(is_.data() + bas, &b, sizeof b);
            std::memcpy(is_.data() + bas + kBaslikBoyutu, &sabit, sizeof sabit);
            if (govde > 0) {
                std::memcpy(is_.data() + bas + kBaslikBoyutu + sizeof sabit, yuk + sizeof(Durum),
                            govde * sizeof(GovdeDurumu));
            }
            if (kuyruga_(!korunan)) {
                artir(s_.gonderilen_durum);
                sd_var_ = true;
                sd_sonrasi_degisim_ = false;
                sd_nesil_ = nesil_;
                sd_adim_ = d.adim_no;
                sd_cd_ = d.calisma_durumu;
            }
            return;
        }
        case KayitTuru::dalga: {
            if (n < sizeof(DalgaKumesi)) {
                return;
            }
            DalgaKumesi dk{};
            std::memcpy(&dk, yuk, sizeof dk);
            const std::size_t m = dk.bilesen_sayisi;
            if (m > kAzamiDalgaBileseni || n != sizeof dk + m * sizeof(DalgaBileseni) || dk.surum <= dalga_surumu_) {
                return;
            }
            std::array<DalgaBileseni, kAzamiDalgaBileseni> bl;
            if (m > 0) {
                std::memcpy(bl.data(), yuk + sizeof dk, m * sizeof(DalgaBileseni));
            }
            dalga_surumu_ = dk.surum;
            is_.clear();
            kodla_dalga(is_, sira_++, dk, std::span<const DalgaBileseni>(bl.data(), m));
            sd_sonrasi_degisim_ = true;
            (void)kuyruga_(false);
            return;
        }
        case KayitTuru::ortam: {
            if (n != sizeof(Ortam)) {
                return;
            }
            Ortam o{};
            std::memcpy(&o, yuk, sizeof o);
            if (o.ortam_surumu <= ortam_surumu_) {
                return;
            }
            ortam_surumu_ = o.ortam_surumu;
            is_.clear();
            kodla_ortam(is_, sira_++, o);
            sd_sonrasi_degisim_ = true;
            (void)kuyruga_(false);
            return;
        }
        case KayitTuru::malzeme: {
            if (n < sizeof(MalzemeOptik)) {
                return;
            }
            MalzemeOptik mo{};
            std::memcpy(&mo, yuk, sizeof mo);
            const std::size_t k = mo.girdi_sayisi;
            if (k > kAzamiMalzemeOptik || n != sizeof mo + k * sizeof(MalzemeOptikGirdisi) ||
                mo.surum <= malzeme_surumu_) {
                return;
            }
            // 14 KiB'lık dizi yığına konmaz (MSVC C6262): doğrudan halkadan kodlanır.
            malzeme_surumu_ = mo.surum;
            is_.clear();
            const std::size_t yuk_bayt = sizeof mo + k * sizeof(MalzemeOptikGirdisi);
            is_.resize(kBaslikBoyutu + yuk_bayt);
            Baslik b{};
            b.sihirli = kSihirliSayi;
            b.tur = static_cast<std::uint16_t>(IletiTuru::malzeme_optik);
            b.yuk_bayt = static_cast<std::uint32_t>(yuk_bayt);
            b.sira = sira_++;
            std::memcpy(is_.data(), &b, sizeof b);
            std::memcpy(is_.data() + kBaslikBoyutu, yuk, yuk_bayt);
            sd_sonrasi_degisim_ = true;
            (void)kuyruga_(false);
            return;
        }
        case KayitTuru::yeni_calistirma:
            // A2-K96 (3): veda(yeni_calistirma); yeni oturum yeni nesille ve tam el sıkışmayla kurulur.
            veda_(VedaNedeni::yeni_calistirma, KopmaNedeni::yeni_calistirma);
            return;
        case KayitTuru::kapanis:
            return;  // yukarıda işlendi
    }
}

}  // namespace ida::a2::ic
