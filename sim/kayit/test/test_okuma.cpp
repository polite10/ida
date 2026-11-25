// test_okuma.cpp — okuyucu: kabul 5 (çökme ve kurtarma; A6-K60), kabul 6 (bozulma; A6-K59, K46),
// sürüm kuralları (A6-K47…K49), rastgele bozulmuş dosyalar, döküm programı.
#include "test_destek.hpp"

#include "crc32.hpp"
#include "ida/kayit/coz.hpp"
#include "mcap_kitaplik_okuma.hpp"
#include "sinama_uretim.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <set>
#include <thread>

namespace ida::kayit::test {

namespace {

std::string yol_metni(const std::filesystem::path& p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::uint64_t oku_u64(const std::vector<std::uint8_t>& b, std::uint64_t k)
{
    std::uint64_t v = 0;
    std::memcpy(&v, b.data() + k, 8);
    return v;
}
std::uint32_t oku_u32(const std::vector<std::uint8_t>& b, std::uint64_t k)
{
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + k, 4);
    return v;
}
void yaz_u32(std::vector<std::uint8_t>& b, std::uint64_t k, std::uint32_t v)
{
    std::memcpy(b.data() + k, &v, 4);
}
void yaz_u64(std::vector<std::uint8_t>& b, std::uint64_t k, std::uint64_t v)
{
    std::memcpy(b.data() + k, &v, 8);
}

// Parça kaydının sıkıştırma dizesi (gövde: zamanlar 16, açık boy 8, CRC 4, dize).
std::string parca_sikistirmasi(const std::vector<std::uint8_t>& b, const KayitKonumu& k)
{
    const std::uint32_t n = oku_u32(b, k.govde + 28);
    return std::string(reinterpret_cast<const char*>(b.data() + k.govde + 32), n);
}

std::int64_t bul(const std::vector<std::uint8_t>& b, std::string_view aranan, std::uint64_t bas = 0)
{
    // Bayt karşılaştırması: `char` işaretli olabilir.
    const auto it = std::search(b.begin() + static_cast<std::ptrdiff_t>(bas), b.end(), aranan.begin(), aranan.end(),
                                [](std::uint8_t x, char y) { return x == static_cast<std::uint8_t>(y); });
    return it == b.end() ? -1 : static_cast<std::int64_t>(it - b.begin());
}

// Parçaları denetimli bir kayıt: her parça bilinen sayıda ileti taşır (saat ilerletilerek kapatılır).
struct ParcaliDosya {
    std::vector<std::uint8_t> bayt;
    std::vector<std::size_t> parca_iletileri;  // parça başına ileti
    std::vector<OkunanIleti> iletiler;         // sağlam dosyadan okunan (sıra numarasına göre)
    std::map<std::uint32_t, OkunanIleti> sira_ile;
    std::vector<KayitKonumu> kayitlar;
    std::vector<KayitKonumu> parcalar;
    UstVeri ust_veri;
    std::uint64_t toplam = 0;
};

enum class Veri { sikistirilir, sikistirilmaz };

ParcaliDosya parcali_dosya(Denetci& d, const std::string& ad, const std::vector<std::size_t>& parca_iletileri, Veri veri)
{
    ParcaliDosya p;
    p.parca_iletileri = parca_iletileri;
    Duzenek dz(ad, Kademe::telemetri, "hil_bariyer");
    DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
    if (!dz.y) {
        return p;
    }
    FizikDurumu fd;
    Rastgele r(2026);
    yuk_hil_tf_yazim tf;
    std::uint64_t adim = 0;
    for (const std::size_t n : parca_iletileri) {
        for (std::size_t i = 0; i < n; ++i, ++adim) {
            const auto t = static_cast<std::int64_t>(adim) * 5'000'000;
            if (veri == Veri::sikistirilir) {
                fd.guncelle(adim);
                DOGRULA(d, dz.u().yaz<Kanal::fizik_durum>(t, adim, fd.y) == YazmaSonucu::yazildi, "yaz");
            } else {
                // Küçük iletiler: parça 1 KiB'ın altında kalır ve kitaplık onu sıkıştırmasız yazar
                // (ilk parça şema ve kanal kayıtlarını da taşıdığı için sıkıştırılmış olabilir).
                tf.batch_seq = static_cast<std::uint32_t>(adim);
                tf.plan_duvar_ns = static_cast<std::int64_t>(r.ham() >> 1u);
                tf.yazim_baslangic_duvar_ns = static_cast<std::int64_t>(r.ham() >> 1u);
                tf.yazim_bitis_duvar_ns = static_cast<std::int64_t>(r.ham() >> 1u);
                DOGRULA(d, dz.u().yaz<Kanal::hil_tf_yazim>(t, adim, tf) == YazmaSonucu::yazildi, "yaz");
            }
        }
        dz.y->isle();
        dz.saat.ilerlet(1'000'000'000);
        dz.y->isle();  // süre doldu: parça kapanır (A6-K04)
    }
    const KapanisSonucu ks = dz.kapat();
    DOGRULA(d, ks.dosya_kapandi && ks.sayaclar.kapanan_parca == parca_iletileri.size(), "parçalar denetimli kapandı");
    p.bayt = dosya_oku(dz.y->yol());
    p.kayitlar = kayitlari_tara(p.bayt);
    for (const KayitKonumu& k : p.kayitlar) {
        if (k.op == 0x06) {
            p.parcalar.push_back(k);
        }
    }
    DOGRULA(d, p.parcalar.size() == parca_iletileri.size(), "dosyadaki parça sayısı");
    AcmaHatasi h = AcmaHatasi::yok;
    const auto o = Okuyucu::ac_bellek(p.bayt, h);
    DOGRULA(d, o != nullptr && o->ozet().saglam(), "özgün dosya sağlam");
    if (o) {
        p.iletiler = hepsini_oku(*o);
        for (const OkunanIleti& i : p.iletiler) {
            p.sira_ile[i.sequence] = i;
        }
        p.ust_veri = o->ozet().ust_veri;
        p.toplam = o->ozet().ileti_sayisi;
    }
    return p;
}

// `a6_kanallar.json` eki (≈ 200 KiB) çıkarılmış kopya: rastgele bozulma sınamasında değişiklikler
// ek yerine dosyanın yapısına düşsün. Özet bölümündeki konumlar bayatlar; A6 okuyucusu özete dayanmaz.
std::vector<std::uint8_t> katalog_eksiz(const std::vector<std::uint8_t>& b)
{
    std::vector<std::uint8_t> c(b.begin(), b.begin() + 8);
    std::uint64_t son = 8;
    for (const KayitKonumu& k : kayitlari_tara(b)) {
        const bool katalog_eki = k.op == 0x09 && k.boy > 100000;
        if (!katalog_eki) {
            c.insert(c.end(), b.begin() + static_cast<std::ptrdiff_t>(k.konum), b.begin() + static_cast<std::ptrdiff_t>(k.govde + k.boy));
        }
        son = k.govde + k.boy;
    }
    c.insert(c.end(), b.begin() + static_cast<std::ptrdiff_t>(son), b.end());
    return c;
}

struct Okuma {
    bool acildi = false;
    AcmaHatasi hata = AcmaHatasi::yok;
    DosyaOzeti ozet;
    std::vector<OkunanIleti> iletiler;
    std::string okuma_hatasi;
};

Okuma oku(const std::vector<std::uint8_t>& b)
{
    Okuma s;
    const auto o = Okuyucu::ac_bellek(b, s.hata);
    if (!o) {
        return s;
    }
    s.acildi = true;
    s.ozet = o->ozet();
    s.iletiler = hepsini_oku(*o);
    s.okuma_hatasi = o->okuma_hatasi();
    return s;
}

// Geçerli açılış üst verisi taşıyan, tek kanallı elle kurulmuş dosyanın ortak parçaları.
struct ElleTaban {
    const KanalTanimi* kanal = kanal_bul("calistirma/durum");
    Ornek ornek = ornek_uret(Kanal::calistirma_durum, 5);
    ElleMcap::Eslem metadata = {{"kanal_surumu", "1"}, {"zaman_anlami", "sim"}, {"kademe", "olaylar"}};
    ElleMcap::Eslem acilis;
    ElleTaban()
    {
        const AcilisUstVerisi u = ornek_ust_veri(kOrnekRunId, "hil_bariyer");
        const auto tablo = acilis_anahtarlari();
        std::size_t i = 0;
        for (const auto& [k, v] : ust_veri_ciftleri(u)) {
            if (!v.empty() || tablo[i].zorunlu) {
                acilis.emplace(std::string(k), std::string(v));
            }
            ++i;
        }
        acilis["a6_surumu"] = "1.0";
        acilis["kayit_kademesi"] = "olaylar";
        acilis["kamera_kareleri"] = "kapali";
        acilis["kayit_duvar_saati"] = "monoton";
    }
    std::vector<std::uint8_t> parca(std::uint32_t ilk_sira = 1) const
    {
        const SemaTanimi& s = semalar()[kanal->sema];
        ElleMcap p;
        p.sema(1, s.ad, kSemaKodlamasi, s.veri).kanal(1, 1, kanal->ad, kIletiKodlamasi, metadata);
        p.ileti(1, ilk_sira, 1000, 5, ornek.bayt).ileti(1, ilk_sira + 1, 2000, 6, ornek.bayt);
        return p.al();
    }
};

}  // namespace

// A6-K60: kapanmamış dosyanın kurtarma okuması. Üç durum (A6 §7): kesik indeks, kesik parça, tam parça
// sonunda çökme; ayrıca her kesim noktası.
int test_kurtarma_kesik()
{
    Denetci d;
    const std::vector<std::size_t> parca_iletileri = {7, 12, 5, 9, 3};
    const ParcaliDosya p = parcali_dosya(d, "kurtarma", parca_iletileri, Veri::sikistirilir);
    if (d.hata != 0 || p.parcalar.size() != parca_iletileri.size()) {
        return d.hata + 1;
    }
    const auto kes = [&](std::uint64_t boy) {
        return std::vector<std::uint8_t>(p.bayt.begin(), p.bayt.begin() + static_cast<std::ptrdiff_t>(boy));
    };
    const auto beklenen_ileti = [&](std::uint64_t boy) {
        std::size_t n = 0;
        for (std::size_t i = 0; i < p.parcalar.size(); ++i) {
            if (p.parcalar[i].govde + p.parcalar[i].boy <= boy) {
                n += parca_iletileri[i];
            }
        }
        return n;
    };
    const auto kurtarma_denetle = [&](std::uint64_t boy, const std::string& ne) {
        const Okuma s = oku(kes(boy));
        DOGRULA(d, s.acildi, ne + ": okunur");
        if (!s.acildi) {
            return;
        }
        const std::size_t n = beklenen_ileti(boy);
        DOGRULA(d, !s.ozet.kapanmis && !s.ozet.bozuk && !s.ozet.saglam(), ne + ": kapanmamış, bozuk değil");
        DOGRULA(d, s.ozet.ileti_sayisi == n && s.iletiler.size() == n, ne + ": kurtarılan ileti sayısı " +
                                                                           std::to_string(s.iletiler.size()) + " / " + std::to_string(n));
        DOGRULA(d, s.ozet.guvence == Guvence::izleme && !s.ozet.guvence_on_eki_son_adim.has_value() && !s.ozet.kapanis.has_value(),
                ne + ": güvence yalnız izleme; kapanış kaydı kullanılmaz");
        for (std::size_t i = 0; i < s.iletiler.size() && i < p.iletiler.size(); ++i) {
            DOGRULA(d, s.iletiler[i].sequence == p.iletiler[i].sequence && s.iletiler[i].veri == p.iletiler[i].veri,
                    ne + ": kurtarılan ileti özgünüyle aynı");
        }
        DOGRULA(d, s.ozet.ust_veri_var && s.ozet.ust_veri_gecerli && s.ozet.ekler.size() == ekler().size(),
                ne + ": açılış üst verisi ve ekler kullanılır");
    };
    // Kayıt konumları.
    std::uint64_t veri_sonu = 0;
    for (const KayitKonumu& k : p.kayitlar) {
        if (k.op == 0x0F) {
            veri_sonu = k.govde + k.boy;
        }
    }
    DOGRULA(d, veri_sonu != 0 && veri_sonu < p.bayt.size(), "Data End bulundu");
    // (1) Kesik indeks: Data End'den sonra, özet bölümünün ortasında: bütün parçalar.
    kurtarma_denetle((veri_sonu + p.bayt.size()) / 2, "kesik indeks");
    DOGRULA(d, beklenen_ileti((veri_sonu + p.bayt.size()) / 2) == p.toplam, "kesik indekste bütün parçalar beklenir");
    // (2) Kesik parça: üçüncü parçanın ortasında: önceki son tam parçada biter.
    kurtarma_denetle(p.parcalar[2].govde + p.parcalar[2].boy / 2, "kesik parça");
    DOGRULA(d, beklenen_ileti(p.parcalar[2].govde + p.parcalar[2].boy / 2) == 19, "kesik parçadan ileti gelmez");
    // (3) Tam parça sonunda çökme: dördüncü parçanın son baytında: o parça dahil.
    kurtarma_denetle(p.parcalar[3].govde + p.parcalar[3].boy, "tam parça sonu");
    DOGRULA(d, beklenen_ileti(p.parcalar[3].govde + p.parcalar[3].boy) == 33, "tam parça dahil");
    // Son parçadan sonra, kapanış üst verisi yazılmış ama Data End yok.
    kurtarma_denetle(veri_sonu - 13, "Data End öncesi");
    // Footer'ın son baytı ya da sondaki sihirli baytlar eksik.
    kurtarma_denetle(p.bayt.size() - 1, "son sihirli bayt eksik");
    kurtarma_denetle(p.bayt.size() - 8, "sondaki sihirli baytlar yok");
    kurtarma_denetle(p.bayt.size() - 9, "Footer yarım");
    // İlk parçanın ortası: hiç parça yok; açılış kaydı ve ekler yine okunur.
    kurtarma_denetle(p.parcalar[0].govde + 10, "ilk parça yarım");
    // Parça bölgesindeki her kesim noktası: bozuk sayılmaz, ileti sayısı tam parçalarla belirlenir.
    // (Büyük katalog eki çıkarılmış kopyada; her vakada ekin CRC'si yeniden hesaplanmasın.)
    {
        const std::vector<std::uint8_t> kucuk = katalog_eksiz(p.bayt);
        std::vector<KayitKonumu> parcalar;
        std::uint64_t son = 0;
        for (const KayitKonumu& k : kayitlari_tara(kucuk)) {
            if (k.op == 0x06) {
                parcalar.push_back(k);
            }
            if (k.op == 0x0F) {
                son = k.govde + k.boy;
            }
        }
        DOGRULA(d, parcalar.size() == parca_iletileri.size() && son != 0, "küçültülmüş kopyada parçalar");
        std::size_t vaka = 0;
        for (std::uint64_t boy = parcalar.empty() ? 0 : parcalar[0].konum - 3; boy < son + 40 && boy < kucuk.size(); ++boy) {
            AcmaHatasi h = AcmaHatasi::yok;
            const std::vector<std::uint8_t> kesik(kucuk.begin(), kucuk.begin() + static_cast<std::ptrdiff_t>(boy));
            const auto o = Okuyucu::ac_bellek(kesik, h);
            std::size_t n = 0;
            for (std::size_t i = 0; i < parcalar.size(); ++i) {
                n += parcalar[i].govde + parcalar[i].boy <= boy ? parca_iletileri[i] : 0u;
            }
            DOGRULA(d, o != nullptr && !o->ozet().kapanmis && !o->ozet().bozuk && o->ozet().ileti_sayisi == n &&
                           o->ozet().guvence == Guvence::izleme,
                    "kesim " + std::to_string(boy) + ": kurtarma");
            ++vaka;
        }
        std::printf("  parça bölgesinde %zu kesim noktası sınandı\n", vaka);
    }
    // Kurtarma sonu son tam parçanın sonudur: ondan sonraki tam üst veri kayıtları da kullanılmaz.
    {
        Duzenek dz("kurtarma_guncelleme", Kademe::telemetri, "hil_bariyer");
        DOGRULA(d, dz.y != nullptr, "yazıcı açıldı: " + dz.hata);
        if (dz.y) {
            a3::yuk_calistirma_durum cd;
            std::string hata;
            const auto parca = [&](int taban) {
                for (int i = 0; i < 3; ++i) {
                    (void)dz.u().yaz<Kanal::calistirma_durum>((taban + i) * 1000LL, static_cast<std::uint64_t>(taban + i), cd);
                }
                dz.y->isle();
                dz.saat.ilerlet(1'000'000'000);
                dz.y->isle();
            };
            parca(0);
            DOGRULA(d, dz.y->guncelle("kart_fw_version", "17", hata), "ilk güncelleme (iki parçanın arasında)");
            parca(10);
            DOGRULA(d, dz.y->guncelle("kart_core_version", "3", hata), "ikinci güncelleme (son parçadan sonra)");
            DOGRULA(d, dz.kapat().dosya_kapandi, "kapandı");
            const std::vector<std::uint8_t> tam = dosya_oku(dz.y->yol());
            std::uint64_t son = 0;
            for (const KayitKonumu& k : kayitlari_tara(tam)) {
                if (k.op == 0x0F) {
                    son = k.govde + k.boy;
                }
            }
            const Okuma butun = oku(tam);
            DOGRULA(d, butun.acildi && butun.ozet.saglam() && butun.ozet.ust_veri.count("kart_fw_version") == 1 &&
                           butun.ozet.ust_veri.count("kart_core_version") == 1,
                    "kapanmış dosyada iki güncelleme de uygulanır");
            const Okuma yarim = oku(std::vector<std::uint8_t>(tam.begin(), tam.begin() + static_cast<std::ptrdiff_t>(son + 20)));
            DOGRULA(d, yarim.acildi && !yarim.ozet.kapanmis && yarim.iletiler.size() == 6, "kesik dosyada bütün parçalar");
            DOGRULA(d, yarim.acildi && yarim.ozet.ust_veri.count("kart_fw_version") == 1 && yarim.ozet.ust_veri.at("kart_fw_version") == "17",
                    "son parçadan önceki güncelleme kullanılır");
            DOGRULA(d, yarim.acildi && yarim.ozet.ust_veri.count("kart_core_version") == 0,
                    "son parçadan sonraki güncelleme kullanılmaz (A6-K60 kurtarma sonu)");
        }
    }
    // Başlık bölgesi: sihirli baytlar ve Header okunamıyorsa dosya okunmaz (A6 §5).
    {
        AcmaHatasi h = AcmaHatasi::yok;
        DOGRULA(d, Okuyucu::ac_bellek(kes(0), h) == nullptr && h == AcmaHatasi::sihirli_bayt_yok, "boş dosya");
        DOGRULA(d, Okuyucu::ac_bellek(kes(7), h) == nullptr && h == AcmaHatasi::sihirli_bayt_yok, "yarım sihirli baytlar");
        DOGRULA(d, Okuyucu::ac_bellek(kes(8), h) == nullptr && h == AcmaHatasi::baslik_okunamadi, "Header yok");
        DOGRULA(d, Okuyucu::ac_bellek(kes(20), h) == nullptr && h == AcmaHatasi::baslik_okunamadi, "Header yarım");
        std::vector<std::uint8_t> kotu = p.bayt;
        kotu[1] = 'X';
        DOGRULA(d, Okuyucu::ac_bellek(kotu, h) == nullptr && h == AcmaHatasi::sihirli_bayt_yok, "sihirli baytlar bozuk");
        DOGRULA(d, Okuyucu::ac("/olmayan/dizin/yok.mcap", h) == nullptr && h == AcmaHatasi::dosya_acilamadi, "olmayan dosya");
        // Header'dan hemen sonra kesilen dosya: okunur ama boştur.
        const KayitKonumu& baslik = p.kayitlar[0];
        const Okuma s = oku(kes(baslik.govde + baslik.boy));
        DOGRULA(d, s.acildi && !s.ozet.kapanmis && !s.ozet.ust_veri_var && s.ozet.ileti_sayisi == 0 && s.ozet.guvence == Guvence::izleme,
                "yalnız Header: boş, kapanmamış");
    }
    return d.hata;
}

// A6-K59: parçada CRC hatası, açılamayan ya da çözülemeyen parça.
int test_bozulma_parca()
{
    Denetci d;
    const std::vector<std::size_t> parca_iletileri = {6, 8, 10, 4};
    for (const Veri veri : {Veri::sikistirilmaz, Veri::sikistirilir}) {
        const bool zstd = veri == Veri::sikistirilir;
        const std::string ad = zstd ? "zstd" : "sikistirmasiz";
        const ParcaliDosya p = parcali_dosya(d, "bozulma_" + ad, parca_iletileri, veri);
        if (d.hata != 0 || p.parcalar.size() != parca_iletileri.size()) {
            return d.hata + 1;
        }
        for (std::size_t i = 0; i < p.parcalar.size(); ++i) {
            // Sıkıştırmasız dosyada ilk parça (şema ve kanal kayıtlarıyla 1 KiB'ı aşar) sıkıştırılmış olabilir.
            if (zstd || i > 0) {
                DOGRULA(d, parca_sikistirmasi(p.bayt, p.parcalar[i]) == (zstd ? "zstd" : ""), ad + ": parça sıkıştırması beklenen gibi");
            }
        }
        const auto bozuk_denetle = [&](const std::vector<std::uint8_t>& b, std::size_t bozuk_parca, const std::string& ne) {
            const Okuma s = oku(b);
            DOGRULA(d, s.acildi, ne + ": okunur");
            if (!s.acildi) {
                return;
            }
            std::size_t once = 0;
            for (std::size_t i = 0; i < bozuk_parca; ++i) {
                once += parca_iletileri[i];
            }
            DOGRULA(d, s.ozet.bozuk && !s.ozet.saglam(), ne + ": dosya bozuk işaretlenir");
            DOGRULA(d, s.ozet.bozulma_konumu == p.parcalar[bozuk_parca].konum, ne + ": bozulma konumu: " + s.ozet.bozulma);
            // O parçadan ve sonrasından hiçbir ileti yayınlanmaz; izleme yalnız öncesiyle.
            DOGRULA(d, s.iletiler.size() == once && s.ozet.ileti_sayisi == once, ne + ": yalnız ilk bozuk parçadan önceki iletiler (" +
                                                                                    std::to_string(s.iletiler.size()) + ")");
            for (std::size_t i = 0; i < s.iletiler.size(); ++i) {
                DOGRULA(d, s.iletiler[i].veri == p.iletiler[i].veri, ne + ": öncesi değişmeden");
            }
            // Hiçbir tekrar güvencesi verilmez.
            DOGRULA(d, s.ozet.guvence == Guvence::izleme && !s.ozet.guvence_on_eki_son_adim.has_value(), ne + ": tekrar güvencesi yok");
            DOGRULA(d, s.ozet.kapanmis, ne + ": dosyanın sonu yerinde (kapanmış ama bozuk)");
        };
        // (a) Parçanın verisinde tek bayt (sıkıştırmasız dosyada bir ileti baytı; yapı geçerli kalır).
        {
            std::vector<std::uint8_t> b = p.bayt;
            const KayitKonumu& k = p.parcalar[2];
            b[static_cast<std::size_t>(k.govde + k.boy - 40)] ^= 0x01u;
            bozuk_denetle(b, 2, ad + ": veri baytı");
        }
        if (!zstd) {
            // Bozulan baytın bir ileti yükü olduğu gösterilir: parça 1'in ilk iletisinin yükü dosyada aranır.
            const OkunanIleti& hedef = p.iletiler[parca_iletileri[0]];
            const std::string_view yuk(reinterpret_cast<const char*>(hedef.veri.data()), hedef.veri.size());
            const std::int64_t konum = bul(p.bayt, yuk);
            DOGRULA(d, konum > 0, "ileti yükü dosyada açık duruyor (sıkıştırmasız parça)");
            if (konum > 0) {
                std::vector<std::uint8_t> b = p.bayt;
                b[static_cast<std::size_t>(konum) + yuk.size() / 2] ^= 0x80u;
                bozuk_denetle(b, 1, ad + ": ileti baytı");
            }
        }
        // (b) CRC alanının kendisi; (c) CRC = 0 "doğrulama yok" sayılmaz.
        {
            std::vector<std::uint8_t> b = p.bayt;
            b[static_cast<std::size_t>(p.parcalar[0].govde + 24)] ^= 0x10u;
            bozuk_denetle(b, 0, ad + ": CRC alanı");
            b = p.bayt;
            yaz_u32(b, p.parcalar[3].govde + 24, 0);
            bozuk_denetle(b, 3, ad + ": CRC sıfırlandı");
        }
        // (d) Sıkıştırmasız boy alanı: küçük, büyük ve sınırın çok üstünde (bellek ayrılmaz).
        for (const std::uint64_t boy : {std::uint64_t{0}, std::uint64_t{17}, std::uint64_t{1} << 40, UINT64_MAX}) {
            std::vector<std::uint8_t> b = p.bayt;
            yaz_u64(b, p.parcalar[1].govde + 16, boy);
            bozuk_denetle(b, 1, ad + ": sıkıştırmasız boy " + std::to_string(boy));
        }
        // (e) Sıkıştırma dizesi bilinmeyen bir yönteme çevrilir.
        if (zstd) {
            std::vector<std::uint8_t> b = p.bayt;
            b[static_cast<std::size_t>(p.parcalar[1].govde + 32)] = 'l';  // "lstd"
            bozuk_denetle(b, 1, ad + ": bilinmeyen sıkıştırma");
        }
        // (f) İlk baytı bozulan sıkıştırılmış akış / kayıt uzunluğu alanı.
        {
            std::vector<std::uint8_t> b = p.bayt;
            const KayitKonumu& k = p.parcalar[2];
            const std::uint64_t veri_basi = k.govde + 32 + parca_sikistirmasi(p.bayt, k).size() + 8;
            b[static_cast<std::size_t>(veri_basi)] ^= 0xFFu;
            bozuk_denetle(b, 2, ad + ": verinin ilk baytı");
            b = p.bayt;
            yaz_u64(b, veri_basi - 8, oku_u64(p.bayt, veri_basi - 8) - 1);
            bozuk_denetle(b, 2, ad + ": parçanın kayıt uzunluğu");
        }
    }
    // Sınırın üstünde sıkıştırmasız boy: okuyucunun ayarındaki üst sınır uygulanır.
    {
        const ParcaliDosya p = parcali_dosya(d, "bozulma_sinir", {100}, Veri::sikistirilir);
        OkumaAyari ayar;
        ayar.azami_parca_bayt = 4096;  // parça ≈ 68 KiB
        AcmaHatasi h = AcmaHatasi::yok;
        const auto o = Okuyucu::ac_bellek(p.bayt, h, ayar);
        DOGRULA(d, o != nullptr && o->ozet().bozuk && o->ozet().ileti_sayisi == 0, "üst sınırı aşan parça çözülemeyen parça sayılır");
    }
    return d.hata;
}

// A6-K59 (ek CRC'si), A6 §5 (üst veri), uçtan uca denetimler.
int test_bozulma_ek_ust_veri()
{
    Denetci d;
    const ParcaliDosya p = parcali_dosya(d, "bozulma_ek", {5, 5}, Veri::sikistirilir);
    if (d.hata != 0) {
        return d.hata;
    }
    // ---- ek ----
    for (const char* ek_adi : {"a1_komutlar.json", "a6_kanallar.json"}) {
        // Ekin veri bölgesinde bir bayt: adının ilk geçtiği yerden (kayıt başlığı) sonra.
        const std::int64_t ad_konumu = bul(p.bayt, ek_adi);
        DOGRULA(d, ad_konumu > 0, std::string(ek_adi) + ": ek dosyada");
        if (ad_konumu <= 0) {
            continue;
        }
        std::vector<std::uint8_t> b = p.bayt;
        b[static_cast<std::size_t>(ad_konumu) + std::strlen(ek_adi) + 4 + 16 + 8 + 20] ^= 0x04u;
        const Okuma s = oku(b);
        DOGRULA(d, s.acildi && s.ozet.bozuk && !s.ozet.saglam() && s.ozet.bozulma.find("ek") != std::string::npos,
                std::string(ek_adi) + ": CRC'si tutmayan ek dosyayı bozuk yapar: " + s.ozet.bozulma);
        DOGRULA(d, s.ozet.guvence == Guvence::izleme, std::string(ek_adi) + ": tekrar güvencesi yok");
        const bool listede = std::any_of(s.ozet.ekler.begin(), s.ozet.ekler.end(), [&](const EkOzeti& e) { return e.ad == ek_adi; });
        DOGRULA(d, !listede && s.ozet.ekler.size() == ekler().size() - 1, std::string(ek_adi) + ": tutmayan ek kullanılmaz");
        AcmaHatasi h = AcmaHatasi::yok;
        const auto o = Okuyucu::ac_bellek(b, h);
        std::vector<std::uint8_t> veri;
        DOGRULA(d, o != nullptr && !o->ek_oku(ek_adi, veri) && o->ek_oku("a7_manifest.json", veri), std::string(ek_adi) + ": öteki ekler okunur");
        // Parçalar sağlam: izleme bütün iletilerle yapılır.
        DOGRULA(d, s.iletiler.size() == p.toplam, std::string(ek_adi) + ": iletiler okunur");
    }
    // Ek açılıştan sonra değişirse okuma başarısız olur (yeniden doğrulama).
    {
        std::vector<std::uint8_t> b = p.bayt;
        AcmaHatasi h = AcmaHatasi::yok;
        const auto o = Okuyucu::ac_bellek(b, h);
        std::vector<std::uint8_t> veri;
        DOGRULA(d, o != nullptr && o->ek_oku("a1_dialect.xml", veri) && !veri.empty(), "ek okunur");
        const std::int64_t k = bul(b, "a1_dialect.xml");
        b[static_cast<std::size_t>(k) + 60] ^= 0x01u;
        DOGRULA(d, o != nullptr && !o->ek_oku("a1_dialect.xml", veri), "sonradan bozulan ek okunmaz");
    }
    // ---- üst veri ----
    const auto ust_veri_kaydi = [&](std::string_view ad) -> KayitKonumu {
        for (const KayitKonumu& k : p.kayitlar) {
            if (k.op == 0x0C && oku_u32(p.bayt, k.govde) == ad.size() &&
                std::memcmp(p.bayt.data() + k.govde + 4, ad.data(), ad.size()) == 0) {
                return k;
            }
        }
        return {};
    };
    const KayitKonumu acilis = ust_veri_kaydi("ida_kayit");
    const KayitKonumu kapanis = ust_veri_kaydi("ida_kayit_kapanis");
    DOGRULA(d, acilis.boy > 0 && kapanis.boy > 0, "üst veri kayıtları bulundu");
    {
        // Zorunlu anahtarın adı bozulur: gerekli anahtar eksik (ve bilinmeyen anahtar var) → yalnız izleme.
        std::vector<std::uint8_t> b = p.bayt;
        const std::int64_t k = bul(b, "senaryo_ozeti", acilis.govde);
        b[static_cast<std::size_t>(k)] = 'z';
        const Okuma s = oku(b);
        DOGRULA(d, s.acildi && !s.ozet.bozuk && s.ozet.ust_veri_var && !s.ozet.ust_veri_gecerli && s.ozet.guvence == Guvence::izleme &&
                       !s.ozet.uyarilar.empty(),
                "gerekli anahtarı eksik açılış kaydı: yalnız izleme");
        DOGRULA(d, s.iletiler.size() == p.toplam, "iletiler yine okunur");
    }
    {
        // Kayıt yapısal olarak çözülemez (bir dize uzunluğu taşar): açılış kaydı yok sayılır → yalnız izleme.
        std::vector<std::uint8_t> b = p.bayt;
        yaz_u32(b, acilis.govde + 4 + 9 + 4, 0x7FFFFFFFu);  // ilk anahtarın uzunluğu
        const Okuma s = oku(b);
        DOGRULA(d, s.acildi && !s.ozet.ust_veri_var && !s.ozet.ust_veri_gecerli && s.ozet.guvence == Guvence::izleme,
                "çözülemeyen açılış kaydı: yalnız izleme");
    }
    {
        // A6-K48: farklı büyük sürüm okunmaz; aynı büyük sürümün başka küçük sürümü okunur.
        const std::int64_t k = bul(p.bayt, "a6_surumu", acilis.govde);
        std::vector<std::uint8_t> b = p.bayt;
        b[static_cast<std::size_t>(k) + 9 + 4] = '2';  // "1.0" → "2.0"
        AcmaHatasi h = AcmaHatasi::yok;
        DOGRULA(d, Okuyucu::ac_bellek(b, h) == nullptr && h == AcmaHatasi::buyuk_surum_farkli, "farklı büyük a6_surumu okunmaz");
        b = p.bayt;
        b[static_cast<std::size_t>(k) + 9 + 4 + 2] = '7';  // "1.0" → "1.7"
        const Okuma s = oku(b);
        DOGRULA(d, s.acildi && s.ozet.saglam() && s.ozet.ust_veri_gecerli && s.ozet.guvence == Guvence::fizik, "aynı büyük sürüm okunur");
        b = p.bayt;
        b[static_cast<std::size_t>(k) + 9 + 4 + 1] = 'x';  // "1x0": sürüm çözülemez
        const Okuma s2 = oku(b);
        DOGRULA(d, s2.acildi && !s2.ozet.ust_veri_gecerli && s2.ozet.guvence == Guvence::izleme, "çözülemeyen sürüm: yalnız izleme");
    }
    {
        // Kapanış kaydındaki ileti sayısı dosyadakiyle uyuşmazsa dosya bozuktur (bir parçanın bütünüyle
        // kaybolması parça CRC'siyle yakalanamaz).
        const std::int64_t k = bul(p.bayt, "ileti_sayisi", kapanis.govde);
        std::vector<std::uint8_t> b = p.bayt;
        b[static_cast<std::size_t>(k) + 12 + 4] = '9';  // "10" → "90"
        const Okuma s = oku(b);
        DOGRULA(d, s.acildi && s.ozet.bozuk && s.ozet.guvence == Guvence::izleme, "ileti_sayisi uyuşmazlığı: " + s.ozet.bozulma);
        // Bir parça kaydı (ve ileti indeksleri) dosyadan temizce çıkarılırsa da aynı denetim yakalar.
        const std::vector<KayitKonumu> parcalar = p.parcalar;
        std::vector<std::uint8_t> eksik(p.bayt.begin(), p.bayt.begin() + static_cast<std::ptrdiff_t>(parcalar[1].konum));
        std::uint64_t sonraki = 0;
        for (const KayitKonumu& kk : p.kayitlar) {
            if (kk.konum > parcalar[1].konum && kk.op != 0x07) {
                sonraki = kk.konum;
                break;
            }
        }
        eksik.insert(eksik.end(), p.bayt.begin() + static_cast<std::ptrdiff_t>(sonraki), p.bayt.end());
        const Okuma s2 = oku(eksik);
        DOGRULA(d, s2.acildi && s2.ozet.kapanmis && s2.ozet.bozuk && s2.ozet.guvence == Guvence::izleme && s2.iletiler.size() == 5,
                "bütünüyle kaybolan parça sessizce geçmez: " + s2.ozet.bozulma);
    }
    {
        // Kapanış kaydındaki güvence dosyanın içeriğinden fazlasını söyleyemez.
        const std::int64_t k = bul(p.bayt, "tekrar_guvencesi", kapanis.govde);
        std::vector<std::uint8_t> b = p.bayt;
        std::memcpy(b.data() + k + 16 + 4, "girdi", 5);  // "fizik" → "girdi" (telemetri kademesi girdi veremez)
        const Okuma s = oku(b);
        DOGRULA(d, s.acildi && s.ozet.guvence == Guvence::fizik && !s.ozet.uyarilar.empty(), "kapanış kaydı güvenceyi yükseltemez");
    }
    {
        // Data End'deki veri bölümü CRC'si (kitaplık varsayılanında 0 = yok): yazılmışsa doğrulanır.
        std::uint64_t de = 0;
        for (const KayitKonumu& k : p.kayitlar) {
            if (k.op == 0x0F) {
                de = k.konum;
            }
        }
        DOGRULA(d, de != 0 && oku_u32(p.bayt, de + 9) == 0, "kitaplık varsayılanı: veri bölümü CRC'si yazılmaz");
        std::vector<std::uint8_t> b = p.bayt;
        yaz_u32(b, de + 9, ic::crc32(p.bayt.data(), static_cast<std::size_t>(de)));
        Okuma s = oku(b);
        DOGRULA(d, s.acildi && s.ozet.saglam(), "doğru veri bölümü CRC'si kabul edilir");
        // Açılış kaydının son baytı: son anahtarın (`tohum`) değerinin son karakteri ("42" → "43").
        b[static_cast<std::size_t>(acilis.govde + acilis.boy - 1)] ^= 0x01u;
        s = oku(b);
        DOGRULA(d, s.acildi && s.ozet.bozuk, "veri bölümü CRC'si varsa üst verideki değişiklik yakalanır");
        // CRC'siz (varsayılan) dosyada aynı değişiklik yapısal olarak görünmez: sınır olarak belgelidir.
        b = p.bayt;
        b[static_cast<std::size_t>(acilis.govde + acilis.boy - 1)] ^= 0x01u;
        s = oku(b);
        DOGRULA(d, s.acildi && !s.ozet.bozuk && s.ozet.ust_veri.at("tohum") == "43",
                "veri bölümü CRC'siz dosyada üst veri değeri CRC ile korunmaz (bilinen sınır)");
    }
    return d.hata;
}

// A6-K46: açılış kaydı ile güncelleme kayıtlarının birleştirilmesi (elle kurulmuş dosyalarla).
int test_ust_veri_birlestirme()
{
    Denetci d;
    const ElleTaban t;
    const auto kur = [&](const std::function<void(ElleMcap&)>& ust_veriler) {
        ElleMcap f;
        f.sihir().baslik("ida", "elle");
        ust_veriler(f);
        f.parca(t.parca()).ust_veri("ida_kayit_kapanis", {{"bitis_durumu", "bitti"},
                                                           {"son_adim_no", "2"},
                                                           {"son_sim_t_ns", "2000"},
                                                           {"gecerli", "true"},
                                                           {"gercek_zamani_kacirdi", "false"},
                                                           {"gecerlilik_nedenleri", ""},
                                                           {"sayaclar", ""},
                                                           {"kayit_eksik", "false"},
                                                           {"tekrar_guvencesi", "izleme"},
                                                           {"ileti_sayisi", "2"}});
        f.veri_sonu().footer();
        return f.al();
    };
    {
        const Okuma s = oku(kur([&](ElleMcap& f) { f.ust_veri("ida_kayit", t.acilis); }));
        DOGRULA(d, s.acildi && s.ozet.saglam() && s.ozet.ust_veri_gecerli && s.iletiler.size() == 2 && s.ozet.kapanis.has_value(),
                "taban dosya sağlam" + (s.acildi && !s.ozet.uyarilar.empty() ? ": " + s.ozet.uyarilar[0] : std::string()));
        DOGRULA(d, s.acildi && s.ozet.ust_veri.count("kart_fw_version") == 0, "koşullu anahtar açılışta yok");
    }
    {
        // Güncelleme yalnız açılışta yok ya da boş olan anahtarı doldurur; aynı değerle yineleme etkisizdir.
        ElleMcap::Eslem acilis = t.acilis;
        acilis["a2_surumu"] = "";  // boş metinle yazılmış koşullu anahtar (A6-K46)
        const Okuma s = oku(kur([&](ElleMcap& f) {
            f.ust_veri("ida_kayit", acilis);
            f.ust_veri("ida_kayit_guncelleme", {{"kart_fw_version", "17"}});
            f.ust_veri("ida_kayit_guncelleme", {{"a2_surumu", "1.2"}, {"kart_fw_git", "305419896"}});
            f.ust_veri("ida_kayit_guncelleme", {{"kart_fw_version", "17"}});
        }));
        DOGRULA(d, s.acildi && !s.ozet.bozuk && s.ozet.ust_veri_gecerli, "güncellemeli dosya sağlam");
        if (s.acildi) {
            DOGRULA(d, s.ozet.ust_veri.at("kart_fw_version") == "17" && s.ozet.ust_veri.at("a2_surumu") == "1.2" &&
                           s.ozet.ust_veri.at("kart_fw_git") == "305419896",
                    "güncellemeler dosya sırasıyla uygulandı");
        }
    }
    {
        // Dolu anahtarı farklı değerle yeniden yazan güncelleme dosyayı bozuk yapar.
        const Okuma s = oku(kur([&](ElleMcap& f) {
            f.ust_veri("ida_kayit", t.acilis);
            f.ust_veri("ida_kayit_guncelleme", {{"kart_fw_version", "17"}});
            f.ust_veri("ida_kayit_guncelleme", {{"kart_fw_version", "18"}});
        }));
        DOGRULA(d, s.acildi && s.ozet.bozuk && s.ozet.guvence == Guvence::izleme && s.ozet.bozulma.find("A6-K46") != std::string::npos,
                "çelişen güncelleme: bozuk");
        // İzleme sürer (bozuk parça yok): iletiler okunur.
        DOGRULA(d, s.acildi && s.iletiler.size() == 2, "çelişen güncellemede iletiler okunur");
        ElleMcap::Eslem acilis = t.acilis;
        acilis["a2_surumu"] = "1.2";
        const Okuma s2 = oku(kur([&](ElleMcap& f) {
            f.ust_veri("ida_kayit", acilis);
            f.ust_veri("ida_kayit_guncelleme", {{"a2_surumu", "1.3"}});
        }));
        DOGRULA(d, s2.acildi && s2.ozet.bozuk, "açılışta dolu anahtarı değiştiren güncelleme: bozuk");
    }
    {
        // İkinci `ida_kayit` açılış kaydı dosyayı bozuk yapar.
        const Okuma s = oku(kur([&](ElleMcap& f) {
            f.ust_veri("ida_kayit", t.acilis);
            f.ust_veri("ida_kayit", t.acilis);
        }));
        DOGRULA(d, s.acildi && s.ozet.bozuk && s.ozet.guvence == Guvence::izleme, "ikinci açılış kaydı: bozuk");
    }
    {
        // Kapalı liste dışındaki anahtar şemadan geçmez: yok sayılır ve uyarılır.
        const Okuma s = oku(kur([&](ElleMcap& f) {
            f.ust_veri("ida_kayit", t.acilis);
            f.ust_veri("ida_kayit_guncelleme", {{"tohum", "99"}, {"kart_fw_version", "17"}});
        }));
        DOGRULA(d, s.acildi && !s.ozet.bozuk && !s.ozet.uyarilar.empty(), "kapalı liste dışı güncelleme uyarılır");
        if (s.acildi) {
            DOGRULA(d, s.ozet.ust_veri.at("tohum") == "42" && s.ozet.ust_veri.at("kart_fw_version") == "17", "liste dışı anahtar uygulanmaz");
        }
    }
    {
        // Açılış kaydı olmayan dosya: yalnız izleme.
        const Okuma s = oku(kur([&](ElleMcap&) {}));
        DOGRULA(d, s.acildi && !s.ozet.ust_veri_var && s.ozet.guvence == Guvence::izleme && s.iletiler.size() == 2, "açılış kaydı yok");
    }
    return d.hata;
}

// A6-K47…K49 ve §5: sürüm uyumluluğu, bilinmeyen kanal, şema uyuşmazlığı, biçim dışı dosyalar.
int test_surum_kurallari()
{
    Denetci d;
    const ElleTaban t;
    const SemaTanimi& sema = semalar()[t.kanal->sema];
    const KanalTanimi* imu = kanal_bul("sensor/imu");
    const SemaTanimi& imu_sema = semalar()[imu->sema];
    const Ornek imu_ornek = ornek_uret(Kanal::sensor_imu, 9);
    const auto dosya = [&](const std::vector<std::uint8_t>& parca) {
        ElleMcap f;
        f.sihir().baslik("ida", "elle").ust_veri("ida_kayit", t.acilis).parca(parca).veri_sonu().footer();
        return f.al();
    };
    {
        // Sonuna alan eklenmiş (`kanal_surumu` + 1) ileti eski okuyucuda okunur: bilinen alanlar aynı,
        // sondaki fazla baytlar yok sayılır (A6-K48).
        std::vector<std::uint8_t> uzun = t.ornek.bayt;
        uzun.insert(uzun.end(), {0xAA, 0xBB, 0xCC});
        ElleMcap p;
        p.sema(1, "ida.yuk_calistirma_durum.v2", kSemaKodlamasi, std::string(sema.veri) + " ")
            .kanal(1, 1, t.kanal->ad, kIletiKodlamasi, {{"kanal_surumu", "2"}, {"zaman_anlami", "sim"}, {"kademe", "olaylar"}})
            .ileti(1, 1, 1000, 0, uzun);
        const std::vector<std::uint8_t> b = dosya(p.al());
        AcmaHatasi h = AcmaHatasi::yok;
        const auto o = Okuyucu::ac_bellek(b, h);
        DOGRULA(d, o != nullptr && !o->ozet().bozuk && o->ozet().kanallar.size() == 1 && o->ozet().kanallar[0].okunur &&
                       o->ozet().kanallar[0].kanal_surumu == 2,
                "yeni kanal_surumu okunur");
        if (o) {
            Ileti i;
            DOGRULA(d, o->sonraki(i) && i.kanal == t.kanal && i.kanal_surumu == 2 && i.veri.size() == uzun.size(), "ileti verilir");
            std::string json;
            const CozmeSonucu c = json_dok(*t.kanal->yuk, i.veri, json);
            DOGRULA(d, c.tamam && c.fazla_bayt == 3 && json == t.ornek.json, "bilinen alanlar okunur, fazla baytlar yok sayılır");
        }
    }
    {
        // Bilinmeyen kanal atlanır; öteki kanallar okunur.
        ElleMcap p;
        p.sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, t.kanal->ad, kIletiKodlamasi, t.metadata);
        p.sema(2, "ida.yuk_gelecek.v1", kSemaKodlamasi, "[]").kanal(2, 2, "gelecek/kanal", kIletiKodlamasi, {{"kanal_surumu", "1"}});
        p.ileti(2, 1, 500, 0, {1, 2, 3}).ileti(1, 2, 1000, 0, t.ornek.bayt).ileti(2, 3, 1500, 0, {4});
        const Okuma s = oku(dosya(p.al()));
        DOGRULA(d, s.acildi && !s.ozet.bozuk && s.ozet.ileti_sayisi == 3 && s.ozet.okunur_ileti_sayisi == 1 && s.iletiler.size() == 1 &&
                       s.iletiler[0].sequence == 2,
                "bilinmeyen kanal atlanır");
        const bool uyari = s.acildi && std::any_of(s.ozet.kanallar.begin(), s.ozet.kanallar.end(), [](const KanalOzeti& k) {
            return k.konu == "gelecek/kanal" && !k.okunur && k.tanim == nullptr && k.ileti_sayisi == 2;
        });
        DOGRULA(d, uyari, "bilinmeyen kanal özetinde işaretli");
    }
    {
        // A6-K49: aynı `kanal_surumu`'nda şema `data`'sı farklıysa kanal okunmaz ve uyarılır.
        std::string farkli(imu_sema.veri);
        farkli[farkli.size() / 2] = farkli[farkli.size() / 2] == 'a' ? 'b' : 'a';
        ElleMcap p;
        p.sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, t.kanal->ad, kIletiKodlamasi, t.metadata);
        p.sema(2, imu_sema.ad, kSemaKodlamasi, farkli)
            .kanal(2, 2, imu->ad, kIletiKodlamasi, {{"kanal_surumu", "1"}, {"zaman_anlami", "olcum"}, {"kademe", "telemetri"}});
        p.ileti(1, 1, 1000, 0, t.ornek.bayt).ileti(2, 2, 1500, 0, imu_ornek.bayt);
        const Okuma s = oku(dosya(p.al()));
        DOGRULA(d, s.acildi && !s.ozet.bozuk && s.iletiler.size() == 1 && s.iletiler[0].kanal == "calistirma/durum", "şeması farklı kanal okunmaz");
        const bool uyari = s.acildi && std::any_of(s.ozet.kanallar.begin(), s.ozet.kanallar.end(), [](const KanalOzeti& k) {
            return k.konu == "sensor/imu" && !k.okunur && k.uyari.find("A6-K49") != std::string::npos;
        });
        DOGRULA(d, uyari && !s.ozet.uyarilar.empty(), "şema uyuşmazlığı uyarılır");
        // Şema adı farklıysa da (yanlış yük) okunmaz; kodlamalar farklıysa da.
        ElleMcap p2;
        p2.sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, imu->ad, kIletiKodlamasi, {{"kanal_surumu", "1"}});
        p2.sema(2, sema.ad, "jsonschema", sema.veri);
        p2.kanal(2, 2, t.kanal->ad, kIletiKodlamasi, t.metadata).kanal(3, 1, "calistirma/durum2", "json", t.metadata);
        p2.ileti(1, 1, 1, 0, imu_ornek.bayt).ileti(2, 2, 2, 0, t.ornek.bayt);
        const Okuma s2 = oku(dosya(p2.al()));
        DOGRULA(d, s2.acildi && !s2.ozet.bozuk && s2.iletiler.empty() && s2.ozet.ileti_sayisi == 2, "yanlış şema ya da kodlama: okunmaz");
        // `kanal_surumu` üst verisi olmayan kanal okunmaz.
        ElleMcap p3;
        p3.sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, t.kanal->ad, kIletiKodlamasi, {{"zaman_anlami", "sim"}});
        p3.ileti(1, 1, 1, 0, t.ornek.bayt);
        const Okuma s3 = oku(dosya(p3.al()));
        DOGRULA(d, s3.acildi && s3.iletiler.empty(), "kanal_surumu'suz kanal okunmaz");
    }
    {
        // Biçim dışı dosyalar.
        AcmaHatasi h = AcmaHatasi::yok;
        ElleMcap f;
        f.sihir().baslik("ros2", "elle").veri_sonu().footer();
        DOGRULA(d, Okuyucu::ac_bellek(f.al(), h) == nullptr && h == AcmaHatasi::profil_farkli, "profil ida değil");
        // Parça dışında ileti (A6 dosyası parçalıdır; CRC'siz ileti yayınlanmaz).
        ElleMcap g;
        g.sihir().baslik("ida", "elle").sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, t.kanal->ad, kIletiKodlamasi, t.metadata);
        g.ileti(1, 1, 1000, 0, t.ornek.bayt).veri_sonu().footer();
        const Okuma s = oku(g.al());
        DOGRULA(d, s.acildi && s.ozet.bozuk && s.iletiler.empty(), "parça dışındaki ileti yayınlanmaz");
        // Tanımlanmamış kanal kimliği taşıyan ileti: çözülemeyen parça.
        ElleMcap p;
        p.ileti(7, 1, 1000, 0, t.ornek.bayt);
        const Okuma s2 = oku(dosya(p.al()));
        DOGRULA(d, s2.acildi && s2.ozet.bozuk && s2.iletiler.empty(), "tanımsız kanal kimliği");
        // Aynı kimliğin farklı içerikle yeniden tanımlanması.
        ElleMcap p2;
        p2.sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, t.kanal->ad, kIletiKodlamasi, t.metadata);
        p2.kanal(1, 1, imu->ad, kIletiKodlamasi, t.metadata).ileti(1, 1, 1000, 0, t.ornek.bayt);
        const Okuma s3 = oku(dosya(p2.al()));
        DOGRULA(d, s3.acildi && s3.ozet.bozuk && s3.iletiler.empty(), "kanal kimliği yeniden tanımlandı");
        // `log_time` i64 aralığının dışında.
        ElleMcap p3;
        p3.sema(1, sema.ad, kSemaKodlamasi, sema.veri).kanal(1, 1, t.kanal->ad, kIletiKodlamasi, t.metadata);
        p3.ileti(1, 1, UINT64_MAX, 0, t.ornek.bayt);
        const Okuma s4 = oku(dosya(p3.al()));
        DOGRULA(d, s4.acildi && s4.ozet.bozuk, "aralık dışı log_time");
    }
    return d.hata;
}

// Kabul 6: rastgele kesilmiş ve bozulmuş dosyalarla (sabit tohum, en az 10 000 vaka) okuyucu çökmez,
// sınırsız bellek ayırmaz, kilitlenmez; bozulmuş veri hiçbir zaman "özgün" diye yayınlanmaz.
int test_rastgele_bozulma()
{
    Denetci d;
    const ParcaliDosya zs = parcali_dosya(d, "rastgele_zstd", {9, 14, 6, 11}, Veri::sikistirilir);
    const ParcaliDosya ham = parcali_dosya(d, "rastgele_ham", {4, 3, 5}, Veri::sikistirilmaz);
    if (d.hata != 0) {
        return d.hata;
    }
    const ParcaliDosya* kaynaklar[] = {&zs, &ham};
    std::vector<std::uint8_t> tabanlar[2] = {katalog_eksiz(zs.bayt), katalog_eksiz(ham.bayt)};
    for (int k = 0; k < 2; ++k) {
        const Okuma s = oku(tabanlar[k]);
        DOGRULA(d, s.acildi && s.ozet.kapanmis && s.iletiler.size() == kaynaklar[k]->toplam,
                "küçültülmüş taban dosya okunur (" + std::to_string(tabanlar[k].size()) + " bayt)");
    }
    constexpr int kVaka = 12000;
    Rastgele r(0x1DA6);
    OkumaAyari ayar;
    ayar.azami_parca_bayt = std::uint64_t{4} << 20;
    ayar.azami_acik_bayt = std::uint64_t{16} << 20;
    std::map<std::string, int> sonuclar;
    std::uint64_t toplam_ileti = 0;
    for (int vaka = 0; vaka < kVaka; ++vaka) {
        const int k = static_cast<int>(r.alt(2));
        const ParcaliDosya& kaynak = *kaynaklar[k];
        std::vector<std::uint8_t> b = tabanlar[k];
        const std::vector<KayitKonumu> kayitlar = kayitlari_tara(b);
        bool yerinde = true;  // yalnız yerinde değişiklikler (bayt eklenmedi, çıkarılmadı, taşınmadı)
        const int degisiklik = 1 + static_cast<int>(r.alt(3));
        for (int i = 0; i < degisiklik && !b.empty(); ++i) {
            const std::uint64_t konum = r.alt(b.size());
            switch (r.alt(9)) {
            case 0:  // kes
                b.resize(static_cast<std::size_t>(konum));
                yerinde = false;
                break;
            case 1:  // bit çevir
                b[static_cast<std::size_t>(konum)] ^= static_cast<std::uint8_t>(1u << r.alt(8));
                break;
            case 2: {  // aralığı rastgele baytlarla ez
                const std::uint64_t n = std::min<std::uint64_t>(1 + r.alt(64), b.size() - konum);
                for (std::uint64_t j = 0; j < n; ++j) {
                    b[static_cast<std::size_t>(konum + j)] = static_cast<std::uint8_t>(r.ham());
                }
                break;
            }
            case 3: {  // bir kaydın uzunluk alanını boz
                if (!kayitlar.empty()) {
                    const KayitKonumu& kk = kayitlar[r.alt(kayitlar.size())];
                    if (kk.konum + 9 <= b.size()) {
                        const std::uint64_t secim = r.alt(4);
                        const std::uint64_t yeni = secim == 0   ? r.ham()
                                                   : secim == 1 ? kk.boy + r.alt(64)
                                                   : secim == 2 ? (kk.boy > 0 ? r.alt(kk.boy) : 0)
                                                                : UINT64_MAX - r.alt(16);
                        yaz_u64(b, kk.konum + 1, yeni);
                    }
                }
                break;
            }
            case 4: {  // bir kaydın türünü değiştir
                if (!kayitlar.empty()) {
                    const KayitKonumu& kk = kayitlar[r.alt(kayitlar.size())];
                    if (kk.konum < b.size()) {
                        b[static_cast<std::size_t>(kk.konum)] = static_cast<std::uint8_t>(r.alt(20));
                    }
                }
                break;
            }
            case 5: {  // aralığı sıfırla
                const std::uint64_t n = std::min<std::uint64_t>(1 + r.alt(200), b.size() - konum);
                std::fill_n(b.begin() + static_cast<std::ptrdiff_t>(konum), static_cast<std::ptrdiff_t>(n), std::uint8_t{0});
                break;
            }
            case 6: {  // bir dilimi başka yere kopyala (ekle)
                const std::uint64_t n = std::min<std::uint64_t>(1 + r.alt(3000), b.size() - konum);
                const std::vector<std::uint8_t> dilim(b.begin() + static_cast<std::ptrdiff_t>(konum),
                                                      b.begin() + static_cast<std::ptrdiff_t>(konum + n));
                const std::uint64_t hedef = r.alt(b.size());
                b.insert(b.begin() + static_cast<std::ptrdiff_t>(hedef), dilim.begin(), dilim.end());
                yerinde = false;
                break;
            }
            case 7: {  // bir dilimi sil
                const std::uint64_t n = std::min<std::uint64_t>(1 + r.alt(3000), b.size() - konum);
                b.erase(b.begin() + static_cast<std::ptrdiff_t>(konum), b.begin() + static_cast<std::ptrdiff_t>(konum + n));
                yerinde = false;
                break;
            }
            default: {  // bir parçanın başlık alanlarından birini boz (boy, CRC, sıkıştırma, kayıt boyu)
                std::vector<KayitKonumu> parcalar;
                for (const KayitKonumu& kk : kayitlar) {
                    if (kk.op == 0x06) {
                        parcalar.push_back(kk);
                    }
                }
                if (!parcalar.empty()) {
                    const KayitKonumu& kk = parcalar[r.alt(parcalar.size())];
                    const std::uint64_t alan = kk.govde + 16 + r.alt(28);
                    if (alan < b.size()) {
                        b[static_cast<std::size_t>(alan)] = static_cast<std::uint8_t>(r.ham());
                    }
                }
                break;
            }
            }
        }
        AcmaHatasi h = AcmaHatasi::yok;
        const auto o = Okuyucu::ac_bellek(b, h, ayar);
        if (!o) {
            ++sonuclar[std::string("okunamadi: ") + std::string(acma_hatasi_adi(h))];
            continue;
        }
        const DosyaOzeti& oz = o->ozet();
        // Sınırlı iş: kopyalanan dilimler parçaları çoğaltabilir; ileti sayısı yine de özgünün küçük
        // bir katını aşamaz.
        const std::size_t tavan = static_cast<std::size_t>(kaynak.toplam) * 16u + 64u;
        std::size_t n = 0;
        std::multiset<std::uint32_t> siralar;
        bool hepsi_ozgun = true;
        Ileti i;
        while (n <= tavan && o->sonraki(i)) {
            ++n;
            siralar.insert(i.sequence);
            const auto ozgun = kaynak.sira_ile.find(i.sequence);
            hepsi_ozgun = hepsi_ozgun && ozgun != kaynak.sira_ile.end() && ozgun->second.kanal == i.kanal->ad &&
                          ozgun->second.log_time == i.log_time && ozgun->second.publish_time == i.publish_time &&
                          ozgun->second.veri.size() == i.veri.size() &&
                          std::equal(i.veri.begin(), i.veri.end(), ozgun->second.veri.begin());
        }
        toplam_ileti += n;
        DOGRULA(d, n <= tavan, "vaka " + std::to_string(vaka) + ": okuma sınırsız sürdü");
        // Parça CRC'si: yayınlanan her ileti özgün dosyadaki iletinin aynısıdır.
        DOGRULA(d, hepsi_ozgun, "vaka " + std::to_string(vaka) + ": bozulmuş ileti yayınlandı");
        for (const EkOzeti& e : oz.ekler) {
            std::vector<std::uint8_t> veri;
            if (o->ek_oku(e.ad, veri)) {
                DOGRULA(d, veri.size() == e.boyut && veri.size() < 4096, "vaka " + std::to_string(vaka) + ": ek boyu");
            }
        }
        if (oz.guvence != Guvence::izleme) {
            // Tekrar güvencesi yalnız kapanmış, bozuk olmayan, üst verisi geçerli dosyadan çıkar ve
            // o zaman dosyadaki iletiler kapanış kaydındaki sayıyla aynıdır.
            DOGRULA(d, oz.saglam() && oz.ust_veri_gecerli && oz.kapanis.has_value(), "vaka " + std::to_string(vaka) + ": güvence koşulu");
            DOGRULA(d, oz.guvence == Guvence::fizik || oz.guvence == Guvence::yok, "vaka " + std::to_string(vaka) + ": güvence yükselemez");
            if (yerinde) {
                DOGRULA(d, n == kaynak.toplam && siralar.size() == kaynak.sira_ile.size(),
                        "vaka " + std::to_string(vaka) + ": güvenceli dosyada eksik ileti (" + std::to_string(n) + ")");
            }
        }
        if (oz.saglam() && oz.kapanis.has_value() && yerinde) {
            DOGRULA(d, n == kaynak.toplam, "vaka " + std::to_string(vaka) + ": sağlam görünen dosyada eksik ileti");
        }
        ++sonuclar[oz.bozuk ? "bozuk" : (oz.kapanmis ? (oz.guvence == Guvence::izleme ? "kapanmis, yalniz izleme" : "saglam, guvenceli") : "kapanmamis")];
    }
    std::printf("  %d vaka, yayınlanan ileti %llu; sonuçlar:\n", kVaka, static_cast<unsigned long long>(toplam_ileti));
    for (const auto& [ad, sayi] : sonuclar) {
        std::printf("    %-40s %d\n", ad.c_str(), sayi);
    }
    DOGRULA(d, sonuclar["bozuk"] > 1000 && sonuclar["kapanmamis"] > 500, "vakalar bozuk ve kapanmamış sonuçları kapsıyor");
    return d.hata;
}

// Kabul 5: yazıcı süreci yazım ortasında öldürülür: kurtarma okuması kapanmış bütün parçaları verir,
// kayıp en çok son açık parçadır; okuyucu dosyayı "kapanmamış" olarak işaretler.
int test_cokme_kurtarma(const char* yazan_program)
{
    Denetci d;
    GeciciDizin dizin("cokme");
    const std::filesystem::path hazir = dizin.yol() / "hazir";
    const std::uint64_t surec = surec_baslat({yazan_program, yol_metni(dizin.yol()), yol_metni(hazir)});
    DOGRULA(d, surec != 0, "yazan süreç başlatıldı");
    if (surec == 0) {
        return d.hata;
    }
    // Yazan süreç iki parçayı kapatıp üçüncüsünü açık bıraktığında `hazir` dosyasını oluşturur.
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    bool hazir_mi = false;
    while (std::chrono::steady_clock::now() < son) {
        std::error_code ec;
        if (std::filesystem::exists(hazir, ec)) {
            hazir_mi = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    surec_oldur(surec);  // SIGKILL / TerminateProcess: yazıcı hiçbir kapanış işi yapamaz
    DOGRULA(d, hazir_mi, "yazan süreç hazır oldu");
    if (!hazir_mi) {
        return d.hata;
    }
    const std::filesystem::path yol = dizin.yol() / (std::string(kOrnekRunId) + ".mcap");
    AcmaHatasi h = AcmaHatasi::yok;
    const auto o = Okuyucu::ac(yol, h);
    DOGRULA(d, o != nullptr, "öldürülen sürecin dosyası okunur: " + std::string(acma_hatasi_adi(h)));
    if (!o) {
        return d.hata;
    }
    const DosyaOzeti& oz = o->ozet();
    DOGRULA(d, !oz.kapanmis && !oz.bozuk && !oz.saglam(), "dosya kapanmamış olarak işaretlenir");
    DOGRULA(d, oz.guvence == Guvence::izleme && !oz.kapanis.has_value(), "kapanmamış dosya yalnız izleme verir");
    DOGRULA(d, oz.ust_veri_gecerli && oz.ekler.size() == ekler().size(), "açılış üst verisi ve ekler okunur");
    // Yazan süreç: parça 1 = 40 ileti, parça 2 = 60 ileti (ikisi de 1 s kuralıyla kapandı), açık parça = 25 ileti.
    const std::vector<OkunanIleti> v = hepsini_oku(*o);
    DOGRULA(d, oz.parca_sayisi == 2 && v.size() == 100, "kapanmış iki parçanın bütün iletileri kurtarıldı: " + std::to_string(v.size()));
    for (std::size_t i = 0; i < v.size(); ++i) {
        DOGRULA(d, v[i].kanal == "calistirma/durum" && v[i].log_time == static_cast<std::int64_t>(i) * 1000 && v[i].sequence == i + 1,
                "kurtarılan iletiler sırayla ve eksiksiz");
    }
    // Kayıp yalnız açık parçadır (25 ileti): dosyada 100'den sonrası yoktur.
    const KitaplikDokumu kd = kitaplik_ile_oku(yol_metni(yol));
    std::printf("  kitaplığın okuyucusu (yedek taramasız) kapanmamış dosyada: %s\n", kd.acildi ? "açtı" : kd.hata.c_str());
    return d.hata;
}

// `ida_kayit_dok`: çıkış kodları sağlam 0, okunamadı 1, bozuk 2, kapanmamış 3.
int test_dok_programi(const char* dok_programi)
{
    Denetci d;
    GeciciDizin dizin("dok");
    const ParcaliDosya p = parcali_dosya(d, "dok_kaynak", {6, 4}, Veri::sikistirilir);
    if (d.hata != 0) {
        return d.hata;
    }
    const auto calistir = [&](const std::string& ad, const std::vector<std::uint8_t>& icerik, const std::vector<std::string>& ek) {
        const std::filesystem::path yol = dizin.yol() / ad;
        dosya_yaz(yol, icerik);
        std::vector<std::string> a = {dok_programi, yol_metni(yol)};
        a.insert(a.end(), ek.begin(), ek.end());
        return surec_calistir(a);
    };
    DOGRULA(d, calistir("saglam.mcap", p.bayt, {}) == 0, "sağlam dosya: 0");
    DOGRULA(d, calistir("saglam2.mcap", p.bayt, {"--iletiler", "5"}) == 0, "ileti dökümü: 0");
    std::vector<std::uint8_t> kesik(p.bayt.begin(), p.bayt.end() - 50);
    DOGRULA(d, calistir("kesik.mcap", kesik, {}) == 3, "kapanmamış dosya: 3");
    std::vector<std::uint8_t> bozuk = p.bayt;
    bozuk[static_cast<std::size_t>(p.parcalar[1].govde + 24)] ^= 0xFFu;
    DOGRULA(d, calistir("bozuk.mcap", bozuk, {}) == 2, "bozuk dosya: 2");
    DOGRULA(d, calistir("cop.mcap", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, {}) == 1, "MCAP olmayan dosya: 1");
    DOGRULA(d, surec_calistir({dok_programi}) == 64, "bağımsız değişkensiz: 64");
    DOGRULA(d, surec_calistir({dok_programi, yol_metni(dizin.yol() / "yok.mcap")}) == 1, "olmayan dosya: 1");
    return d.hata;
}

}  // namespace ida::kayit::test
