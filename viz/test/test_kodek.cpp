// A2 kodlaması: a2_wire.h yapılarından kurulan bayt dizileriyle gidiş-dönüş, bilinen bayt
// dökümü ve bozuk/kısa ileti reddi (W3-viz kabul md. 2; A2-K06, K07; A2-U01, U02'nin
// görselleştirici yarısı).
#include <cmath>
#include <cstring>
#include <limits>

#include "deneme.hpp"
#include "ida/viz/kodek.hpp"
#include "ida/viz/oturum.hpp"

using namespace ida::viz;

namespace {

// Bir bayt dizisini sim→görselleştirici okuyucusundan geçirir.
KodHata cercevele(const Baytlar& b, a2::Baslik& baslik, Baytlar& yuk, bool& cikti, Yon alici = Yon::sim_gorsel) {
  CerceveOkuyucu o(alici);
  if (!o.ekle(b.data(), b.size())) return KodHata::yuk_buyuk;
  KodHata h = KodHata::yok;
  cikti = o.sonraki(baslik, yuk, h);
  return h;
}

HosgeldinIleti ornek_hosgeldin() {
  HosgeldinIleti h;
  h.h.oturum_kimligi = 0x1122334455667788ull;
  h.h.surum_ana = 1;
  h.h.surum_alt = 0;
  h.h.sim_kipi = 1;
  h.h.kademe_3b = 2;
  h.h.kademe_kamera = 0;
  h.h.kamera_sayisi = 0;
  h.h.adim_no = 800;
  h.h.adim_ns = 1'250'000;
  h.h.t_ns = 800 * 1'250'000LL;
  h.h.durum_hiz_pay = 60;
  h.h.durum_hiz_payda = 1;
  metin_yaz(h.h.manifest_yolu, sizeof h.h.manifest_yolu, "/tmp/ida/assets/acik_su/manifest.json");
  h.h.calisma_nesli = 1;
  a2::GovdeBilgisi g{};
  g.govde_kimligi = 7;
  g.varlik_indeksi = 0;
  g.govde_turu = 1;
  g.dolu_tupler = 3;
  h.govdeler.push_back(g);
  g.govde_kimligi = 8;
  g.varlik_indeksi = 1;
  g.govde_turu = 3;
  g.dolu_tupler = 0;
  h.govdeler.push_back(g);
  return h;
}

a2::GovdeDurumu ornek_govde(std::uint32_t kimlik, std::uint8_t tur) {
  a2::GovdeDurumu g{};
  g.govde_kimligi = kimlik;
  g.govde_turu = tur;
  g.bayraklar = a2::kGovdeGecerli;
  g.p_n[0] = 1.5;
  g.p_n[1] = -2.0;
  g.p_n[2] = 3.0;
  g.q_nb[0] = std::cos(0.25);
  g.q_nb[3] = std::sin(0.25);
  g.v_n[0] = 0.3;
  return g;
}

void gidis_donus() {
  // merhaba (görselleştirici → sim)
  a2::Merhaba m{};
  m.surum_ana = 1;
  m.platform = 1;
  m.cizici = 1;
  m.surec_kimligi = 4242;
  metin_yaz(m.godot_surumu, sizeof m.godot_surumu, "4.7.2.stable");
  metin_yaz(m.uzanti_surumu, sizeof m.uzanti_surumu, "w3-viz");
  metin_yaz(m.gpu_adi, sizeof m.gpu_adi, "Apple M3 Pro");
  Baytlar b;
  kodla_merhaba(b, 0, m);
  DENETLE(b.size() == 16 + 144);
  // Bilinen başlık dökümü: "IDA2", tür 0x0001, bayrak 0, yük 144, sıra 0 (küçük sonlu).
  const Bayt beklenen[16] = {0x49, 0x44, 0x41, 0x32, 0x01, 0x00, 0x00, 0x00,
                             0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  DENETLE(std::memcmp(b.data(), beklenen, 16) == 0);
  a2::Baslik bs{};
  Baytlar yuk;
  bool cikti = false;
  DENETLE(cercevele(b, bs, yuk, cikti, Yon::gorsel_sim) == KodHata::yok && cikti);
  a2::Merhaba m2{};
  DENETLE(coz_merhaba(yuk, m2) == KodHata::yok);
  DENETLE(std::memcmp(&m, &m2, sizeof m) == 0);

  // hosgeldin (değişken uzunluk)
  const HosgeldinIleti h = ornek_hosgeldin();
  b.clear();
  kodla_hosgeldin(b, 0, h);
  DENETLE(b.size() == 16 + 600 + 2 * 16);
  DENETLE(cercevele(b, bs, yuk, cikti) == KodHata::yok && cikti);
  HosgeldinIleti h2;
  DENETLE(coz_hosgeldin(yuk, h2) == KodHata::yok);
  DENETLE(h2.govdeler.size() == 2 && h2.govdeler[1].govde_kimligi == 8);
  DENETLE(metin_oku(h2.h.manifest_yolu, sizeof h2.h.manifest_yolu) == "/tmp/ida/assets/acik_su/manifest.json");
  Baytlar b2;
  kodla_hosgeldin(b2, 0, h2);
  DENETLE(b == b2);

  // durum
  DurumIleti d;
  d.d.t_ns = 801 * 1'250'000LL;
  d.d.adim_no = 801;
  d.d.gonderim_duvar_ns = 123456789;
  d.d.dalga_kumesi_surumu = 1;
  d.d.calisma_durumu = 1;
  d.govdeler.push_back(ornek_govde(7, 1));
  d.govdeler.push_back(ornek_govde(8, 3));
  b.clear();
  kodla_durum(b, 5, d);
  DENETLE(b.size() == 16 + 32 + 2 * 120);
  DENETLE(cercevele(b, bs, yuk, cikti) == KodHata::yok && cikti);
  DENETLE(bs.sira == 5 && bs.tur == 0x0204);
  DurumIleti d2;
  DENETLE(coz_durum(yuk, 1'250'000, d2) == KodHata::yok);
  DENETLE(d2.govdeler.size() == 2);
  DENETLE(std::memcmp(&d.govdeler[0], &d2.govdeler[0], sizeof(a2::GovdeDurumu)) == 0);
  // p_n ofseti 16 (a2_wire.h): yük içinde 32 + 16 baytta 1.5 double'ı durur.
  double p0 = 0.0;
  std::memcpy(&p0, yuk.data() + 32 + 16, 8);
  DENETLE(p0 == 1.5);

  // dalga kümesi: özet bileşen dizisinin SHA-256'sı
  DalgaIleti dk;
  dk.d.surum = 1;
  dk.d.derin_su = 1;
  dk.d.g = 9.80665;
  dk.d.rho = 1025.0;
  dk.d.derinlik_m = 0.0;  // derin_su = 1 iken yok sayılır
  a2::DalgaBileseni db{};
  db.a = 0.1;
  db.k = 0.2;
  db.omega = std::sqrt(9.80665 * 0.2);
  db.beta = 0.5;
  db.phi = 1.0;
  dk.bilesenler.push_back(db);
  dalga_ozeti_hesapla(dk);
  b.clear();
  kodla_dalga(b, 1, dk);
  DENETLE(cercevele(b, bs, yuk, cikti) == KodHata::yok && cikti);
  DalgaIleti dk2;
  DENETLE(coz_dalga(yuk, dk2) == KodHata::yok);
  // özet bozulunca H17
  yuk[40] ^= 0x01;
  DENETLE(coz_dalga(yuk, dk2) == KodHata::ozet);
}

void bozuk_ve_kisa() {
  a2::Baslik bs{};
  Baytlar yuk;
  bool cikti = true;

  // Kısa: başlığın yalnız 10 baytı → hata yok, çıktı yok (veri bekleniyor).
  Baytlar b;
  kodla_veda(b, 0, a2::VedaNedeni::olagan);
  Baytlar kisa(b.begin(), b.begin() + 10);
  DENETLE(cercevele(kisa, bs, yuk, cikti) == KodHata::yok && !cikti);
  // Kısa: başlık tam, yük eksik → yine bekler.
  Baytlar kisa2(b.begin(), b.end() - 1);
  DENETLE(cercevele(kisa2, bs, yuk, cikti) == KodHata::yok && !cikti);

  // Sihirli sayı yanlış
  Baytlar s = b;
  s[0] = 0x00;
  DENETLE(cercevele(s, bs, yuk, cikti) == KodHata::sihirli && !cikti);
  // Bilinmeyen tür
  s = b;
  s[4] = 0x00;
  s[5] = 0x03;
  DENETLE(cercevele(s, bs, yuk, cikti) == KodHata::tur_bilinmiyor);
  // Bayraklar ≠ 0
  s = b;
  s[6] = 0x01;
  DENETLE(cercevele(s, bs, yuk, cikti) == KodHata::bayraklar);
  // Yük > 1 MiB
  s = b;
  const std::uint32_t buyuk = (1u << 20) + 1u;
  std::memcpy(s.data() + 8, &buyuk, 4);
  DENETLE(cercevele(s, bs, yuk, cikti) == KodHata::yuk_buyuk);
  // Sabit boyutlu iletide yanlış uzunluk (veda 8 yerine 9)
  s = b;
  const std::uint32_t dokuz = 9;
  std::memcpy(s.data() + 8, &dokuz, 4);
  s.push_back(0);
  DENETLE(cercevele(s, bs, yuk, cikti) == KodHata::uzunluk);
  // Yön yanlış: merhaba görselleştiriciye gelmez
  a2::Merhaba m{};
  m.surum_ana = 1;
  m.platform = 1;
  m.cizici = 1;
  Baytlar mb;
  kodla_merhaba(mb, 0, m);
  DENETLE(cercevele(mb, bs, yuk, cikti, Yon::sim_gorsel) == KodHata::yon_yanlis);

  // hosgeldin: manifest yolunda NUL yok → metin
  HosgeldinIleti h = ornek_hosgeldin();
  std::memset(h.h.manifest_yolu, 'a', sizeof h.h.manifest_yolu);
  b.clear();
  kodla_hosgeldin(b, 0, h);
  DENETLE(cercevele(b, bs, yuk, cikti) == KodHata::yok && cikti);
  HosgeldinIleti h2;
  DENETLE(coz_hosgeldin(yuk, h2) == KodHata::metin);
  // hosgeldin: 65 gövde → sınır
  h = ornek_hosgeldin();
  h.govdeler.clear();
  for (std::uint32_t i = 0; i < 65; ++i) {
    a2::GovdeBilgisi g{};
    g.govde_kimligi = i + 1;
    g.govde_turu = 3;
    h.govdeler.push_back(g);
  }
  b.clear();
  kodla_hosgeldin(b, 0, h);
  DENETLE(cercevele(b, bs, yuk, cikti) == KodHata::yok && cikti);
  DENETLE(coz_hosgeldin(yuk, h2) == KodHata::sinir);
  // hosgeldin: gövde sayısı yük boyutuyla tutmuyor → uzunluk
  h = ornek_hosgeldin();
  b.clear();
  kodla_hosgeldin(b, 0, h);
  DENETLE(cercevele(b, bs, yuk, cikti) == KodHata::yok && cikti);
  yuk.resize(yuk.size() - 16);
  DENETLE(coz_hosgeldin(yuk, h2) == KodHata::uzunluk);
  // hosgeldin: t_ns ≠ adim_no · adim_ns → alan
  h = ornek_hosgeldin();
  h.h.t_ns += 1;
  b.clear();
  kodla_hosgeldin(b, 0, h);
  cercevele(b, bs, yuk, cikti);
  DENETLE(coz_hosgeldin(yuk, h2) == KodHata::alan);
  // hosgeldin: kademe_3b = 0 (kapalı) → alan (A2-K02)
  h = ornek_hosgeldin();
  h.h.kademe_3b = 0;
  b.clear();
  kodla_hosgeldin(b, 0, h);
  cercevele(b, bs, yuk, cikti);
  DENETLE(coz_hosgeldin(yuk, h2) == KodHata::alan);

  // durum: bildirilen gövde sayısından az gövde → uzunluk
  DurumIleti d;
  d.d.t_ns = 1'250'000;
  d.d.adim_no = 1;
  d.d.calisma_durumu = 1;
  d.govdeler.push_back(ornek_govde(7, 1));
  d.govdeler.push_back(ornek_govde(8, 3));
  b.clear();
  kodla_durum(b, 0, d);
  cercevele(b, bs, yuk, cikti);
  Baytlar eksik(yuk.begin(), yuk.end() - 120);
  DurumIleti d2;
  DENETLE(coz_durum(eksik, 1'250'000, d2) == KodHata::uzunluk);
  // durum: zaman adımla tutarsız → alan
  d.d.t_ns = 1'250'001;
  b.clear();
  kodla_durum(b, 0, d);
  cercevele(b, bs, yuk, cikti);
  DENETLE(coz_durum(yuk, 1'250'000, d2) == KodHata::alan);
  // durum: çalışma durumu 0 → alan
  d.d.t_ns = 1'250'000;
  d.d.calisma_durumu = 0;
  b.clear();
  kodla_durum(b, 0, d);
  cercevele(b, bs, yuk, cikti);
  DENETLE(coz_durum(yuk, 1'250'000, d2) == KodHata::alan);
  // ret: ayrıntıda NUL yok → metin
  a2::Ret r{};
  r.neden = 5;
  std::memset(r.ayrinti, 'x', sizeof r.ayrinti);
  Baytlar rb;
  rb.resize(sizeof r);
  std::memcpy(rb.data(), &r, sizeof r);
  a2::Ret r2{};
  DENETLE(coz_ret(rb, r2) == KodHata::metin);
}

void govde_gecerliligi() {
  a2::GovdeDurumu g = ornek_govde(1, 1);
  DENETLE(govde_durumu_gecerli(g));
  a2::GovdeDurumu n = g;
  n.p_n[1] = std::numeric_limits<double>::quiet_NaN();
  DENETLE(!govde_durumu_gecerli(n));
  n = g;
  n.q_nb[0] *= 1.0 + 3e-6;  // |‖q‖ − 1| > 1e-6
  DENETLE(!govde_durumu_gecerli(n));
  n = g;
  n.bayraklar = 0;
  DENETLE(!govde_durumu_gecerli(n));
}

void sira_aritmetigi() {
  DENETLE(sira_yeni(1, 0));
  DENETLE(!sira_yeni(0, 0));
  DENETLE(!sira_yeni(0, 1));
  DENETLE(sira_yeni(0, 0xFFFFFFFFu));  // sarma
  DENETLE(!sira_yeni(0x80000000u, 0));  // fark 2³¹: eski sayılır
}

// İstemci oturumu: kısa parçalı ve bozuk ileti iletimi.
void oturum_reddi() {
  IstemciOturumu::Ayarlar a;
  IstemciOturumu o(a);
  o.baglandi(0, true);
  Baytlar merhaba = o.giden_al();
  DENETLE(merhaba.size() == 160);
  // hosgeldin'i bayt bayt ver: tek ileti, hata yok.
  Baytlar b;
  kodla_hosgeldin(b, 0, ornek_hosgeldin());
  for (Bayt x : b) o.veri_geldi(&x, 1, 1);
  DENETLE(o.asama() == Asama::kurulum);
  // Sırada dalga_kumesi beklenirken durum gelirse protokol hatası ve veda.
  DurumIleti d;
  d.d.t_ns = 801 * 1'250'000LL;
  d.d.adim_no = 801;
  d.d.calisma_durumu = 1;
  b.clear();
  kodla_durum(b, 1, d);
  o.veri_geldi(b.data(), b.size(), 2);
  DENETLE(o.asama() == Asama::kapali);
  Baytlar giden = o.giden_al();
  a2::Baslik bs{};
  Baytlar yuk;
  bool cikti = false;
  DENETLE(cercevele(giden, bs, yuk, cikti, Yon::gorsel_sim) == KodHata::yok && cikti);
  a2::Veda v{};
  DENETLE(bs.tur == 0x00FF && coz_veda(yuk, v) == KodHata::yok &&
          v.neden == static_cast<std::uint32_t>(a2::VedaNedeni::protokol_hatasi));
  DENETLE(o.yeniden_baglanabilir());

  // Azalan sıra protokol hatasıdır (A2-K07).
  IstemciOturumu o2(a);
  o2.baglandi(0, true);
  b.clear();
  kodla_hosgeldin(b, 0, ornek_hosgeldin());
  o2.veri_geldi(b.data(), b.size(), 1);
  b.clear();
  DalgaIleti dk;
  dk.d.surum = 1;
  dk.d.derin_su = 1;
  dk.d.g = 9.8;
  dk.d.rho = 1000.0;
  dalga_ozeti_hesapla(dk);
  kodla_dalga(b, 0, dk);  // sıra 0 yeniden
  o2.veri_geldi(b.data(), b.size(), 2);
  DENETLE(o2.asama() == Asama::kapali);

  // ret(surum) sonrası yeniden bağlanılmaz (A2-K16).
  IstemciOturumu o3(a);
  o3.baglandi(0, true);
  b.clear();
  kodla_ret(b, 0, a2::RetNedeni::surum, "ana sürüm 1 değil");
  o3.veri_geldi(b.data(), b.size(), 1);
  DENETLE(o3.asama() == Asama::kapali && !o3.yeniden_baglanabilir());
}

// sol #2: kamera hızı denetiminde u32 çarpım taşması.
a2::KameraYapilandirmasi gecerli_kamera() {
  a2::KameraYapilandirmasi k{};
  k.kamera_kimligi = 0;
  k.piksel_bicimi = 1;
  k.genislik = 640;
  k.yukseklik = 480;
  k.hiz_pay = 30;
  k.hiz_payda = 1;
  k.yapilandirma_surumu = 1;
  k.kalibrasyon_surumu = 1;
  k.fx = 381.36;
  k.fy = 381.36;
  k.cx = 319.5;
  k.cy = 239.5;
  k.montaj_govde_kimligi = 7;
  k.montaj_q_bs[0] = 1.0;
  k.pozlama_ns = 8'000'000;
  k.yakin_m = 0.05;
  k.uzak_m = 40.0;
  return k;
}

KodHata kamerali_hosgeldin(std::uint8_t kademe, std::uint32_t pay, std::uint32_t payda) {
  HosgeldinIleti h = ornek_hosgeldin();
  h.h.kademe_kamera = kademe;
  a2::KameraYapilandirmasi k = gecerli_kamera();
  if (kademe == 1) {
    k.genislik = 320;
    k.yukseklik = 240;
    k.cx = 159.5;
    k.cy = 119.5;
  }
  k.hiz_pay = pay;
  k.hiz_payda = payda;
  h.kameralar.push_back(k);
  a2::HalkaBilgisi r{};
  r.yuva_sayisi = 8;
  metin_yaz(r.ad, sizeof r.ad, "/ida2-00000001-c0-g00000001");
  h.halkalar.push_back(r);
  Baytlar b;
  kodla_hosgeldin(b, 0, h);
  a2::Baslik bs{};
  Baytlar yuk;
  bool cikti = false;
  cercevele(b, bs, yuk, cikti);
  HosgeldinIleti c;
  return coz_hosgeldin(yuk, c);
}

// sol #1 (çerçeve okuyucu düzeyi): bekleyen tamamlanmamış veri sınırlı.
void bekleyen_siniri() {
  CerceveOkuyucu o(Yon::sim_gorsel);
  Baytlar parca(kEklemeParcasi, 0);
  // Geçerli bir başlık: 1 MiB'lik dalga_kumesi değil, en büyük izinli değişken ileti boyu
  a2::Baslik b{};
  b.sihirli = a2::kSihirliSayi;
  b.tur = 0x0204;
  b.yuk_bayt = a2::kAzamiYuk;
  Baytlar bas(16);
  std::memcpy(bas.data(), &b, 16);
  DENETLE(o.ekle(bas.data(), bas.size()));
  std::size_t eklenen = 16;
  bool ret = false;
  while (eklenen < kAzamiBekleyenBayt + kEklemeParcasi) {
    if (!o.ekle(parca.data(), parca.size())) {
      ret = true;
      break;
    }
    eklenen += parca.size();
  }
  DENETLE(ret);  // tüketilmeden biriken veri sınırı aşamaz
  DENETLE(o.bekleyen() <= kAzamiBekleyenBayt);
  Baytlar dev(kAzamiBekleyenBayt + 1, 0);
  CerceveOkuyucu o2(Yon::sim_gorsel);
  DENETLE(!o2.ekle(dev.data(), dev.size()));
}

void kamera_hizi_tasmasi() {
  DENETLE(kamerali_hosgeldin(2, 30, 1) == KodHata::yok);          // denetim: geçerli tam
  DENETLE(kamerali_hosgeldin(1, 15, 1) == KodHata::yok);          // denetim: geçerli sade
  DENETLE(kamerali_hosgeldin(2, 0, 2147483648u) == KodHata::alan);  // 30 × 2³¹ ≡ 0 (mod 2³²): 0 Hz
  DENETLE(kamerali_hosgeldin(1, 2147483648u, 2147483648u) == KodHata::alan);  // 15 × 2³¹ ≡ 2³¹: 1 Hz
}

// sol #12: kodlayıcı kullanmadan, sözleşmedeki ofsetlerle elle kurulan tel vektörleri.
struct ElleYuk {
  Baytlar b;
  explicit ElleYuk(std::size_t n) : b(n, 0) {}
  void u8(std::size_t o, std::uint64_t v) { b[o] = static_cast<Bayt>(v & 0xFFu); }
  void le(std::size_t o, std::uint64_t v, int n) {
    for (int i = 0; i < n; ++i) b[o + static_cast<std::size_t>(i)] = static_cast<Bayt>((v >> (8 * i)) & 0xFFu);
  }
  void u16(std::size_t o, std::uint64_t v) { le(o, v, 2); }
  void u32(std::size_t o, std::uint64_t v) { le(o, v, 4); }
  void u64(std::size_t o, std::uint64_t v) { le(o, v, 8); }
  void f64(std::size_t o, double v) { le(o, std::bit_cast<std::uint64_t>(v), 8); }
  void metin(std::size_t o, const char* s) {
    for (std::size_t i = 0; s[i] != '\0'; ++i) b[o + i] = static_cast<Bayt>(s[i]);
  }
};

Baytlar elle_cerceve(std::uint16_t tur, std::uint32_t sira, const Baytlar& yuk) {
  ElleYuk c(16);
  c.b[0] = 0x49;  // "IDA2"
  c.b[1] = 0x44;
  c.b[2] = 0x41;
  c.b[3] = 0x32;
  c.u16(4, tur);
  c.u16(6, 0);
  c.u32(8, yuk.size());
  c.u32(12, sira);
  c.b.insert(c.b.end(), yuk.begin(), yuk.end());
  return c.b;
}

std::uint8_t onaltilik_hane(char c) {
  return static_cast<std::uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10);
}

void bagimsiz_vektorler() {
  a2::Baslik bs{};
  Baytlar yuk;
  bool cikti = false;

  // hosgeldin (§3.3; ofsetler a2_wire.h static_assert'leri ve tablo sırası), 1 gövde
  ElleYuk h(600 + 16);
  h.u64(0, 0x0102030405060708ull);  // oturum_kimligi
  h.u16(8, 1);                       // surum_ana
  h.u16(10, 0);                      // surum_alt
  h.u8(12, 2);                       // sim_kipi
  h.u8(13, 1);                       // kademe_3b
  h.u8(14, 0);                       // kademe_kamera
  h.u8(15, 0);                       // kamera_sayisi
  h.u64(16, 800ull * 1'250'000ull);  // t_ns
  h.u64(24, 800);                    // adim_no
  h.u64(32, 1'250'000);              // adim_ns
  h.u32(40, 60);                     // durum_hiz_pay
  h.u32(44, 1);                      // durum_hiz_payda
  for (std::size_t i = 0; i < 32; ++i) h.u8(48 + i, i + 1);  // manifest_sha256
  h.metin(80, "/a/b/manifest.json");                          // manifest_yolu (512 B)
  h.u16(592, 1);                                               // govde_sayisi
  h.u32(596, 3);                                               // calisma_nesli
  h.u32(600, 42);                                              // GovdeBilgisi: kimlik
  h.u32(604, 5);                                               // sureklilik_no
  h.u16(608, 2);                                               // varlik_indeksi
  h.u8(610, 1);                                                // govde_turu araç
  h.u8(611, 3);                                                // dolu_tupler
  Baytlar cer = elle_cerceve(0x0002, 0, h.b);
  DENETLE(cercevele(cer, bs, yuk, cikti) == KodHata::yok && cikti);
  HosgeldinIleti hc;
  DENETLE(coz_hosgeldin(yuk, hc) == KodHata::yok);
  DENETLE(hc.h.oturum_kimligi == 0x0102030405060708ull && hc.h.sim_kipi == 2 && hc.h.kademe_3b == 1);
  DENETLE(hc.h.t_ns == 1'000'000'000 && hc.h.adim_no == 800 && hc.h.calisma_nesli == 3);
  DENETLE(hc.h.manifest_sha256[0] == 1 && hc.h.manifest_sha256[31] == 32);
  DENETLE(metin_oku(hc.h.manifest_yolu, sizeof hc.h.manifest_yolu) == "/a/b/manifest.json");
  DENETLE(hc.govdeler.size() == 1 && hc.govdeler[0].govde_kimligi == 42 && hc.govdeler[0].sureklilik_no == 5 &&
          hc.govdeler[0].varlik_indeksi == 2 && hc.govdeler[0].govde_turu == 1 && hc.govdeler[0].dolu_tupler == 3);
  Baytlar yeniden;
  kodla_hosgeldin(yeniden, 0, hc);
  DENETLE(yeniden == cer);

  // durum (§3.4): başlık 32 B + GovdeDurumu 120 B (p_n 16, q_nb 40, v_n 72, w_b 96)
  ElleYuk d(32 + 120);
  d.u64(0, 801ull * 1'250'000ull);
  d.u64(8, 801);
  d.u64(16, 987654321);
  d.u32(24, 1);  // dalga_kumesi_surumu
  d.u16(28, 1);  // govde_sayisi
  d.u8(30, 1);   // calisma_durumu
  d.u8(31, 1);   // bayraklar: takvim dışı
  d.u32(32 + 0, 42);
  d.u32(32 + 4, 6);
  d.u8(32 + 8, 1);
  d.u8(32 + 9, 2);
  d.u16(32 + 10, 1);  // geçerli
  d.f64(32 + 16, 1.5);
  d.f64(32 + 24, -2.25);
  d.f64(32 + 32, 3.0);
  d.f64(32 + 40, 0.6);  // q w
  d.f64(32 + 48, 0.0);
  d.f64(32 + 56, 0.0);
  d.f64(32 + 64, 0.8);  // q z
  d.f64(32 + 72, 0.5);
  d.f64(32 + 96, 0.125);
  cer = elle_cerceve(0x0204, 9, d.b);
  DENETLE(cercevele(cer, bs, yuk, cikti) == KodHata::yok && cikti && bs.sira == 9);
  DurumIleti dc;
  DENETLE(coz_durum(yuk, 1'250'000, dc) == KodHata::yok);
  DENETLE(dc.d.gonderim_duvar_ns == 987654321 && dc.d.bayraklar == 1 && dc.govdeler.size() == 1);
  const a2::GovdeDurumu& g = dc.govdeler[0];
  DENETLE(g.govde_kimligi == 42 && g.sureklilik_no == 6 && g.dolu_tupler == 2 && g.bayraklar == 1);
  DENETLE(g.p_n[0] == 1.5 && g.p_n[1] == -2.25 && g.p_n[2] == 3.0 && g.q_nb[0] == 0.6 && g.q_nb[3] == 0.8);
  DENETLE(g.v_n[0] == 0.5 && g.w_b[0] == 0.125 && govde_durumu_gecerli(g));
  yeniden.clear();
  kodla_durum(yeniden, 9, dc);
  DENETLE(yeniden == cer);

  // dalga_kumesi (§3.4): sabit kısım 72 B, bileşen 40 B; beklenen özet bağımsız hesaplandı
  // (Python hashlib, struct.pack('<5d', 0.25, 1.5, 0.25, 0.5, -1.0)).
  const char* beklenen_hex = "fb1f85d1cdb170610a6667e0c5044604b3318f6417b2f48c9ad85ba5d00891f0";
  ElleYuk w(72 + 40);
  w.u32(0, 7);
  w.u16(4, 1);
  w.u8(6, 1);
  w.u64(8, 1234);
  w.f64(16, 9.80665);
  w.f64(24, 1025.0);
  w.f64(32, 50.0);
  for (std::size_t i = 0; i < 32; ++i)
    w.u8(40 + i, static_cast<std::uint64_t>(onaltilik_hane(beklenen_hex[2 * i]) * 16 + onaltilik_hane(beklenen_hex[2 * i + 1])));
  w.f64(72, 0.25);
  w.f64(80, 1.5);
  w.f64(88, 0.25);
  w.f64(96, 0.5);
  w.f64(104, -1.0);
  cer = elle_cerceve(0x0201, 1, w.b);
  DENETLE(cercevele(cer, bs, yuk, cikti) == KodHata::yok && cikti);
  DalgaIleti wc;
  DENETLE(coz_dalga(yuk, wc) == KodHata::yok);
  DENETLE(wc.d.surum == 7 && wc.d.tohum == 1234 && wc.bilesenler.size() == 1 && wc.bilesenler[0].phi == -1.0);
  yuk[40 + 31] ^= 0x01u;
  DENETLE(coz_dalga(yuk, wc) == KodHata::ozet);

  // sahne_olayi govde_dogdu (§3.4; SahneOlayi 120 B)
  ElleYuk o(120);
  o.u64(0, 1'250'000);
  o.u64(8, 1);
  o.u16(16, 1);   // tur: govde_dogdu
  o.u32(20, 77);  // govde_kimligi
  o.u32(28, 0);   // sureklilik_no
  o.u16(32, 1);   // varlik_indeksi
  o.u8(34, 2);    // govde_turu torpido
  o.u8(37, 2);    // kaynak_tup
  o.u32(112, 1);  // calisma_nesli
  cer = elle_cerceve(0x0205, 2, o.b);
  DENETLE(cercevele(cer, bs, yuk, cikti) == KodHata::yok && cikti);
  a2::SahneOlayi oc{};
  DENETLE(coz_sahne_olayi(yuk, 1'250'000, oc) == KodHata::yok);
  DENETLE(oc.tur == 1 && oc.govde_kimligi == 77 && oc.varlik_indeksi == 1 && oc.govde_turu == 2 &&
          oc.kaynak_tup == 2 && oc.calisma_nesli == 1);

  // hazir (görselleştirici → sim, 48 B): kodlayıcının ürettiği elle kurulanla aynı
  ElleYuk z(48);
  for (std::size_t i = 0; i < 32; ++i) z.u8(i, 0xA0u + i);
  z.u32(32, 7);
  z.u32(36, 3);
  z.u32(40, 0);
  z.u32(44, 4);
  a2::Hazir hz{};
  for (std::size_t i = 0; i < 32; ++i) hz.manifest_sha256[i] = static_cast<std::uint8_t>(0xA0u + i);
  hz.dalga_kumesi_surumu = 7;
  hz.ortam_surumu = 3;
  hz.durum = 4;
  yeniden.clear();
  kodla_hazir(yeniden, 5, hz);
  DENETLE(yeniden == elle_cerceve(0x0003, 5, z.b));

  // veda (8 B)
  ElleYuk v(8);
  v.u32(0, 1);
  yeniden.clear();
  kodla_veda(yeniden, 3, a2::VedaNedeni::protokol_hatasi);
  DENETLE(yeniden == elle_cerceve(0x00FF, 3, v.b));
}

}  // namespace

int main() {
  kamera_hizi_tasmasi();
  bekleyen_siniri();
  bagimsiz_vektorler();
  gidis_donus();
  bozuk_ve_kisa();
  govde_gecerliligi();
  sira_aritmetigi();
  oturum_reddi();
  DENEME_SONUC("viz.kodek");
}
