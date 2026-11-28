// A6 kayıt kitaplığının `ida_sim`'e bağlanması (kayit_baglanti.hpp).
#include "kayit_baglanti.hpp"

#include "a3_baglanti.hpp"
#include "ida/a3/kaynak.hpp"
#include "ida/a3/kodlayici.hpp"
#include "ida/kayit/coz.hpp"
#include "ida/kayit/okuyucu.hpp"
#include "ida/kopru/kopru.hpp"
#include "ida_link/dialect.h"

#include <Eigen/Core>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <system_error>

namespace ida::sunucu {

namespace {

constexpr std::size_t kVarsayilanHalkaBayt = std::size_t{16} << 20;   // tahmin (ölçülmedi)

std::optional<std::string> dosya_oku(const std::filesystem::path& yol)
{
    std::ifstream f(yol, std::ios::binary);
    if (!f) {
        return std::nullopt;
    }
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (f.bad()) {
        return std::nullopt;
    }
    return s;
}

std::string utc_simdi()
{
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char b[32];
    std::snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02dZ", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour,
                  g.tm_min, g.tm_sec);
    return b;
}

std::string platform_adi()
{
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return "windows-x64";
#elif defined(__APPLE__) && defined(__aarch64__)
    return "macos-arm64";
#else
    return "bilinmiyor";   // A6 §3.5 yalnız macos-arm64 ve windows-x64'ü sayar
#endif
}

std::string derleyici_adi()
{
#if defined(_MSC_VER) && !defined(__clang__)
    return "msvc " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("gcc ") + __VERSION__;
#else
    return "bilinmiyor";
#endif
}

// A3 ortak ve A6'ya özgü kanalların bu bağlamada bağlı olanları (üretici çağrısı kodda var).
// `a3_acik`: A3 durum makinesine bağlı kanallar yalnız A3 açıkken üretilir.
bool kanal_bagli(kayit::Kanal k, bool a3_acik, bool sonar_acik)
{
    using K = kayit::Kanal;
    switch (k) {
    case K::fizik_durum:
    case K::sensor_basinc:
    case K::sensor_dvl:
    case K::sensor_batarya:
    case K::kart_durum:
    case K::kart_cekirdek_sayaclari:
    case K::sayac_gercek_zaman:
    case K::emniyet_girdiler:
    case K::atis_durum:
    case K::senaryo_oge_uygulama:
    case K::kayit_eksik:   // yazıcının kendisi üretir (A6-K30)
        return true;
    case K::sonar_ping360:
    case K::sonar_ping1d:
        return sonar_acik;
    case K::olay:
    case K::calistirma_durum:
    case K::ariza_etkin:
    case K::a3_komut_sonucu:
        return a3_acik;
    default:
        return false;
    }
}

std::string json_dizisi(const std::map<std::string, std::string>& m, bool onek)
{
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [k, v] : m) {
        j[k] = onek ? "sha256:" + v : v;
    }
    return j.dump();   // anahtarlar sıralı, boşluksuz; yollar ASCII (RFC 8785 ile aynı dizilim)
}

}  // namespace

OnDenetimRaporu on_denetim_kur(const OnDenetimGirdisi& g)
{
    // Rapor yalnız gerçekten yapılan denetimleri `gecti` gösterir: senaryonun A5 doğrulaması (rapor onsuz
    // kurulamaz) ve kayıt isteniyorsa boş alan ölçümü. Yapılmayanlar şemanın `atlandi` durumuyla ve
    // nedeniyle yazılır; `gecti` uydurulmaz (W5-K50). Bu sim'de A1 el sıkışması bu raporun parçası değildir:
    // rapor çalıştırma başlarken, el sıkışmasından önce üretilir (A3-K62).
    OnDenetimRaporu r;
    r.kimlik = senaryo::sha256_hex(g.run_id).substr(0, 16);
    nlohmann::json maddeler = nlohmann::json::array();
    maddeler.push_back({{"kod", "senaryo_gecerli"},
                        {"durum", "gecti"},
                        {"ileti", g.kayit_istendi
                                      ? "senaryo A5 doğrulamasından geçti; doğrulama raporu dosyada (dogrulama_raporu.json)"
                                      : "senaryo A5 doğrulamasından geçti; doğrulama raporu GET /senaryo/dogrula ile alınır"},
                        {"dayanak", "A5"}});
    if (g.kayit_istendi) {
        std::error_code ec_alan;
        const std::filesystem::space_info alan = std::filesystem::space(g.kayit_dizini, ec_alan);
        const bool olculdu = !ec_alan;
        const std::uint64_t bos = olculdu ? static_cast<std::uint64_t>(alan.available) : 0u;
        if (olculdu && bos < g.disk_siniri_bayt) {
            r.kaldi = true;
            r.kaldi_ileti = "ön denetim kaldı: kayit_disk_alani: " + g.kayit_dizini.generic_string() + " dizininde boş alan " +
                            std::to_string(bos) + " bayt, en az " + std::to_string(g.disk_siniri_bayt) + " bayt gerekir (A3 §3.8)";
            maddeler.push_back({{"kod", "kayit_disk_alani"},
                                {"durum", "kaldi"},
                                {"olculen", bos},
                                {"sinir", g.disk_siniri_bayt},
                                {"birim", "bayt"},
                                {"ileti", "kayıt dizininde yeterli boş alan yok"},
                                {"dayanak", "A3 §3.8; 8. başlık md.6"}});
        } else if (olculdu) {
            maddeler.push_back({{"kod", "kayit_disk_alani"},
                                {"durum", "gecti"},
                                {"olculen", bos},
                                {"sinir", g.disk_siniri_bayt},
                                {"birim", "bayt"},
                                {"ileti", "kayıt dizininde yeterli boş alan"},
                                {"dayanak", "A3 §3.8; 8. başlık md.6"}});
        } else {
            maddeler.push_back({{"kod", "kayit_disk_alani"},
                                {"durum", "uyari"},
                                {"sinir", g.disk_siniri_bayt},
                                {"birim", "bayt"},
                                {"ileti", "boş alan ölçülemedi: " + ec_alan.message()},
                                {"dayanak", "A3 §3.8; 8. başlık md.6"}});
        }
    } else {
        maddeler.push_back({{"kod", "kayit_disk_alani"},
                            {"durum", "atlandi"},
                            {"ileti", "yapılmadı: kayıt kapalı (--kayit-dizini verilmedi); boş alan ölçülmedi"},
                            {"dayanak", "A3 §3.8"}});
    }
    for (const char* kod : {"surum_uyumu", "kontrol_hizi", "a1_bant_payi", "a7_manifest", "ping360_zamanlama", "gorsellestirici",
                            "algilama_bileseni"}) {
        maddeler.push_back({{"kod", kod},
                            {"durum", "atlandi"},
                            {"ileti", "yapılmadı: bu sim bu maddeyi ölçmüyor; rapor çalıştırma başlarken, A1 el sıkışmasından önce üretilir (A3-K62)"},
                            {"dayanak", "A3 §3.8"}});
    }
    bool kaldi = false;
    for (const nlohmann::json& m : maddeler) {
        kaldi = kaldi || m.at("durum") == "kaldi";
    }
    const nlohmann::json rapor = {{"rapor_surumu", "1.0"},
                                  {"rapor_kimligi", r.kimlik},
                                  {"sonuc", kaldi ? "kaldi" : "gecti"},
                                  {"kip", g.kip},
                                  {"olusturma_utc", utc_simdi()},
                                  {"maddeler", maddeler}};
    r.json = rapor.dump();
    return r;
}

bool kayit_girdisi_kur(const senaryo::DogrulanmisSenaryo& cozumlenmis, const senaryo::DogrulamaRaporu& rapor,
                       const senaryo::DosyaKaynagi& kaynak, KayitGirdisi& girdi, std::string& hata)
{
    girdi = KayitGirdisi{};
    const senaryo::Deger& c = cozumlenmis.agac();
    auto dizgi = [&](const char* yol, std::string& hedef) {
        const senaryo::Deger* d = c.yol_bul(yol);
        if (d == nullptr || !d->dizgi_mi()) {
            return false;
        }
        hedef = d->dizgi;
        return true;
    };
    std::string gorunum, kamera, sonar, kayit;
    if (!dizgi("kalite.kayit", kayit) || !dizgi("kalite.gorunum", gorunum) || !dizgi("kalite.kamera", kamera) ||
        !dizgi("kalite.sonar", sonar)) {
        hata = "kalite.* alanları okunamadı";
        return false;
    }
    girdi.kademe = kayit;
    girdi.kademeler = "gorunum=" + gorunum + ",kamera=" + kamera + ",sonar=" + sonar + ",kayit=" + kayit;
    if (!dizgi("meta.sema_surumu", girdi.a5_sema_surumu)) {
        hata = "meta.sema_surumu okunamadı";
        return false;
    }
    if (!dizgi("varliklar.kume", girdi.kume) || girdi.kume.empty()) {
        hata = "varliklar.kume okunamadı";
        return false;
    }
    const std::string yol = "assets/" + girdi.kume + "/manifest.json";
    const std::optional<std::string> bayt = kaynak.oku(yol);
    if (!bayt) {
        hata = "A7 manifesti okunamadı: " + yol;
        return false;
    }
    girdi.manifest = *bayt;
    girdi.manifest_ozeti = senaryo::sha256_hex(girdi.manifest);
    const auto ro = rapor.kaynak_ozetleri.find(yol);
    if (ro == rapor.kaynak_ozetleri.end() || ro->second != girdi.manifest_ozeti) {
        hata = "A7 manifesti doğrulama raporundaki özetle aynı değil (A5-K115): " + yol;
        return false;
    }
    const nlohmann::json m = nlohmann::json::parse(girdi.manifest, nullptr, false);
    if (!m.is_object() || !m.contains("kume_surumu") || !m["kume_surumu"].is_number_unsigned()) {
        hata = "A7 manifestinin kume_surumu okunamadı: " + yol;
        return false;
    }
    girdi.kume_surumu = std::to_string(m["kume_surumu"].get<std::uint64_t>());
    if (!rapor.davranis_ozeti || rapor.davranis_ozeti->size() != 64u) {
        hata = "davranış özeti yok";
        return false;
    }
    girdi.davranis_ozeti = *rapor.davranis_ozeti;
    girdi.kaynak_ozetleri = rapor.kaynak_ozetleri;
    girdi.kanonik = cozumlenmis.kanonik();
    girdi.dogrulama_raporu = rapor.json();
    return true;
}

std::unique_ptr<KayitBaglanti> KayitBaglanti::ac(const KayitBaglantiAyari& ayar, std::string& hata)
{
    kayit::Kademe kademe{};
    if (!kayit::kademe_coz(ayar.girdi.kademe, kademe)) {
        hata = "kalite.kayit geçersiz: " + ayar.girdi.kademe;
        return nullptr;
    }
    if (!kayit::run_id_gecerli(ayar.run_id)) {
        hata = "run_id geçersiz: " + ayar.run_id;
        return nullptr;
    }
    // ---- ekler (A6 §3.6; sim'in elinde olanlar, değiştirilmeden) ----
    const std::filesystem::path kok = ayar.depo_koku.empty() ? std::filesystem::path(".") : std::filesystem::path(ayar.depo_koku);
    const std::optional<std::string> komutlar = dosya_oku(kok / "protocol" / "komutlar.json");
    const std::optional<std::string> dialect = dosya_oku(kok / "protocol" / "ida.xml");
    if (!komutlar || !dialect) {
        hata = "A1 ekleri okunamadı (protocol/komutlar.json, protocol/ida.xml; --kok: " + kok.generic_string() + ")";
        return nullptr;
    }
    // Dizin ve boş alan (A3 §3.8 `kayit_disk_alani`): ölçülür, sınırın altındaysa çalıştırma başlamaz.
    std::error_code ec;
    std::filesystem::create_directories(ayar.dizin, ec);
    if (ec) {
        hata = "kayıt dizini oluşturulamadı: " + ayar.dizin.generic_string() + ": " + ec.message();
        return nullptr;
    }
    // Ön denetim raporu (A3 §3.8): tek üretici `on_denetim_kur`dur (W5-a3-senaryo, W5-K49): kayıt eki ile
    // `GET /on_denetim` aynı işlevden gelir.
    OnDenetimGirdisi odg;
    odg.run_id = ayar.run_id;
    odg.kip = ayar.kip;
    odg.kayit_istendi = true;
    odg.kayit_dizini = ayar.dizin;
    odg.disk_siniri_bayt = ayar.disk_siniri_bayt;
    OnDenetimRaporu od = on_denetim_kur(odg);
    if (od.kaldi) {
        hata = od.kaldi_ileti;
        return nullptr;
    }
    const std::string on_denetim_json = od.json;
    const auto ek = [](const char* ad, const std::string& s) {
        return kayit::Ek{ad, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size())};
    };
    const std::vector<kayit::Ek> ekler = {
        ek("cozumlenmis_senaryo.json", ayar.girdi.kanonik), ek("dogrulama_raporu.json", ayar.girdi.dogrulama_raporu),
        ek("a1_komutlar.json", *komutlar),                  ek("a1_dialect.xml", *dialect),
        ek("a7_manifest.json", ayar.girdi.manifest),        ek("on_denetim_raporu.json", on_denetim_json)};

    // ---- açılış üst verisi (A6 §3.5): elde olanlar; olmayanlar "bilinmiyor" (zorunlu anahtar boş olamaz) ----
    kayit::AcilisUstVerisi uv;
    uv.a3_surumu = "1.0";
    // Köprünün SIM_HELLO'da bildirdiği (kopru.cpp: proto_major 1, proto_minor 0); köprüde adlandırılmış sabit yok.
    uv.a1_proto_surumu = "1.0";
    uv.a1_dialect_hash = std::to_string(static_cast<std::uint64_t>(IDA_LINK_DIALECT_HASH));
    uv.a5_sema_surumu = ayar.girdi.a5_sema_surumu;
    uv.a7_manifest_ozeti = "sha256:" + ayar.girdi.manifest_ozeti;
    uv.a7_kume = ayar.girdi.kume;
    uv.a7_kume_surumu = ayar.girdi.kume_surumu;
    uv.a2_port = ayar.a2_port;
    uv.run_id = ayar.run_id;
    uv.a1_run_id = ayar.a1_run_id;
    uv.sil_n_bg = ayar.kip == "sil" ? ayar.sil_n_bg : std::string();
    uv.kip = ayar.kip;
    uv.kademeler = ayar.girdi.kademeler;
    uv.tohum = ayar.tohum;
    uv.senaryo_ozeti = "sha256:" + ayar.girdi.davranis_ozeti;
    uv.senaryo_kaynak_ozetleri = json_dizisi(ayar.girdi.kaynak_ozetleri, true);
    uv.sim_surumu = "bilinmiyor";           // derleme kimliği (git commit, kirli bayrağı) derlemeye gömülü değil
    uv.platform = platform_adi();
    uv.derleyici = derleyici_adi();
    uv.derleme_bayraklari = "bilinmiyor";   // fizik yolunu etkileyen bayraklar (04 §2.13) derlemeye gömülü değil
    uv.rng_surumu = "bilinmiyor";           // RNG ve dağılım kodu sürümü tanımlı değil
    uv.dalga_listesi_ozeti = "bilinmiyor";  // 04 dalga listesi özeti tanımlı değil
    uv.bagimliliklar = "eigen=" + std::to_string(EIGEN_WORLD_VERSION) + "." + std::to_string(EIGEN_MAJOR_VERSION) + "." +
                       std::to_string(EIGEN_MINOR_VERSION);   // fizik çekirdeği Eigen'e bağlıdır; mcap ve zstd'yi yazıcı ekler
    uv.kontrol_hizi_hz = std::to_string(std::int64_t{1'000'000'000} / kopru::kP);
    uv.fizik_adimi_ns = std::to_string(kopru::kH);
    uv.l_act_ns = std::to_string(kopru::kLact);
    uv.baslangic_utc = utc_simdi();

    kayit::Ayar ya;
    ya.dizin = ayar.dizin;
    ya.kademe = kademe;
    ya.ureticiler.push_back({"dunya", ayar.halka_bayt != 0 ? ayar.halka_bayt : kVarsayilanHalkaBayt});
    ya.is_parcacigi = true;
    ya.ilk_sira = ayar.ilk_sira;

    std::unique_ptr<kayit::Yazici> y = kayit::Yazici::ac(ya, uv, ekler, hata);
    if (!y) {
        return nullptr;
    }
    std::unique_ptr<KayitBaglanti> b(new KayitBaglanti());
    b->kip_ = ayar.kip;
    b->u_ = &y->uretici(0);
    b->y_ = std::move(y);
    b->a3_acik_ = ayar.a3_acik;
    b->sonar_acik_ = ayar.sonar_acik;
    b->bilinmeyen_ = bilinmeyen_anahtarlar(uv);
    b->on_denetim_json_ = on_denetim_json;
    b->on_denetim_kimligi_ = od.kimlik;
    b->guvence_siniri_ = b->guvence_hesapla_();   // kanal bağlantıları ve üst veri çalıştırma boyunca değişmez
    // W5-K41: tavanlı ilk kapanış taslağı, ilk üretici çağrısından ÖNCE. Dosya erken kapanırsa (disk dolu,
    // yazma hatası, sınır, yıkıcı) kapanış kaydı bu taslağı kullanır; taslak yazılmadan kapanan dosya
    // tavansız (kademe ve kipin verdiği) güvence yazardı.
    kayit::KapanisAlanlari ilk;
    ilk.bitis_durumu = kayit::BitisDurumu::hata;
    b->taslak_guncelle(ilk);
    return b;
}

KayitBaglanti::~KayitBaglanti() = default;

void KayitBaglanti::taslak_guncelle(kayit::KapanisAlanlari a)
{
    tavan_uygula(eksik_kanallar(true), bilinmeyen_, a);   // erken kapanışta da aynı tavan (W5-K41)
    y_->kapanis_taslagi(a);
}

kayit::KapanisSonucu KayitBaglanti::kapat(kayit::KapanisAlanlari alanlar)
{
    if (kapandi_) {
        return sonuc_;
    }
    kapandi_ = true;
    tavan_uygula(eksik_kanallar(true), bilinmeyen_, alanlar);
    sonuc_ = y_->kapat(alanlar);
    return sonuc_;
}

std::vector<std::string> KayitBaglanti::uretilen_kanallar() const
{
    std::vector<std::string> r;
    const auto k = kayit::kanallar();
    for (std::size_t i = 0; i < k.size(); ++i) {
        // Üretici çağrısı alan ve kademe/kipe göre dosyaya yazılan (süzülenler dosyada yok).
        if (uretildi_[i] && kayit::kanal_etkin(k[i], y_->kademe(), kip_)) {
            r.emplace_back(k[i].ad);
        }
    }
    return r;
}

std::vector<std::string> KayitBaglanti::eksik_kanallar(bool yalniz_tekrar_duzeyi) const
{
    std::vector<std::string> r;
    kayit::Kademe kademe = y_->kademe();
    for (const kayit::KanalTanimi& k : kayit::kanallar()) {
        if (!kayit::kanal_etkin(k, kademe, kip_)) {
            continue;
        }
        if (k.ad == "kamera/kare") {
            continue;   // W5-K37: bu dilimde kanal tablolarda yok; `kamera_kareleri` kapali
        }
        if (kanal_bagli(static_cast<kayit::Kanal>(k.indeks), a3_acik_, sonar_acik_)) {
            continue;
        }
        if (yalniz_tekrar_duzeyi && (k.tekrar_duzeyleri & (kayit::tekrar::fizik | kayit::tekrar::girdi)) == 0) {
            continue;
        }
        r.emplace_back(k.ad);
    }
    return r;
}

void KayitBaglanti::tavan_uygula(const std::vector<std::string>& eksik, const std::vector<std::string>& bilinmeyen,
                                 kayit::KapanisAlanlari& a)
{
    if (eksik.empty() && bilinmeyen.empty()) {
        return;
    }
    // W5-K41: düzeyin gerektirdiği kanalların hepsi üretilmiyor ya da zorunlu üst veri anahtarı bilinmiyor
    // (W5-K39: tekrar için gereken sürüm kimliği yok) → dosya `fizik` ya da `girdi` iddia edemez.
    a.guvence_tavani = kayit::Guvence::izleme;
    std::string n;
    if (!eksik.empty()) {
        n = "kanal_uretilmiyor:";
        for (std::size_t i = 0; i < eksik.size(); ++i) {
            n += (i == 0 ? "" : "+") + eksik[i];
        }
    }
    if (!bilinmeyen.empty()) {
        n += n.empty() ? "" : ",";
        n += "ust_veri_bilinmiyor:";
        for (std::size_t i = 0; i < bilinmeyen.size(); ++i) {
            n += (i == 0 ? "" : "+") + bilinmeyen[i];
        }
    }
    a.guvence_tavan_nedeni = n;
}

std::vector<std::string> KayitBaglanti::bilinmeyen_anahtarlar(const kayit::AcilisUstVerisi& uv)
{
    std::vector<std::string> r;
    for (const auto& [anahtar, deger] : kayit::ust_veri_ciftleri(uv)) {
        if (deger == "bilinmiyor") {
            r.emplace_back(anahtar);
        }
    }
    return r;
}

kayit::Guvence KayitBaglanti::guvence_hesapla_() const
{
    kayit::KapanisAlanlari a;
    tavan_uygula(eksik_kanallar(true), bilinmeyen_, a);
    const kayit::Guvence beklenen = kayit::beklenen_duzey(y_->kademe(), kip_);
    return a.guvence_tavani.has_value() && *a.guvence_tavani < beklenen ? *a.guvence_tavani : beklenen;
}

std::string kayit_sayac_listesi(const a3::yuk_sayac_gercek_zaman& y)
{
    const kayit::KanalTanimi* k = kayit::kanal_bul("sayac/gercek_zaman");
    if (k == nullptr || k->yuk == nullptr) {
        return {};
    }
    std::vector<std::uint8_t> bayt(a3::boyut(y));
    std::size_t yazilan = 0;
    if (!a3::kodla(y, bayt.data(), bayt.size(), yazilan)) {
        return {};
    }
    std::string s;
    (void)kayit::alanlari_gez(*k->yuk, std::span<const std::uint8_t>(bayt.data(), yazilan),
                              [&](const a3::AlanTanimi& alan, const kayit::AlanDegeri& d) {
                                  std::string v;
                                  switch (d.tur) {
                                  case kayit::AlanDegeri::Tur::isaretsiz: v = std::to_string(d.isaretsiz); break;
                                  case kayit::AlanDegeri::Tur::isaretli: v = std::to_string(d.isaretli); break;
                                  case kayit::AlanDegeri::Tur::kayan: {
                                      char b[48];
                                      std::snprintf(b, sizeof b, "%.9g", d.kayan);
                                      v = b;
                                      break;
                                  }
                                  case kayit::AlanDegeri::Tur::metin: v = std::string(d.metin); break;
                                  case kayit::AlanDegeri::Tur::baytlar: return;
                                  }
                                  if (!s.empty()) {
                                      s += ',';
                                  }
                                  s += std::string(alan.ad) + "=" + v;
                              });
    return s;
}

}  // namespace ida::sunucu
