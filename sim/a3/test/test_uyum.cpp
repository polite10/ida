// test_uyum.cpp — web/src/a3 çözücüsüyle uyum örnekleri (W4-a3 kabul 2).
//
// Bu dilimde yayınlanan her kanal için bir yük kurulur, sunucu çekirdeğinden (soketsiz) gerçek
// çerçeve olarak alınır ve beklenen değerler üretilen `json_dok` ile yazılır. `uyum.mjs` aynı
// çerçeveyi panelin çözücüsüyle çözüp değerleri karşılaştırır. Dizi içermeyen yüklerin değerleri
// sabit tohumlu rastgele baytlardan (her alan ayırt edici), dizi içerenler elle kurulur.
#include "ozet.hpp"
#include "test_destek.hpp"

#include <fstream>
#include <random>

namespace ida::a3::test {

namespace {

// Rastgele ama geçerli yük baytları (test_birim.cpp'deki üretecin küçük kopyası).
void rastgele_yuk(std::mt19937_64& g, const AlanTanimi* alanlar, std::uint16_t n, Yazici& w)
{
    for (std::uint16_t i = 0; i < n; ++i) {
        const AlanTanimi& a = alanlar[i];
        auto sayac = [&](std::uint32_t v) {
            if (a.sayac == Tip::u8) {
                w.u8(static_cast<std::uint8_t>(v));
            } else if (a.sayac == Tip::u16) {
                w.u16(static_cast<std::uint16_t>(v));
            } else {
                w.u32(v);
            }
        };
        if (a.tip == Tip::metin || a.tip == Tip::bayt_dizisi) {
            const auto uz = static_cast<std::uint32_t>(1 + g() % std::min<std::uint32_t>(a.azami, 30));
            sayac(uz);
            for (std::uint32_t k = 0; k < uz; ++k) {
                const auto b = a.tip == Tip::metin ? static_cast<std::uint8_t>('a' + g() % 26) : static_cast<std::uint8_t>(g());
                w.baytlar(&b, 1);
            }
            continue;
        }
        const int tekrar = a.adet > 0 ? a.adet : 1;
        for (int k = 0; k < tekrar; ++k) {
            switch (a.tip) {
            case Tip::u8: w.u8(static_cast<std::uint8_t>(g() % 250)); break;  // 255 (yok) dışı
            case Tip::u16: w.u16(static_cast<std::uint16_t>(g())); break;
            case Tip::u32: w.u32(static_cast<std::uint32_t>(g() % 0xFFFFFF00u) + 1u); break;
            case Tip::u64: w.u64((g() >> 2u) + 1u); break;
            case Tip::i32: w.i32(static_cast<std::int32_t>(static_cast<std::uint32_t>(g()))); break;
            case Tip::i64: w.i64(static_cast<std::int64_t>(g() >> 2u)); break;
            case Tip::f32: w.f32(static_cast<float>(static_cast<double>(static_cast<std::int32_t>(static_cast<std::uint32_t>(g()))) / 4096.0)); break;
            case Tip::f64: w.f64(static_cast<double>(static_cast<std::int64_t>(g() >> 4u)) / 65536.0); break;
            default: break;
            }
        }
    }
}

struct Ornek {
    std::string kanal;
    std::string beklenen;   // json_dok
    bool baglanti = false;  // baglanti/durum: son üç alan sunucuda bağlantıya göre yamanır
};

template <Kanal K>
Ornek rastgele_ornek(std::mt19937_64& g, Yayinci& y, std::vector<std::vector<std::uint8_t>>& depo, std::int64_t t_ns)
{
    using T = typename KanalYuku<K>::tip;
    const KanalTanimi* k = kanal_bul(static_cast<std::uint16_t>(K));
    depo.emplace_back(70000);
    auto& b = depo.back();
    Yazici w(b.data(), b.size());
    rastgele_yuk(g, k->yuk->alanlar, k->yuk->alan_sayisi, w);
    b.resize(w.konum());
    T v;
    coz(b.data(), b.size(), v);
    y.yayinla<K>(t_ns, v);
    return {std::string(k->ad), json_dok(v)};
}

}  // namespace

// Döndürür: (kanal adı, JSON satırı) — satır: {"kanal","ileti_hex","beklenen","baslik"}.
std::vector<std::pair<std::string, std::string>> uyum_ornekleri()
{
    Ortam o;
    std::mt19937_64 g(20261124u);
    std::vector<std::vector<std::uint8_t>> depo;
    std::vector<Ornek> ornekler;
    const std::int64_t t = 987654321012;
    // Durum sınıfı (dizi içermeyenler): rastgele
    ornekler.push_back(rastgele_ornek<Kanal::calistirma_durum>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::kart_durum>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::emniyet_girdiler>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::sensor_basinc>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::sensor_dvl>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::sensor_batarya>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::sonar_ping1d>(g, o.yayinci, depo, t));
    ornekler.push_back(rastgele_ornek<Kanal::kart_cekirdek_sayaclari>(g, o.yayinci, depo, t));
    // Dizi içerenler: elle
    std::array<yuk_fizik_durum_govdeler, 2> govde{};
    govde[0] = {7, 1, 2, {1.5, -2.25, 3.125}, {0.5, 0.25, -0.125, 0.0625}, {1, 2, 3, 4, 5, 6}, {-1, -2, -3}};
    govde[1] = {9, 2, 255, {10.5, 20.25, 30.125}, {1, 0, 0, 0}, {0.5, 0, 0, 0, 0, 0.25}, {0, 0, 9.75}};
    std::array<yuk_fizik_durum_iticiler, 3> itici{{{0.5f, 20.0f, 3.0f}, {-0.25f, -10.0f, 1.5f}, {1.0f, 40.0f, 6.0f}}};
    yuk_fizik_durum f;
    f.adim_no = 123456789012345ull;
    f.govdeler = govde;
    f.iticiler = itici;
    f.bara_gerilimi_v = 15.5f;
    f.paket_akimi_a = 2.75f;
    f.soc = 0.625f;
    o.yayinci.yayinla<Kanal::fizik_durum>(t, f);
    ornekler.push_back({"fizik/durum", json_dok(f)});
    std::array<yuk_sayac_gercek_zaman_mesaj_retleri, 2> mr{{{11, 12, 13}, {21, 22, 23}}};
    yuk_sayac_gercek_zaman s;
    s.parti_gonderilen = 1;
    s.parti_kacirildi = 2;
    s.gercek_zaman_kaymasi_ns = -5;
    s.olay_sonuc = {1, 2, 3, 4, 5, 6};
    s.calistirma_gecersiz = 1;
    s.neden_bitleri = 0x41;
    s.emniyet_rev_doydu = 1;
    s.mesaj_retleri = mr;
    s.a3_halka_tasma = 99;
    s.kayit_dusen = 100;
    o.yayinci.yayinla<Kanal::sayac_gercek_zaman>(t, s);
    ornekler.push_back({"sayac/gercek_zaman", json_dok(s)});
    std::array<yuk_atis_durum_tupler, 2> tup{{{0, 1, 0, -1, 2}, {1, 0, 1, 5000, 3}}};
    yuk_atis_durum a;
    a.fc_durumu = 2;
    a.engel_maskesi = 0x30;
    a.kart_son_olay_kimligi = 77;
    a.tupler = tup;
    o.yayinci.yayinla<Kanal::atis_durum>(t, a);
    ornekler.push_back({"atis/durum", json_dok(a)});
    std::array<yuk_ariza_etkin_arizalar, 1> ar{};
    ar[0].fault_id = "E3";
    ar[0].sinif = 1;
    ar[0].hedef = "itici_3";
    ar[0].tur = "zayiflama";
    ar[0].kaynak = 1;
    ar[0].baslangic_adimi = 400;
    yuk_ariza_etkin ae;
    ae.arizalar = ar;
    o.yayinci.yayinla<Kanal::ariza_etkin>(t, ae);
    ornekler.push_back({"ariza/etkin", json_dok(ae)});
    std::array<yuk_baglanti_durum_emulator, 1> em{{{1, 4096, 3, 0}}};
    yuk_baglanti_durum bd;
    bd.a1_oturum = 3;
    bd.sess = 9;
    bd.acilis_sayaci = 4;
    bd.d_asagi_ort_ns = 1500000;
    bd.emulator = em;
    o.yayinci.yayinla<Kanal::baglanti_durum>(t, bd);
    ornekler.push_back({"baglanti/durum", {}, true});
    o.dongu();
    // Abone ol: durum sınıfı son değerler `ozet` bayrağıyla hemen gelir (A3-K35).
    const BagId id = o.ws_ac();
    std::string liste;
    for (const char* k : {"fizik/durum", "kart/durum", "atis/durum", "emniyet/girdiler", "ariza/etkin", "sensor/basinc",
                          "sensor/dvl", "sensor/batarya", "sonar/ping360", "sonar/ping1d", "baglanti/durum",
                          "kart/cekirdek_sayaclari"}) {
        liste += std::string(liste.empty() ? "" : ",") + "{\"kanal\":\"" + k + "\",\"hz\":50}";
    }
    o.gonder(id, ws_istemci_cercevesi(1, "{\"tur\":\"abone_ol\",\"istek_no\":1,\"kanallar\":[" + liste + "]}"));
    // Dizi sınıfı ve olay: abonelikten sonra
    ornekler.push_back(rastgele_ornek<Kanal::sonar_ping360>(g, o.yayinci, depo, t));
    yuk_olay ol;
    ol.tur = 22;
    ol.onem = 1;
    ol.kaynak = 0;
    ol.adim_no = 4242;
    ol.komut_no = 17;
    ol.istemci_id = 3;
    ol.ayrinti = R"({"kod":"uyum","ileti":"çğışöü ✓"})";
    o.yayinci.olay(t, ol);
    ornekler.push_back({"olay", json_dok(ol)});
    o.ilerle(std::chrono::milliseconds(40));
    std::vector<A3Kare> kareler;
    for (const auto& c : o.ws_al(id)) {
        if (c.opkod == 2) {
            a3_ileti_ayir(c.yuk, kareler);
        }
    }
    std::vector<std::pair<std::string, std::string>> sonuc;
    for (auto& orn : ornekler) {
        const KanalTanimi* k = kanal_bul(orn.kanal);
        const A3Kare* bulunan = nullptr;
        for (const auto& kr : kareler) {
            if (kr.baslik.kanal_id == k->kanal_id) {
                bulunan = &kr;
                break;
            }
        }
        if (bulunan == nullptr) {
            std::printf("  uyum: %s çerçevesi alınamadı\n", orn.kanal.c_str());
            continue;
        }
        if (orn.baglanti) {
            // Bağlantıya özgü son üç alan: sunucunun yazdığı değerler beklenen sayılır.
            Okuyucu r(bulunan->yuk.data() + bulunan->yuk.size() - 20, 20);
            bd.bu_baglanti_ezilen_durum = r.u64();
            bd.bu_baglanti_dusen_dizi = r.u64();
            bd.bu_baglanti_olay_kuyrugu = r.u32();
            orn.beklenen = json_dok(bd);
        }
        std::vector<std::uint8_t> cerceve(kBaslikBayt);
        baslik_yaz(bulunan->baslik, cerceve.data());
        cerceve.insert(cerceve.end(), bulunan->yuk.begin(), bulunan->yuk.end());
        std::string satir = "{\"kanal\":\"" + orn.kanal + "\",\"ileti_hex\":\"" + ozet::onaltilik(cerceve.data(), cerceve.size()) +
                            "\",\"beklenen\":" + orn.beklenen + ",\"baslik\":{\"t_ns\":\"" + std::to_string(bulunan->baslik.t_ns) +
                            "\",\"sira\":" + std::to_string(bulunan->baslik.sira) + ",\"kanal_id\":" +
                            std::to_string(bulunan->baslik.kanal_id) + ",\"kanal_surumu\":" +
                            std::to_string(bulunan->baslik.kanal_surumu) + ",\"bayraklar\":" +
                            std::to_string(bulunan->baslik.bayraklar) + "}}";
        sonuc.emplace_back(orn.kanal, std::move(satir));
    }
    return sonuc;
}

int uyum_ornekleri_yaz(const char* yol)
{
    const auto o = uyum_ornekleri();
    std::ofstream f(yol, std::ios::binary);
    f << "{\"ornekler\":[";
    for (std::size_t i = 0; i < o.size(); ++i) {
        f << (i == 0 ? "" : ",\n") << o[i].second;
    }
    f << "]}\n";
    return o.size() == 15 && f.good() ? 0 : 1;  // bu dilimde yayınlanan 15 kanal (olay dahil)
}

}  // namespace ida::a3::test
