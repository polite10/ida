// Sahte kaynağın ürettiği her ileti istemciden geçer (W3-viz kabul md. 2): sahte sim ve
// istemci oturumu bellekte, rastgele parçalara bölünmüş baytlarla, iki sıfırlamayı kapsayan
// 260 s'lik sim zamanı boyunca konuşur. Hiçbir tarafta protokol hatası olmaz; istemcinin
// ara değerlediği poz sahte senaryonun kinematik pozuna yakındır.
#include <cmath>
#include <random>

#include "deneme.hpp"
#include "ida/viz/oturum.hpp"
#include "sahte_sim.hpp"

using namespace ida::viz;
using namespace ida::sahte;

namespace {

constexpr std::int64_t kMs = 1'000'000;

struct Hat {
  std::mt19937 rastgele{12345};
  // Baytları 1–700 baytlık rastgele parçalarla aktarır (çerçevelemeyi zorlar).
  template <class F>
  void aktar(const Baytlar& b, F&& al) {
    std::size_t i = 0;
    while (i < b.size()) {
      const std::size_t n = std::min<std::size_t>(b.size() - i, 1 + rastgele() % 700);
      al(b.data() + i, n);
      i += n;
    }
  }
};

void tam_akis() {
  SenaryoAyarlari a;
  a.kademe_3b = 2;
  a.manifest_yolu = "/yok/acik_su/manifest.json";
  a.manifest_ozeti = sha256("ornek manifest", 14);
  a.sifirlama_ns = 121'000'000'000;
  SahteSim sim(a);
  IstemciOturumu istemci(IstemciOturumu::Ayarlar{});
  Hat hat;

  std::int64_t duvar = 1'000'000'000;
  sim.baglanti_acildi(duvar, 0xABCDEF0102030405ull);
  istemci.baglandi(duvar, true);

  int sahne_kur = 0, sifirlandi = 0, kapandi = 0;
  double en_buyuk_hata = 0.0;
  std::int64_t en_kotu_t = 0;
  std::uint64_t poz_sayisi = 0;
  for (std::int64_t adim = 0; adim < 260'000 / 4; ++adim) {  // 4 ms'lik döngü, 260 s
    duvar += 4 * kMs;
    sim.sim_ilerle(duvar - 1'000'000'000);
    sim.zaman(duvar);
    hat.aktar(sim.giden_al(), [&](const Bayt* p, std::size_t n) { istemci.veri_geldi(p, n, duvar); });
    istemci.zaman(duvar, 60'000, true);
    for (const Olay& o : istemci.olaylari_al()) {
      if (o.tur == OlayTuru::sahne_kur) {
        ++sahne_kur;
        DENETLE(o.metin == "/yok/acik_su/manifest.json");
        istemci.sahne_sonucu(a2::HazirDurumu::basarili, a.manifest_ozeti);
      } else if (o.tur == OlayTuru::sifirlandi) {
        ++sifirlandi;
      } else if (o.tur == OlayTuru::kapandi) {
        ++kapandi;
        std::fprintf(stderr, "kapandı: %s\n", o.metin.c_str());
      }
    }
    hat.aktar(istemci.giden_al(), [&](const Bayt* p, std::size_t n) { sim.veri_geldi(p, n, duvar); });

    // Operatör saati ve ara değer: araç pozu kinematik senaryoya yakın.
    std::int64_t t_op = 0;
    if (istemci.saat().ilerle(duvar, t_op)) {
      for (const auto& [kimlik, g] : istemci.sahne().govdeler()) {
        // İlk örnekten önceki t_op'ta ilk örnek çizilir (ekstrapolasyon yok); karşılaştırılmaz.
        if (g.tur != 1 || !Sahne::gorunur(g, t_op) || t_op < g.gecmis.front().t_ns) continue;
        Poz p;
        if (!Sahne::poz(g, t_op, p)) continue;
        const KinematikPoz k = sim.senaryo().arac_pozu(t_op);
        const double hata = uzunluk(cikar(p.p, k.p));
        if (hata > en_buyuk_hata) {
          en_buyuk_hata = hata;
          en_kotu_t = t_op;
        }
        ++poz_sayisi;
      }
      istemci.sahne().buda(t_op, [](std::uint32_t) {});
    }
  }
  DENETLE(kapandi == 0);
  DENETLE(istemci.asama() == Asama::akis);
  DENETLE(sim.asama() == SahteSim::Asama::akis);
  DENETLE(sahne_kur == 1);
  DENETLE(sifirlandi == 2);
  DENETLE(sim.sayaclar().sifirlama == 2);
  DENETLE(sim.sayaclar().protokol_hatasi == 0);
  DENETLE(sim.sayaclar().hazir == 1 && sim.sayaclar().son_hazir_ozet_tuttu && sim.sayaclar().son_hazir_durumu == 0);
  DENETLE(sim.sayaclar().gor_nabzi >= 250);
  DENETLE(istemci.sayaclar().sira_boslugu == 0);
  DENETLE(istemci.sayaclar().gecersiz_govde == 0);
  DENETLE(istemci.calisma_nesli() == 3);
  // 60 Hz durum: 260 s'de yaklaşık 15 600 ileti.
  DENETLE(istemci.sayaclar().alinan_ileti > 15'000);
  // Sıfırlamalardan sonra gövdeler yeni kimliklerle: 2 gövde yaşıyor.
  DENETLE(istemci.sahne().govdeler().size() == 2);
  DENETLE(istemci.sahne().bul(sim.senaryo().arac_kimligi()) != nullptr);
  DENETLE(poz_sayisi > 30'000);
  // Hermite ara değeri (16,7 ms aralık) yumuşak bir yörüngede mm altında kalmalı.
  std::printf("en büyük ara değer hatası: %.3e m, t_op = %.4f s (%llu poz)\n", en_buyuk_hata,
              static_cast<double>(en_kotu_t) * 1e-9, static_cast<unsigned long long>(poz_sayisi));
  DENETLE(en_buyuk_hata < 1e-3);
}

void manifest_reddi() {
  SenaryoAyarlari a;
  a.manifest_yolu = "/yok/manifest.json";
  a.manifest_ozeti = sha256("a", 1);
  SahteSim sim(a);
  IstemciOturumu istemci(IstemciOturumu::Ayarlar{});
  sim.baglanti_acildi(0, 1);
  istemci.baglandi(0, true);
  Baytlar b = istemci.giden_al();
  sim.veri_geldi(b.data(), b.size(), 1);
  b = sim.giden_al();
  istemci.veri_geldi(b.data(), b.size(), 2);
  bool kuruldu = false;
  for (const Olay& o : istemci.olaylari_al()) {
    if (o.tur == OlayTuru::sahne_kur) {
      kuruldu = true;
      istemci.sahne_sonucu(a2::HazirDurumu::basarili, sha256("b", 1));  // farklı özet
    }
  }
  DENETLE(kuruldu);
  b = istemci.giden_al();
  sim.veri_geldi(b.data(), b.size(), 3);
  b = sim.giden_al();
  istemci.veri_geldi(b.data(), b.size(), 4);
  // A2-K14 / K16: ret(manifest) → yeniden bağlanılmaz.
  DENETLE(istemci.asama() == Asama::kapali);
  DENETLE(!istemci.yeniden_baglanabilir());
  DENETLE(sim.kapatilmali());
}

void nabiz_zaman_asimi() {
  SenaryoAyarlari a;
  a.manifest_yolu = "/yok/manifest.json";
  SahteSim sim(a);
  IstemciOturumu istemci(IstemciOturumu::Ayarlar{});
  istemci.baglandi(0, true);
  (void)istemci.giden_al();
  istemci.zaman(2'900 * kMs, 0, true);
  DENETLE(istemci.asama() == Asama::merhaba_gonderildi);
  istemci.zaman(3'100 * kMs, 0, true);  // A2-K62: 3 s sessizlik
  DENETLE(istemci.asama() == Asama::kapali && istemci.yeniden_baglanabilir());
}

// A2-K90–K92: atlama → govde_tablosu → durum bloğu.
struct ElleSim {
  Baytlar b;
  std::uint32_t sira = 0;
  std::uint32_t nesil = 1;
};

a2::GovdeBilgisi bilgi(std::uint32_t kimlik, std::uint16_t varlik, std::uint8_t tur) {
  a2::GovdeBilgisi g{};
  g.govde_kimligi = kimlik;
  g.varlik_indeksi = varlik;
  g.govde_turu = tur;
  return g;
}

a2::GovdeDurumu gdurum(std::uint32_t kimlik, std::uint8_t tur, double n) {
  a2::GovdeDurumu g{};
  g.govde_kimligi = kimlik;
  g.govde_turu = tur;
  g.bayraklar = a2::kGovdeGecerli;
  g.p_n[0] = n;
  g.q_nb[0] = 1.0;
  return g;
}

void kur(IstemciOturumu& o, ElleSim& s) {
  o.baglandi(0, true);
  (void)o.giden_al();
  HosgeldinIleti h;
  h.h.oturum_kimligi = 5;
  h.h.surum_ana = 1;
  h.h.sim_kipi = 4;  // tekrar izleme
  h.h.kademe_3b = 1;
  h.h.adim_ns = 1'250'000;
  h.h.durum_hiz_pay = 60;
  h.h.durum_hiz_payda = 1;
  h.h.calisma_nesli = 1;
  metin_yaz(h.h.manifest_yolu, sizeof h.h.manifest_yolu, "/yok/m.json");
  h.govdeler = {bilgi(7, 0, 1), bilgi(8, 1, 3)};
  kodla_hosgeldin(s.b, s.sira++, h);
  DalgaIleti d;
  d.d.surum = 1;
  d.d.derin_su = 1;
  d.d.g = 9.8;
  d.d.rho = 1025.0;
  dalga_ozeti_hesapla(d);
  kodla_dalga(s.b, s.sira++, d);
  a2::Ortam ort{};
  ort.ortam_surumu = 1;
  ort.ortam_turu = 2;
  ort.statik_varlik_indeksi = 0xFFFF;
  ort.statik_q_nb[0] = 1.0;
  ort.gunes_yonu_n[2] = -1.0;
  kodla_ortam(s.b, s.sira++, ort);
  MalzemeIleti m;
  m.m.surum = 1;
  kodla_malzeme(s.b, s.sira++, m);
  DurumIleti du;
  du.d.t_ns = 0;
  du.d.dalga_kumesi_surumu = 1;
  du.d.calisma_durumu = 1;
  du.govdeler = {gdurum(7, 1, 0.0), gdurum(8, 3, 5.0)};
  kodla_durum(s.b, s.sira++, du);
  o.veri_geldi(s.b.data(), s.b.size(), 1);
  s.b.clear();
  o.sahne_sonucu(a2::HazirDurumu::basarili, Ozet{});
}

void atlama_olayi(ElleSim& s, std::int64_t t) {
  a2::SahneOlayi e{};
  e.t_ns = t;
  e.adim_no = static_cast<std::uint64_t>(t / 1'250'000);
  e.tur = static_cast<std::uint16_t>(a2::SahneOlayiTuru::atlama);
  e.calisma_nesli = ++s.nesil;
  kodla_sahne_olayi(s.b, s.sira++, e);
}

void tablo(ElleSim& s, std::int64_t t, std::vector<a2::GovdeBilgisi> g) {
  GovdeTablosuIleti tb;
  tb.t.t_ns = t;
  tb.t.adim_no = static_cast<std::uint64_t>(t / 1'250'000);
  tb.t.calisma_nesli = s.nesil;
  tb.govdeler = std::move(g);
  kodla_govde_tablosu(s.b, s.sira++, tb);
}

void durum_yaz(ElleSim& s, std::int64_t t, std::vector<a2::GovdeDurumu> g) {
  DurumIleti du;
  du.d.t_ns = t;
  du.d.adim_no = static_cast<std::uint64_t>(t / 1'250'000);
  du.d.dalga_kumesi_surumu = 1;
  du.d.calisma_durumu = 1;
  du.govdeler = std::move(g);
  kodla_durum(s.b, s.sira++, du);
}

void atlama() {
  const std::int64_t T = 400 * 1'250'000LL;
  {  // (a) geçerli blok: hedef 8 tablodan çıkar, yeni torpido 9 tablodan kurulur
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);
    DENETLE(o.asama() == Asama::akis);
    atlama_olayi(s, T);
    tablo(s, T, {bilgi(7, 0, 1), bilgi(9, 2, 2)});
    durum_yaz(s, T, {gdurum(7, 1, 1.0), gdurum(9, 2, 1.2)});
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::akis);
    DENETLE(o.sahne().bul(8) == nullptr && o.sahne().bul(9) != nullptr && o.sahne().bul(7) != nullptr);
    DENETLE(o.calisma_nesli() == 2);
    // (c) geri atlama: ölmüş hedef 8 aynı varlık indeksiyle yeniden görünür
    s.b.clear();
    atlama_olayi(s, 100 * 1'250'000LL);
    tablo(s, 100 * 1'250'000LL, {bilgi(7, 0, 1), bilgi(8, 1, 3)});
    durum_yaz(s, 100 * 1'250'000LL, {gdurum(7, 1, 0.5), gdurum(8, 3, 5.0)});
    o.veri_geldi(s.b.data(), s.b.size(), 3);
    DENETLE(o.asama() == Asama::akis && o.sahne().bul(8) != nullptr && o.sahne().bul(9) == nullptr);
  }
  {  // tablodan önce durum: protokol hatası (A2-K91)
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);
    atlama_olayi(s, T);
    durum_yaz(s, T, {gdurum(7, 1, 1.0)});
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::kapali);
  }
  {  // aynı kimlik başka varlık indeksiyle: protokol hatası (A2-K92)
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);
    atlama_olayi(s, T);
    tablo(s, T, {bilgi(7, 0, 1), bilgi(8, 0, 3)});
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::kapali);
  }
  {  // tablonun zamanı atlamadan farklı: protokol hatası
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);
    atlama_olayi(s, T);
    tablo(s, T + 1'250'000, {bilgi(7, 0, 1)});
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::kapali);
  }
}

// sol #3 (A2-K92): tekrar izlemede doğumdan önceye geri atlanıp yeniden ilerlenince aynı
// kimlik–varlık–tür ile yeniden govde_dogdu gelir; bu yeni kimlik kullanımı değildir.
void yeniden_dogum() {
  const std::int64_t h = 1'250'000;
  const std::int64_t T1 = 400 * h;
  auto dogum = [](ElleSim& s, std::int64_t t, std::uint32_t kimlik, std::uint16_t varlik, std::uint8_t tur) {
    a2::SahneOlayi e{};
    e.t_ns = t;
    e.adim_no = static_cast<std::uint64_t>(t / 1'250'000);
    e.tur = static_cast<std::uint16_t>(a2::SahneOlayiTuru::govde_dogdu);
    e.govde_kimligi = kimlik;
    e.varlik_indeksi = varlik;
    e.govde_turu = tur;
    e.kaynak_tup = tur == 2 ? 1 : 0;
    e.calisma_nesli = s.nesil;
    kodla_sahne_olayi(s.b, s.sira++, e);
  };
  {
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);  // sim_kipi = 4 (tekrar izleme), gövdeler 7 ve 8
    dogum(s, T1, 9, 2, 2);
    durum_yaz(s, T1, {gdurum(7, 1, 1.0), gdurum(8, 3, 5.0), gdurum(9, 2, 1.3)});
    atlama_olayi(s, 100 * h);  // doğumdan önceye
    tablo(s, 100 * h, {bilgi(7, 0, 1), bilgi(8, 1, 3)});
    durum_yaz(s, 100 * h, {gdurum(7, 1, 0.2), gdurum(8, 3, 5.0)});
    dogum(s, T1, 9, 2, 2);  // aynı gövde, yeniden doğuyor
    durum_yaz(s, T1, {gdurum(7, 1, 1.0), gdurum(8, 3, 5.0), gdurum(9, 2, 1.3)});
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::akis);
    DENETLE(o.sahne().bul(9) != nullptr);
  }
  {  // yaşayan gövdenin yinelenen doğumu yine reddedilir
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);
    dogum(s, T1, 7, 0, 1);
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::kapali);
  }
  {  // kimliğin başka varlıkla yeniden görünmesi reddedilir
    IstemciOturumu o(IstemciOturumu::Ayarlar{});
    ElleSim s;
    kur(o, s);
    dogum(s, T1, 9, 2, 2);
    atlama_olayi(s, 100 * h);
    tablo(s, 100 * h, {bilgi(7, 0, 1), bilgi(8, 1, 3)});
    dogum(s, T1, 9, 1, 3);
    o.veri_geldi(s.b.data(), s.b.size(), 2);
    DENETLE(o.asama() == Asama::kapali);
  }
}

// sol #4 (A2-K27): geçerli (0 m) – geçersiz – geçerli (2 m); geçersiz aralıkta son geçerli poz.
void gecersiz_aralik() {
  const std::int64_t h = 1'250'000;
  IstemciOturumu o(IstemciOturumu::Ayarlar{});
  ElleSim s;
  kur(o, s);  // t = 0'da gövde 7 N = 0
  a2::GovdeDurumu bozuk = gdurum(7, 1, 1.0);
  bozuk.bayraklar = 0;  // geçersiz
  durum_yaz(s, 80 * h, {bozuk, gdurum(8, 3, 5.0)});
  durum_yaz(s, 160 * h, {gdurum(7, 1, 2.0), gdurum(8, 3, 5.0)});
  o.veri_geldi(s.b.data(), s.b.size(), 2);
  DENETLE(o.asama() == Asama::akis);
  const GovdeKaydi* g = o.sahne().bul(7);
  DENETLE(g != nullptr);
  if (!g) return;
  Poz p;
  DENETLE(Sahne::poz(*g, 80 * h, p));
  YAKIN(p.p.x, 0.0, 1e-12);  // sol'un örneği: şimdiki 1 m
  DENETLE(Sahne::poz(*g, 120 * h, p));
  YAKIN(p.p.x, 0.0, 1e-12);
  DENETLE(Sahne::poz(*g, 160 * h, p));
  YAKIN(p.p.x, 2.0, 1e-12);
  DENETLE(Sahne::poz(*g, 40 * h, p));  // geçersizden önceki aralıkta da ileriye ara değer yok
  YAKIN(p.p.x, 0.0, 1e-12);
}

}  // namespace

int main() {
  tam_akis();
  manifest_reddi();
  nabiz_zaman_asimi();
  atlama();
  yeniden_dogum();
  gecersiz_aralik();
  DENEME_SONUC("viz.oturum");
}
