// test_bicim.cpp — kabul 1 (biçim), 2 (gidiş-dönüş, zaman alanları, sıra) ve 3 (kademe).
#include "test_destek.hpp"

#include "ida/kayit/coz.hpp"
#include "mcap_kitaplik_okuma.hpp"
#include "sinama_uretim.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <tuple>

namespace ida::kayit::test {

namespace {

using nlohmann::json;

json katalog()
{
    const std::vector<std::uint8_t> b = dosya_oku(IDA_A6_KATALOG_YOLU);
    return json::parse(b.begin(), b.end());
}

std::string yol_metni(const std::filesystem::path& p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

// Tablo 3.3'e göre kanal bu kademe ve kipte yazılır mı: katalog JSON'ından, üretilen tablolardan
// bağımsız hesap (A6-K16; `kamera/kare` bu dilimde yok: W5-K37).
bool beklenen_etkin(const json& kn, Kademe kademe, std::string_view kip)
{
    const std::string ad = kn["ad"].get<std::string>();
    if (ad == "kamera/kare") {
        return false;
    }
    const std::string k = kn["kademe"].get<std::string>();
    const int sira = k == "olaylar" ? 0 : (k == "telemetri" ? 1 : 2);
    if (sira > static_cast<int>(kademe)) {
        return false;
    }
    if (!kn["kosul"].is_null()) {
        // `cekirdek/*`: yalnız çekirdeği PC'de çağıran kiplerde.
        return kip == "sil" || kip == "tekrar_girdi_sil" || kip == "tekrar_yeniden_kurma";
    }
    return true;
}

// `olaylar` kademesinde süzgeçten geçen örnekler.
YazmaSonucu bariyer_yaz(Uretici& u, std::uint8_t karar, std::int64_t t, std::uint64_t adim)
{
    yuk_hil_bariyer y;
    y.step_seq = static_cast<std::uint32_t>(adim);
    y.kapanis_adimi = adim;
    y.uygulama_adimi = adim + 8;
    y.karar = karar;
    y.kip = 3;
    return u.yaz<Kanal::hil_bariyer>(t, adim, y);
}

YazmaSonucu parti_yaz(Uretici& u, std::uint8_t nihai, std::uint8_t ret, std::uint8_t gec, std::int64_t t, std::uint64_t adim)
{
    a3::yuk_algilama_parti y;
    y.kaynak = 1;
    y.batch_seq = static_cast<std::uint32_t>(adim);
    y.nihai_durum = nihai;
    y.ret_nedeni = ret;
    y.gec = gec;
    return u.yaz<Kanal::algilama_parti>(t, adim, y);
}

}  // namespace

// Kabul 1: yazılan dosya `mcap` kitaplığının kendi okuyucusuyla hatasız açılır; sihirli baytlar,
// `profile`, şema ve kodlama adları §3.2'deki gibi; şema `data`'sı kataloğun kanonik JSON'ı; kanal
// `metadata`'sı katalogla aynı.
int test_bicim()
{
    Denetci d;
    const json k = katalog();
    Duzenek dz("bicim", Kademe::tam, "sil", [](Ayar& a) { a.parca_hedefi_bayt = 32768; });
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    std::multiset<std::tuple<std::string, std::uint64_t, std::vector<std::uint8_t>>> beklenen;
    constexpr int kTur = 12;
    for (int tur = 0; tur < kTur; ++tur) {
        for (const KanalTanimi& kn : kanallar()) {
            const std::uint64_t tohum = static_cast<std::uint64_t>(tur) * 1000u + kn.indeks;
            const std::int64_t t = static_cast<std::int64_t>(tur) * 5'000'000 + kn.indeks;
            if (kn.ad == "kayit/eksik") {
                continue;  // yalnız kayıt yazıcısı yazar
            }
            const YazmaSonucu s = ornek_yaz(dz.u(), static_cast<Kanal>(kn.indeks), tohum, t, static_cast<std::uint64_t>(tur));
            DOGRULA(d, s == YazmaSonucu::yazildi, std::string(kn.ad) + ": yazılamadı");
            beklenen.emplace(std::string(kn.ad), static_cast<std::uint64_t>(t), ornek_uret(static_cast<Kanal>(kn.indeks), tohum).bayt);
        }
        dz.saat.ilerlet(5'000'000);
        dz.y->isle();
    }
    const KapanisSonucu ks = dz.kapat();
    DOGRULA(d, ks.dosya_kapandi && ks.hata.empty(), "dosya kapandı: " + ks.hata);
    DOGRULA(d, ks.sayaclar.yazilan_ileti == beklenen.size() && ks.sayaclar.eksik_kaydi == 0, "yazılan ileti sayısı");

    const std::filesystem::path yol = dz.y->yol();
    DOGRULA(d, yol.filename() == std::string(kOrnekRunId) + ".mcap", "dosya adı <run_id>.mcap");
    const std::vector<std::uint8_t> bayt = dosya_oku(yol);
    static constexpr std::uint8_t kSihir[] = {0x89, 'M', 'C', 'A', 'P', 0x30, '\r', '\n'};
    DOGRULA(d, bayt.size() > 16 && std::equal(std::begin(kSihir), std::end(kSihir), bayt.begin()), "baştaki sihirli baytlar");
    DOGRULA(d, bayt.size() > 16 && std::equal(std::begin(kSihir), std::end(kSihir), bayt.end() - 8), "sondaki sihirli baytlar");

    const KitaplikDokumu kd = kitaplik_ile_oku(yol_metni(yol));
    DOGRULA(d, kd.acildi, "mcap kitaplığının okuyucusu dosyayı açtı: " + kd.hata);
    DOGRULA(d, kd.sorunlar.empty(), "kitaplık sorun bildirmedi" + (kd.sorunlar.empty() ? std::string() : ": " + kd.sorunlar[0]));
    DOGRULA(d, kd.profil == "ida", "Header profile = ida");
    DOGRULA(d, kd.kitaplik == "libmcap 2.1.3", "Header library = " + kd.kitaplik);

    // Şemalar: ad "ida.<yük>.v<n>", kodlama "ida-yuk-v1", data = kataloğun kanonik JSON'ı.
    std::map<std::uint16_t, const KitaplikSema*> sema_kimlik;
    for (const KitaplikSema& s : kd.semalar) {
        sema_kimlik[s.id] = &s;
        DOGRULA(d, s.id != 0, "şema kimliği sıfır olamaz");
        DOGRULA(d, s.kodlama == "ida-yuk-v1", s.ad + ": şema kodlaması");
        const bool bicim = s.ad.rfind("ida.yuk_", 0) == 0 && s.ad.size() > 7 && s.ad.substr(s.ad.size() - 3) == ".v1";
        DOGRULA(d, bicim, s.ad + ": şema adı biçimi");
        if (bicim) {
            const std::string yuk = s.ad.substr(4, s.ad.size() - 7);
            DOGRULA(d, k["x_ida_yukler"].contains(yuk) && s.veri == k["x_ida_yukler"][yuk].dump(),
                    s.ad + ": şema data'sı kataloğun kanonik JSON'ıyla bayt bayt aynı");
        }
    }
    // Kanallar: katalogdaki her kanal (kamera/kare hariç), metadata = {kanal_surumu, zaman_anlami, kademe}.
    std::set<std::string> konular;
    for (const KitaplikKanal& c : kd.kanallar) {
        konular.insert(c.konu);
        DOGRULA(d, c.ileti_kodlamasi == "ida-ikili-v1", c.konu + ": ileti kodlaması");
        const auto kn = std::find_if(k["x_ida_kanallar"].begin(), k["x_ida_kanallar"].end(),
                                     [&](const json& x) { return x["ad"].get<std::string>() == c.konu; });
        DOGRULA(d, kn != k["x_ida_kanallar"].end(), c.konu + ": katalogda olmayan kanal");
        if (kn == k["x_ida_kanallar"].end()) {
            continue;
        }
        const std::map<std::string, std::string> beklenen_metadata = {
            {"kanal_surumu", std::to_string((*kn)["kanal_surumu"].get<int>())},
            {"zaman_anlami", (*kn)["zaman_anlami"].get<std::string>()},
            {"kademe", (*kn)["kademe"].get<std::string>()},
        };
        DOGRULA(d, c.metadata == beklenen_metadata, c.konu + ": kanal metadata'sı katalogla aynı");
        const auto s = sema_kimlik.find(c.sema_id);
        DOGRULA(d, s != sema_kimlik.end() &&
                       s->second->ad == "ida." + (*kn)["yuk"].get<std::string>() + ".v" +
                                            std::to_string((*kn)["kanal_surumu"].get<int>()),
                c.konu + ": kanalın şeması");
    }
    DOGRULA(d, konular.size() == kKanalSayisi && kd.kanallar.size() == kKanalSayisi, "kanal sayısı: " + std::to_string(konular.size()));
    DOGRULA(d, konular.count("kamera/kare") == 0, "kamera/kare yok (W5-K37)");

    // İletiler: kitaplığın okuduğu her ileti üretilenle bayt bayt aynı; sequence 1…N.
    DOGRULA(d, kd.istatistik_ileti == beklenen.size() && kd.iletiler.size() == beklenen.size(), "ileti sayısı (kitaplık)");
    std::multiset<std::tuple<std::string, std::uint64_t, std::vector<std::uint8_t>>> okunan;
    std::set<std::uint32_t> siralar;
    for (const KitaplikIleti& i : kd.iletiler) {
        okunan.emplace(i.konu, i.log_time, i.veri);
        siralar.insert(i.sira);
    }
    DOGRULA(d, okunan == beklenen, "kitaplığın okuduğu iletiler üretilenlerle aynı");
    DOGRULA(d, siralar.size() == beklenen.size() && *siralar.begin() == 1 && *siralar.rbegin() == beklenen.size(),
            "sequence 1'den başlar ve boşluksuzdur");

    // Parçalar: zstd (kazanç yoksa sıkıştırmasız), CRC yazılmış; özetle tutarlı.
    DOGRULA(d, kd.parcalar.size() >= 3 && kd.parcalar.size() == ks.sayaclar.kapanan_parca && kd.istatistik_parca == kd.parcalar.size(),
            "parça sayısı: " + std::to_string(kd.parcalar.size()));
    bool zstd_var = false;
    std::uint64_t en_buyuk = 0;
    for (const KitaplikParca& p : kd.parcalar) {
        DOGRULA(d, p.sikistirma == "zstd" || p.sikistirma.empty(), "parça sıkıştırması zstd ya da yok: " + p.sikistirma);
        zstd_var = zstd_var || p.sikistirma == "zstd";
        en_buyuk = std::max(en_buyuk, p.acik_boy);
    }
    // Hedef (burada 32 KiB) en çok bir kayıt kadar aşılır: bir ileti ve, kanalın ilk iletisiyse, şema
    // ve kanal kaydı (en büyük şema ≈ 12 KiB).
    DOGRULA(d, en_buyuk < 32768u + 16384u, "parça hedefi bir kayıt kadar aşılabilir: " + std::to_string(en_buyuk));
    DOGRULA(d, zstd_var, "en az bir parça zstd ile sıkıştırıldı");

    // Üst veri: açılış kaydı ilk, kapanış kaydı son.
    DOGRULA(d, kd.ust_veri.size() == 2 && kd.ust_veri.front().first == "ida_kayit" && kd.ust_veri.back().first == "ida_kayit_kapanis",
            "üst veri kayıtları");
    if (kd.ust_veri.size() == 2) {
        const auto& a = kd.ust_veri.front().second;
        DOGRULA(d, a.at("a6_surumu") == "1.0" && a.at("run_id") == kOrnekRunId && a.at("kayit_kademesi") == "tam" &&
                       a.at("kamera_kareleri") == "kapali" && a.at("kip") == "sil",
                "açılış kaydının yazıcının doldurduğu alanları");
        DOGRULA(d, a.at("bagimliliklar").find("mcap=2.1.3") != std::string::npos &&
                       a.at("bagimliliklar").find("zstd=") != std::string::npos && a.at("bagimliliklar").find("eigen=5.0.1") == 0,
                "bagimliliklar mcap ve zstd sürümünü taşır: " + a.at("bagimliliklar"));
        std::string hata;
        UstVeri u(a.begin(), a.end());
        DOGRULA(d, ust_veri_denetle(kUstVeriAcilis, u, hata), "açılış kaydı şemaya uyuyor: " + hata);
        UstVeri kp(kd.ust_veri.back().second.begin(), kd.ust_veri.back().second.end());
        DOGRULA(d, ust_veri_denetle(kUstVeriKapanis, kp, hata), "kapanış kaydı şemaya uyuyor: " + hata);
        DOGRULA(d, kp.at("ileti_sayisi") == std::to_string(beklenen.size()) && kp.at("bitis_durumu") == "bitti" &&
                       kp.at("kayit_eksik") == "false" && kp.at("tekrar_guvencesi") == "girdi",
                "kapanış kaydı");
    }
    // Ekler: katalogdaki adlar ve ortam türleri; `a6_kanallar.json` kataloğun kendisi (A6-K50).
    DOGRULA(d, kd.ekler.size() == ekler().size(), "ek sayısı: " + std::to_string(kd.ekler.size()));
    for (const EkTanimi& t : ekler()) {
        const auto e = std::find_if(kd.ekler.begin(), kd.ekler.end(), [&](const KitaplikEk& x) { return x.ad == t.ad; });
        DOGRULA(d, e != kd.ekler.end() && e->ortam_turu == t.ortam_turu, "ek: " + std::string(t.ad));
        if (e == kd.ekler.end()) {
            continue;
        }
        if (t.ad == kKatalogEki) {
            DOGRULA(d, e->veri == dosya_oku(IDA_A6_KATALOG_YOLU), "a6_kanallar.json eki katalog dosyasıyla aynı");
        } else {
            const auto v = std::find_if(dz.ekler.icerik.begin(), dz.ekler.icerik.end(),
                                        [&](const auto& x) { return x.first == t.ad; });
            DOGRULA(d, v != dz.ekler.icerik.end() && v->second == e->veri, "ek içeriği değişmeden: " + std::string(t.ad));
        }
    }

    // A6 okuyucusu aynı dosyada aynı sonucu verir.
    const auto o = dz.oku();
    DOGRULA(d, o != nullptr, "A6 okuyucusu açtı");
    if (o) {
        const DosyaOzeti& oz = o->ozet();
        DOGRULA(d, oz.saglam() && oz.uyarilar.empty(), "dosya sağlam ve uyarısız" + (oz.uyarilar.empty() ? std::string() : ": " + oz.uyarilar[0]));
        DOGRULA(d, oz.ileti_sayisi == beklenen.size() && oz.okunur_ileti_sayisi == beklenen.size() &&
                       oz.parca_sayisi == kd.parcalar.size() && oz.kanallar.size() == kKanalSayisi - 1,
                "okuyucu özeti (iletisi olan kanallar: kayit/eksik dışında hepsi)");
        DOGRULA(d, oz.ust_veri_gecerli && oz.kademe_var && oz.kademe == Kademe::tam && oz.guvence == Guvence::girdi &&
                       oz.kitaplik == "libmcap 2.1.3" && oz.ekler.size() == ekler().size(),
                "üst veri, kademe, güvence");
        std::vector<std::uint8_t> ek;
        DOGRULA(d, o->ek_oku(kKatalogEki, ek) && ek == dosya_oku(IDA_A6_KATALOG_YOLU), "ek okuma");
        DOGRULA(d, !o->ek_oku("olmayan_ek", ek), "olmayan ek");
    }
    return d.hata;
}

// Kabul 2: her kanalın yükü için yaz → oku bayt eşitliği ve alan değerleri.
int test_gidis_donus()
{
    Denetci d;
    Duzenek dz("gidis_donus", Kademe::tam, "sil", [](Ayar& a) { a.parca_hedefi_bayt = 65536; });
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    struct Beklenen {
        std::string kanal;
        std::int64_t t;
        Ornek ornek;
    };
    std::vector<Beklenen> beklenen;
    constexpr int kTur = 30;
    for (int tur = 0; tur < kTur; ++tur) {
        for (const KanalTanimi& kn : kanallar()) {
            if (kn.ad == "kayit/eksik") {
                continue;  // yalnız kayıt yazıcısı yazar (eksik_sayim testinde okunur)
            }
            const std::uint64_t tohum = 500000u + static_cast<std::uint64_t>(tur) * 977u + kn.indeks * 13u;
            // Zaman her iletide artar: okuma sırası yazım sırasıdır.
            const std::int64_t t = static_cast<std::int64_t>(beklenen.size()) * 1000;
            DOGRULA(d, ornek_yaz(dz.u(), static_cast<Kanal>(kn.indeks), tohum, t, static_cast<std::uint64_t>(tur)) == YazmaSonucu::yazildi,
                    std::string(kn.ad) + ": yazılamadı");
            beklenen.push_back(Beklenen{std::string(kn.ad), t, ornek_uret(static_cast<Kanal>(kn.indeks), tohum)});
        }
        dz.y->isle();
    }
    DOGRULA(d, dz.kapat().dosya_kapandi, "dosya kapandı");
    const auto o = dz.oku();
    DOGRULA(d, o != nullptr && o->ozet().saglam(), "dosya sağlam");
    if (!o) {
        return d.hata;
    }
    Ileti i;
    std::size_t n = 0;
    std::string json;
    std::set<std::string> gorulen;
    while (o->sonraki(i)) {
        if (n < beklenen.size()) {
            const Beklenen& b = beklenen[n];
            DOGRULA(d, i.kanal->ad == b.kanal && i.log_time == b.t, "sıra ve kanal: " + b.kanal);
            DOGRULA(d, i.veri.size() == b.ornek.bayt.size() && std::equal(i.veri.begin(), i.veri.end(), b.ornek.bayt.begin()),
                    b.kanal + ": baytlar aynı");
            const CozmeSonucu c = json_dok(*i.kanal->yuk, i.veri, json);
            DOGRULA(d, c.tamam && json == b.ornek.json, b.kanal + ": alan değerleri aynı");
            DOGRULA(d, i.sequence == n + 1 && i.kanal_surumu == i.kanal->kanal_surumu, b.kanal + ": sequence ve sürüm");
            gorulen.insert(b.kanal);
        }
        ++n;
    }
    DOGRULA(d, o->okuma_hatasi().empty(), "okuma hatasız: " + o->okuma_hatasi());
    DOGRULA(d, n == beklenen.size(), "okunan ileti sayısı: " + std::to_string(n) + " / " + std::to_string(beklenen.size()));
    DOGRULA(d, gorulen.size() == kKanalSayisi - 1, "her kanal okundu");
    return d.hata;
}

// A6-K09, K13: `log_time` = verilen sim zamanı; `publish_time` = üretildiği andaki kayıt duvar saati
// (açılışta 0); `sequence` = dosyadaki yazım sırası, 1'den.
int test_zaman_alanlari()
{
    Denetci d;
    Duzenek dz("zaman_alanlari", Kademe::telemetri, "hil_bariyer");
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return d.hata;
    }
    DOGRULA(d, dz.y->duvar_ns() == 0 && dz.u().duvar_ns() == 0, "kayıt duvar saati açılışta 0");
    a3::yuk_calistirma_durum y;
    constexpr int kAdet = 50;
    for (int i = 0; i < kAdet; ++i) {
        dz.saat.ilerlet(1'000'000);  // her iletiden önce 1 ms
        if (i % 2 == 0) {
            DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(i * 5'000'000LL, static_cast<std::uint64_t>(i), y) == YazmaSonucu::yazildi, "yaz");
        } else {
            // `publish_time`'ı üretici verir.
            DOGRULA(d, dz.u().yaz<Kanal::calistirma_durum>(i * 5'000'000LL, static_cast<std::uint64_t>(i), y, 777'000 + i) == YazmaSonucu::yazildi, "yaz");
        }
    }
    DOGRULA(d, dz.u().duvar_ns() == kAdet * 1'000'000LL, "kayıt duvar saati ilerler");
    DOGRULA(d, dz.kapat().dosya_kapandi, "dosya kapandı");
    const auto o = dz.oku();
    DOGRULA(d, o != nullptr, "okuyucu");
    if (!o) {
        return d.hata;
    }
    const std::vector<OkunanIleti> v = hepsini_oku(*o);
    DOGRULA(d, v.size() == static_cast<std::size_t>(kAdet), "ileti sayısı");
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto n = static_cast<std::int64_t>(i);
        DOGRULA(d, v[i].log_time == n * 5'000'000, "log_time = sim zamanı");
        DOGRULA(d, v[i].sequence == i + 1, "sequence 1'den artar");
        const std::int64_t beklenen = i % 2 == 0 ? (n + 1) * 1'000'000 : 777'000 + n;
        DOGRULA(d, v[i].publish_time == beklenen, "publish_time = kayıt duvar saati: " + std::to_string(v[i].publish_time));
    }
    return d.hata;
}

// A6-K11: dosya sırası `log_time` sırası değilken okuyucu `log_time`, eşitlikte `sequence` sırasıyla verir.
int test_sira_karisik()
{
    Denetci d;
    {
        Duzenek dz("sira_karisik", Kademe::telemetri, "hil_bariyer", [](Ayar& a) { a.parca_hedefi_bayt = 1500; });
        DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
        if (!dz.y) {
            return d.hata;
        }
        // Zamanlar karışık: bazı kanallar geriden gelir (ölçüm zamanı), bazı iletiler aynı zamanı taşır.
        Rastgele r(20261125);
        struct Yazilan {
            std::int64_t t;
            std::uint32_t sira;
            std::string kanal;
        };
        std::vector<Yazilan> yazilan;
        const Kanal secenekler[] = {Kanal::sensor_imu, Kanal::fizik_durum, Kanal::calistirma_durum, Kanal::olay,
                                    Kanal::sensor_basinc, Kanal::hil_tf_yazim};
        constexpr int kAdet = 3000;
        for (int i = 0; i < kAdet; ++i) {
            const Kanal kn = secenekler[r.alt(std::size(secenekler))];
            // Tabanı ilerleyen, ±200 ms saçılan zaman; her 10 iletiden biri öncekiyle aynı zamanda.
            std::int64_t t = static_cast<std::int64_t>(i) * 1'000'000 + static_cast<std::int64_t>(r.alt(400'000'000));
            if (i % 10 == 9 && !yazilan.empty()) {
                t = yazilan.back().t;
            }
            DOGRULA(d, ornek_yaz(dz.u(), kn, static_cast<std::uint64_t>(i), t, static_cast<std::uint64_t>(i)) == YazmaSonucu::yazildi, "yaz");
            yazilan.push_back(Yazilan{t, static_cast<std::uint32_t>(i + 1), std::string(kanallar()[static_cast<std::size_t>(kn)].ad)});
            if (i % 64 == 0) {
                dz.y->isle();
            }
        }
        const KapanisSonucu ks = dz.kapat();
        DOGRULA(d, ks.dosya_kapandi && ks.sayaclar.kapanan_parca > 50, "çok parçalı dosya: " + std::to_string(ks.sayaclar.kapanan_parca));
        const auto o = dz.oku();
        DOGRULA(d, o != nullptr && o->ozet().saglam(), "dosya sağlam");
        if (!o) {
            return d.hata;
        }
        std::stable_sort(yazilan.begin(), yazilan.end(), [](const Yazilan& a, const Yazilan& b) {
            return a.t != b.t ? a.t < b.t : a.sira < b.sira;
        });
        const std::vector<OkunanIleti> v = hepsini_oku(*o);
        DOGRULA(d, o->okuma_hatasi().empty(), "okuma hatasız: " + o->okuma_hatasi());
        DOGRULA(d, v.size() == yazilan.size(), "ileti sayısı");
        bool dosya_sirasi_farkli = false;
        for (std::size_t i = 0; i < v.size() && i < yazilan.size(); ++i) {
            DOGRULA(d, v[i].log_time == yazilan[i].t && v[i].sequence == yazilan[i].sira && v[i].kanal == yazilan[i].kanal,
                    "sıra: log_time, eşitlikte sequence (konum " + std::to_string(i) + ")");
            dosya_sirasi_farkli = dosya_sirasi_farkli || v[i].sequence != i + 1;
        }
        DOGRULA(d, dosya_sirasi_farkli, "dosya sırası log_time sırasından farklıydı (sınama anlamlı)");
        // Süzgeç: yalnız seçilen kanallar, aynı sırayla.
        const KanalTanimi* secili[] = {kanal_bul("olay"), kanal_bul("sensor/imu")};
        o->suz(secili);
        std::size_t n = 0;
        std::int64_t onceki_t = -1;
        std::uint32_t onceki_s = 0;
        Ileti i;
        bool sirali = true;
        while (o->sonraki(i)) {
            sirali = sirali && (i.kanal == secili[0] || i.kanal == secili[1]) &&
                     (i.log_time > onceki_t || (i.log_time == onceki_t && i.sequence > onceki_s));
            onceki_t = i.log_time;
            onceki_s = i.sequence;
            ++n;
        }
        const auto beklenen_n = static_cast<std::size_t>(
            std::count_if(yazilan.begin(), yazilan.end(), [](const Yazilan& y) { return y.kanal == "olay" || y.kanal == "sensor/imu"; }));
        DOGRULA(d, sirali && n == beklenen_n, "süzgeçli okuma");
    }
    {
        // Elle kurulmuş dosya: parçalar zaman sırasının tersine; eşit `log_time`'lar farklı parçalarda.
        const KanalTanimi* kn = kanal_bul("calistirma/durum");
        const SemaTanimi& s = semalar()[kn->sema];
        const Ornek ornek = ornek_uret(Kanal::calistirma_durum, 1);
        const ElleMcap::Eslem metadata = {{"kanal_surumu", "1"}, {"zaman_anlami", "sim"}, {"kademe", "olaylar"}};
        ElleMcap p1;
        p1.sema(1, s.ad, kSemaKodlamasi, s.veri).kanal(1, 1, kn->ad, kIletiKodlamasi, metadata);
        p1.ileti(1, 1, 900, 0, ornek.bayt).ileti(1, 2, 500, 0, ornek.bayt).ileti(1, 3, 700, 0, ornek.bayt);
        ElleMcap p2;
        p2.ileti(1, 4, 500, 0, ornek.bayt).ileti(1, 5, 100, 0, ornek.bayt).ileti(1, 6, 900, 0, ornek.bayt);
        ElleMcap p3;
        p3.ileti(1, 7, 0, 0, ornek.bayt).ileti(1, 8, 700, 0, ornek.bayt);
        ElleMcap f;
        f.sihir().baslik("ida", "elle").parca(p1.al()).parca(p2.al()).parca(p3.al()).veri_sonu().footer();
        AcmaHatasi h = AcmaHatasi::yok;
        const auto o = Okuyucu::ac_bellek(f.al(), h);
        DOGRULA(d, o != nullptr && o->ozet().kapanmis && !o->ozet().bozuk, "elle kurulan dosya okunur");
        if (o) {
            const std::vector<OkunanIleti> v = hepsini_oku(*o);
            const std::vector<std::pair<std::int64_t, std::uint32_t>> beklenen = {{0, 7},   {100, 5}, {500, 2}, {500, 4},
                                                                                  {700, 3}, {700, 8}, {900, 1}, {900, 6}};
            DOGRULA(d, v.size() == beklenen.size(), "elle dosya ileti sayısı");
            for (std::size_t i = 0; i < v.size() && i < beklenen.size(); ++i) {
                DOGRULA(d, v[i].log_time == beklenen[i].first && v[i].sequence == beklenen[i].second, "elle dosya sırası");
            }
            // Üst verisi olmayan dosya: yalnız izleme.
            DOGRULA(d, !o->ozet().ust_veri_var && o->ozet().guvence == Guvence::izleme, "üst verisiz dosya yalnız izleme");
        }
    }
    return d.hata;
}

// Kabul 3: üç kademede hangi kanalların yazıldığı Tablo 3.3 ile birebir; zorunlu kanallar her kademede.
int test_kademe()
{
    Denetci d;
    const json k = katalog();
    int no = 0;
    for (const Kademe kademe : {Kademe::olaylar, Kademe::telemetri, Kademe::tam}) {
        for (const char* kip : {"sil", "hil_bariyer"}) {
            const std::string etiket = std::string(kademe_adi(kademe)) + "/" + kip;
            Duzenek dz("kademe_" + std::to_string(no++), kademe, kip);
            DOGRULA(d, dz.y != nullptr, etiket + ": yazıcı açıldı: " + dz.hata);
            if (!dz.y) {
                continue;
            }
            DOGRULA(d, dz.y->kademe() == kademe && dz.u().kademe() == kademe, etiket + ": kademe");
            std::set<std::string> beklenen;
            std::set<std::string> zorunlu;
            for (const json& kn : k["x_ida_kanallar"]) {
                const std::string ad = kn["ad"].get<std::string>();
                const bool etkin = beklenen_etkin(kn, kademe, kip);
                if (etkin) {
                    beklenen.insert(ad);
                }
                if (kn["kademe"].get<std::string>() == "olaylar") {
                    zorunlu.insert(ad);
                }
                if (ad == "kamera/kare") {
                    continue;
                }
                const KanalTanimi* t = kanal_bul(ad);
                DOGRULA(d, t != nullptr, ad + ": tabloda yok");
                if (t == nullptr) {
                    continue;
                }
                if (ad == "kayit/eksik") {
                    continue;  // yalnız kayıt yazıcısı yazar; kanal kaydı özet bölümünde bulunur
                }
                YazmaSonucu s;
                if (ad == "hil/bariyer") {
                    s = bariyer_yaz(dz.u(), 2, 1000, 1);  // kaçırıldı: her kademede yazılır
                } else if (ad == "algilama/parti") {
                    s = parti_yaz(dz.u(), 2, 0, 1, 1000, 1);  // geç: her kademede yazılır
                } else {
                    s = ornek_yaz(dz.u(), static_cast<Kanal>(t->indeks), 99, 1000, 1);
                }
                DOGRULA(d, s == (etkin ? YazmaSonucu::yazildi : YazmaSonucu::suzuldu), etiket + ": " + ad + " beklenen sonuç");
            }
            const KapanisSonucu ks = dz.kapat();
            DOGRULA(d, ks.dosya_kapandi, etiket + ": dosya kapandı");
            const UreticiSayaclari us = dz.u().sayaclar();
            DOGRULA(d, us.uretilen == beklenen.size() - 1 && us.kuyruklanan == beklenen.size() - 1 &&
                           us.suzulen == kKanalSayisi - beklenen.size(),
                    etiket + ": üretici sayaçları");
            // Dosyadaki kanal kümesi (A6 okuyucusu ve kitaplığın okuyucusu).
            const auto o = dz.oku();
            DOGRULA(d, o != nullptr && o->ozet().saglam(), etiket + ": dosya sağlam");
            if (!o) {
                continue;
            }
            std::set<std::string> dosyada;
            for (const KanalOzeti& c : o->ozet().kanallar) {
                dosyada.insert(c.konu);
                DOGRULA(d, c.okunur && c.ileti_sayisi == 1, etiket + ": " + c.konu + " iletisi");
            }
            // Düşme olmadığı için `kayit/eksik` iletisi yoktur; kanal kaydı özet bölümündedir.
            std::set<std::string> iletili = beklenen;
            iletili.erase("kayit/eksik");
            DOGRULA(d, dosyada == iletili, etiket + ": iletisi olan kanal kümesi Tablo 3.3 ile aynı (" + std::to_string(dosyada.size()) + ")");
            const KitaplikDokumu kd = kitaplik_ile_oku(yol_metni(dz.y->yol()));
            std::set<std::string> ozette;
            for (const KitaplikKanal& c : kd.kanallar) {
                ozette.insert(c.konu);
            }
            DOGRULA(d, kd.acildi && ozette == beklenen, etiket + ": özet bölümündeki kanal kümesi");
            for (const std::string& z : zorunlu) {
                DOGRULA(d, ozette.count(z) == 1 && (z == "kayit/eksik" || dosyada.count(z) == 1),
                        etiket + ": zorunlu kanal her kademede: " + z);
            }
            DOGRULA(d, zorunlu.size() == 21, "olaylar kademesinin kanal sayısı");
            const Guvence beklenen_guvence = kademe == Kademe::olaylar                                 ? Guvence::izleme
                                             : (kademe == Kademe::tam && std::string_view(kip) == "sil") ? Guvence::girdi
                                                                                                          : Guvence::fizik;
            DOGRULA(d, o->ozet().guvence == beklenen_guvence && o->ozet().kademe == kademe,
                    etiket + ": güvence " + std::string(guvence_adi(o->ozet().guvence)));
        }
    }
    // Tablo 3.3'ün kanal sayıları (62 kanal; kamera/kare bu dilimde yok).
    std::size_t olaylar = 0;
    std::size_t telemetri = 0;
    std::size_t tam = 0;
    for (const KanalTanimi& t : kanallar()) {
        olaylar += kanal_etkin(t, Kademe::olaylar, "sil") ? 1u : 0u;
        telemetri += kanal_etkin(t, Kademe::telemetri, "sil") ? 1u : 0u;
        tam += kanal_etkin(t, Kademe::tam, "sil") ? 1u : 0u;
    }
    DOGRULA(d, olaylar == 21 && telemetri == 55 && tam == 61, "kademe başına kanal sayısı: " + std::to_string(olaylar) + ", " +
                                                                   std::to_string(telemetri) + ", " + std::to_string(tam));
    return d.hata;
}

// A6-K17 ve Tablo 3.3: `hil/bariyer` ve `algilama/parti`'nin `olaylar` kademesindeki süzgeci.
int test_kademe_suzgeci()
{
    Denetci d;
    int no = 0;
    for (const Kademe kademe : {Kademe::olaylar, Kademe::telemetri, Kademe::tam}) {
        Duzenek dz("suzgec_" + std::to_string(no++), kademe, "hil_bariyer");
        DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
        if (!dz.y) {
            continue;
        }
        const bool olaylar = kademe == Kademe::olaylar;
        std::vector<std::uint8_t> beklenen_karar;
        for (std::uint8_t karar = 0; karar <= 4; ++karar) {
            const YazmaSonucu s = bariyer_yaz(dz.u(), karar, 1000 + karar, karar);
            const bool yazilir = !olaylar || karar != 0;
            DOGRULA(d, s == (yazilir ? YazmaSonucu::yazildi : YazmaSonucu::suzuldu),
                    std::string(kademe_adi(kademe)) + ": hil/bariyer karar " + std::to_string(karar));
            if (yazilir) {
                beklenen_karar.push_back(karar);
            }
        }
        struct Parti {
            std::uint8_t nihai;
            std::uint8_t ret;
            std::uint8_t gec;
            bool olaylarda;
        };
        const Parti partiler[] = {{0, 0, 0, false}, {2, 0, 0, true}, {0, 3, 0, true}, {0, 0, 1, true}, {255, 0, 0, true}, {8, 10, 1, true}};
        std::size_t beklenen_parti = 0;
        for (const Parti& p : partiler) {
            const YazmaSonucu s = parti_yaz(dz.u(), p.nihai, p.ret, p.gec, 2000, 5);
            const bool yazilir = !olaylar || p.olaylarda;
            DOGRULA(d, s == (yazilir ? YazmaSonucu::yazildi : YazmaSonucu::suzuldu),
                    std::string(kademe_adi(kademe)) + ": algilama/parti " + std::to_string(p.nihai) + "/" + std::to_string(p.ret) + "/" +
                        std::to_string(p.gec));
            beklenen_parti += yazilir ? 1u : 0u;
        }
        DOGRULA(d, dz.kapat().dosya_kapandi, "dosya kapandı");
        const auto o = dz.oku();
        DOGRULA(d, o != nullptr, "okuyucu");
        if (!o) {
            continue;
        }
        std::vector<std::uint8_t> okunan_karar;
        std::size_t okunan_parti = 0;
        for (const OkunanIleti& i : hepsini_oku(*o)) {
            if (i.kanal == "hil/bariyer") {
                yuk_hil_bariyer y;
                DOGRULA(d, coz(i.veri.data(), i.veri.size(), y), "hil/bariyer çözülür");
                okunan_karar.push_back(y.karar);
            } else if (i.kanal == "algilama/parti") {
                ++okunan_parti;
            }
        }
        DOGRULA(d, okunan_karar == beklenen_karar, std::string(kademe_adi(kademe)) + ": dosyadaki bariyer kararları");
        DOGRULA(d, okunan_parti == beklenen_parti, std::string(kademe_adi(kademe)) + ": dosyadaki algılama partileri");
    }
    return d.hata;
}

}  // namespace ida::kayit::test
