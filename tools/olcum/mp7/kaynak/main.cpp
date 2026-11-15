// W0-MP7a: Ping360 sonar modelinin Embree CPU süresi, ışın yakınsaması ve
// aynı platformda tekrar üretilebilirlik ölçümü (Mac ön turu).
//
// Komutlar:
//   mp7 bilgi
//   mp7 sure      --sahne 1e5 --menzil 7 --N 1000,5000 --D 0,1,2 --is 1,2,4,11 [--ping 200] [--isinma 10]
//                 [--kademe tam|sade] [--cikti dosya.csv] [--ham klasör]
//   mp7 yakinsama --sahne 1e5 --menzil 7 --D 2 --N 625,...,80000 [--M 32] [--acilar 8] [--is 11]
//                 [--kademe tam|sade] [--benek 0|1] [--Nref N] [--ref_tohum 5000] [--cikti dosya.csv]
//   mp7 tekrar    --sahne 1e5 --menzil 7 --N 5000 --D 2 --is 1 [--ping 200]
#include <embree4/rtcore.h>
#include <mach/mach.h>
#include <sys/resource.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "havuz.hpp"
#include "rng.hpp"
#include "sahne.hpp"
#include "sonar.hpp"

using namespace mp7;

namespace {

constexpr uint64_t SAHNE_TOHUMU = 20261114;

std::atomic<long long> g_embree_simdi{0}, g_embree_tepe{0};
bool bellek_izle(void*, ssize_t bytes, bool) {
  const long long s = g_embree_simdi.fetch_add(bytes) + bytes;
  long long t = g_embree_tepe.load();
  while (s > t && !g_embree_tepe.compare_exchange_weak(t, s)) {
  }
  return true;
}

double rss_tepe_mb() {
  rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
  return double(ru.ru_maxrss) / (1024.0 * 1024.0);  // macOS: bayt
}
double rss_simdi_mb() {
  mach_task_basic_info_data_t info{};
  mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &n) != KERN_SUCCESS)
    return -1;
  return double(info.resident_size) / (1024.0 * 1024.0);
}

void hata(void*, RTCError code, const char* str) {
  std::fprintf(stderr, "Embree hatası %d: %s\n", int(code), str ? str : "");
  std::exit(2);
}

struct Arg {
  std::map<std::string, std::string> m;
  Arg(int argc, char** argv, int bas) {
    for (int i = bas; i + 1 < argc; i += 2) {
      std::string k = argv[i];
      if (k.rfind("--", 0) != 0) throw std::runtime_error("beklenen --anahtar: " + k);
      m[k.substr(2)] = argv[i + 1];
    }
  }
  std::string s(const std::string& k, const std::string& v) const {
    auto it = m.find(k);
    return it == m.end() ? v : it->second;
  }
  long long i(const std::string& k, long long v) const {
    auto it = m.find(k);
    return it == m.end() ? v : std::stoll(it->second);
  }
  std::vector<int> liste(const std::string& k, const std::string& v) const {
    std::vector<int> r;
    std::stringstream ss(s(k, v));
    std::string t;
    while (std::getline(ss, t, ',')) {
      if (t == "tumu")
        r.push_back(int(std::thread::hardware_concurrency()));
      else
        r.push_back(std::stoi(t));
    }
    return r;
  }
};

RTCDevice cihaz(bool ayrinti) {
  RTCDevice d = rtcNewDevice(ayrinti ? "threads=1,verbose=2" : "threads=1");
  if (!d) throw std::runtime_error("rtcNewDevice başarısız");
  rtcSetDeviceErrorFunction(d, hata, nullptr);
  rtcSetDeviceMemoryMonitorFunction(d, bellek_izle, nullptr);
  return d;
}

SonarAyar menzil_ayari(int menzil) {
  SonarAyar a;
  a.n = 1200;
  if (menzil == 7)
    a.sp = 311;  // R = 6,9975 m (05 §2.4.5)
  else if (menzil == 50)
    a.sp = 2222;  // R = 49,995 m
  else
    throw std::runtime_error("menzil 7|50");
  return a;
}

uint64_t fnv(const void* p, size_t n, uint64_t h) {
  const auto* b = static_cast<const unsigned char*>(p);
  for (size_t i = 0; i < n; ++i) {
    h ^= b[i];
    h *= 1099511628211ull;
  }
  return h;
}

double yuzdelik(std::vector<double> v, double q) {  // en yakın sıra yöntemi
  std::sort(v.begin(), v.end());
  size_t r = size_t(std::ceil(q * double(v.size())));
  if (r < 1) r = 1;
  return v[r - 1];
}

int komut_bilgi() {
  RTCDevice d = cihaz(true);
  std::printf("embree_surum=%zd\n", rtcGetDeviceProperty(d, RTC_DEVICE_PROPERTY_VERSION));
  std::printf("native_ray4=%zd native_ray8=%zd\n", rtcGetDeviceProperty(d, RTC_DEVICE_PROPERTY_NATIVE_RAY4_SUPPORTED),
              rtcGetDeviceProperty(d, RTC_DEVICE_PROPERTY_NATIVE_RAY8_SUPPORTED));
  std::printf("tasking_system=%zd (0 internal, 1 TBB, 2 PPL)\n",
              rtcGetDeviceProperty(d, RTC_DEVICE_PROPERTY_TASKING_SYSTEM));
  std::printf("hardware_concurrency=%u\n", std::thread::hardware_concurrency());
  for (const char* ad : {"1e4", "1e5", "1e6"}) {
    Sahne s;
    s.kur(d, ad, SAHNE_TOHUMU);
    std::printf("sahne %s: ucgen=%zu (taban=%zu) govde=%zu kurulum_ms=%.1f\n", ad, s.toplam_ucgen,
                s.statik[0].ucgen(), s.statik.size() + 1, s.kurulum_ms);
    s.birak();
  }
  rtcReleaseDevice(d);
  return 0;
}

int komut_sure(const Arg& a) {
  qos_yuksek();
  const std::string sahne_ad = a.s("sahne", "1e5");
  const int menzil = int(a.i("menzil", 7));
  const auto Nl = a.liste("N", "1000,2000,5000,10000,20000,40000");
  const auto Dl = a.liste("D", "0,1,2");
  const auto Il = a.liste("is", "1,2,4,tumu");
  const int ping = int(a.i("ping", 200));
  const int isinma = int(a.i("isinma", 10));
  const bool tam = a.s("kademe", "tam") == "tam";
  const std::string cikti = a.s("cikti", "");
  const std::string ham = a.s("ham", "");

  const double rss0 = rss_simdi_mb();
  RTCDevice d = cihaz(false);
  Sahne s;
  s.kur(d, sahne_ad, SAHNE_TOHUMU);
  const double rss1 = rss_simdi_mb();
  const double embree_sahne_mb = double(g_embree_simdi.load()) / (1024.0 * 1024.0);

  FILE* f = nullptr;
  if (!cikti.empty()) {
    std::ifstream var(cikti);
    const bool bos = !var.good() || var.peek() == std::ifstream::traits_type::eof();
    f = std::fopen(cikti.c_str(), "a");
    if (bos)
      std::fprintf(f,
                   "sahne,ucgen,menzil_m,kademe,N_istek,N_gercek,nh,nv,D,is,ping,p50_ms,p99_ms,max_ms,ort_ms,"
                   "kesisim_ort,gorunurluk_ort,kurulum_ms,rss_sahne_mb,embree_sahne_mb,embree_tepe_mb,rss_tepe_mb\n");
  }
  std::printf("# sahne=%s ucgen=%zu kurulum_ms=%.1f rss_artis_mb=%.1f embree_mb=%.1f\n", sahne_ad.c_str(),
              s.toplam_ucgen, s.kurulum_ms, rss1 - rss0, embree_sahne_mb);

  for (int is : Il) {
    Havuz havuz(is);
    for (int D : Dl) {
      if (!tam && D > 0) continue;
      for (int N : Nl) {
        SonarAyar ay = menzil_ayari(menzil);
        ay.N = N;
        ay.D = D;
        ay.tam = tam;
        ay.tohum = 1;
        Sonar son(s, ay);
        Profil p;
        for (int w = 0; w < isinma; ++w) son.ping(1000000 + uint64_t(w), (2 * w + 1) % 400, 0.0, havuz, p);
        std::vector<double> ms(static_cast<size_t>(ping));
        double kes = 0, gor = 0;
        for (int k = 0; k < ping; ++k) {
          const auto t0 = std::chrono::steady_clock::now();
          son.ping(uint64_t(k), (2 * k) % 400, 0.025 * k, havuz, p);
          const auto t1 = std::chrono::steady_clock::now();
          ms[size_t(k)] = std::chrono::duration<double, std::milli>(t1 - t0).count();
          kes += double(p.kesisim);
          gor += double(p.gorunurluk);
        }
        double ort = 0;
        for (double x : ms) ort += x;
        ort /= ping;
        const double p50 = yuzdelik(ms, 0.50), p99 = yuzdelik(ms, 0.99);
        const double mx = *std::max_element(ms.begin(), ms.end());
        const double et = double(g_embree_tepe.load()) / (1024.0 * 1024.0);
        std::printf("%s menzil=%d %s N=%d(%d) D=%d is=%d ping=%d p50=%.3f p99=%.3f max=%.3f ort=%.3f ms "
                    "kesisim=%.0f gorunurluk=%.0f\n",
                    sahne_ad.c_str(), menzil, tam ? "tam" : "sade", N, son.N_gercek(), D, is, ping, p50, p99, mx,
                    ort, kes / ping, gor / ping);
        std::fflush(stdout);
        if (f) {
          std::fprintf(f, "%s,%zu,%d,%s,%d,%d,%d,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                       sahne_ad.c_str(), s.toplam_ucgen, menzil, tam ? "tam" : "sade", N, son.N_gercek(), son.nh(),
                       son.nv(), D, is, ping, p50, p99, mx, ort, kes / ping, gor / ping, s.kurulum_ms, rss1 - rss0,
                       embree_sahne_mb, et, rss_tepe_mb());
          std::fflush(f);
        }
        if (!ham.empty()) {
          char ad[256];
          std::snprintf(ad, sizeof ad, "%s/sure_%s_%dm_%s_N%d_D%d_is%d.txt", ham.c_str(), sahne_ad.c_str(), menzil,
                        tam ? "tam" : "sade", N, D, is);
          if (FILE* h = std::fopen(ad, "w")) {
            for (double x : ms) std::fprintf(h, "%.5f\n", x);
            std::fclose(h);
          }
        }
      }
    }
  }
  if (f) std::fclose(f);
  s.birak();
  rtcReleaseDevice(d);
  return 0;
}

int komut_tekrar(const Arg& a) {
  qos_yuksek();
  const std::string sahne_ad = a.s("sahne", "1e5");
  const int menzil = int(a.i("menzil", 7));
  const int ping = int(a.i("ping", 200));
  RTCDevice d = cihaz(false);
  Sahne s;
  s.kur(d, sahne_ad, SAHNE_TOHUMU);
  Havuz havuz(a.liste("is", "1")[0]);
  SonarAyar ay = menzil_ayari(menzil);
  ay.N = int(a.i("N", 5000));
  ay.D = int(a.i("D", 2));
  ay.tam = a.s("kademe", "tam") == "tam";
  ay.tohum = uint64_t(a.i("tohum", 1));
  Sonar son(s, ay);
  Profil p;
  uint64_t hI = 1469598103934665603ull, hU = hI;
  for (int k = 0; k < ping; ++k) {
    son.ping(uint64_t(k), (2 * k) % 400, 0.025 * k, havuz, p);
    hI = fnv(p.I.data(), p.I.size() * sizeof(double), hI);
    hU = fnv(p.u8.data(), p.u8.size(), hU);
  }
  std::printf("sahne=%s menzil=%d N=%d D=%d is=%d ping=%d ozet_I=%016llx ozet_u8=%016llx\n", sahne_ad.c_str(), menzil,
              son.N_gercek(), ay.D, havuz.boyut(), ping, (unsigned long long)hI, (unsigned long long)hU);
  s.birak();
  rtcReleaseDevice(d);
  return 0;
}

// Tek ping profilini yazar (akıl sağlığı denetimi: duvar tepe kutusu vb.)
int komut_profil(const Arg& a) {
  const std::string sahne_ad = a.s("sahne", "1e5");
  const int menzil = int(a.i("menzil", 7));
  RTCDevice d = cihaz(false);
  Sahne s;
  s.kur(d, sahne_ad, SAHNE_TOHUMU);
  Havuz havuz(a.liste("is", "tumu")[0]);
  SonarAyar ay = menzil_ayari(menzil);
  ay.N = int(a.i("N", 5000));
  ay.D = int(a.i("D", 2));
  ay.tam = a.s("kademe", "tam") == "tam";
  ay.gurultu = a.i("gurultu", 1) != 0;
  Sonar son(s, ay);
  Profil p;
  const int grad = int(a.i("grad", 0));
  son.ping(0, grad, 0.0, havuz, p);
  std::printf("kutu,menzil_m,I,u8\n");
  for (int i = 0; i < p.n_eff; ++i)
    std::printf("%d,%.4f,%.6e,%d\n", i, 1500.0 * (i + 0.5) * p.T_s / 2, p.I[size_t(i)], int(p.u8[size_t(i)]));
  std::fprintf(stderr, "grad=%d N=%d kesisim=%llu gorunurluk=%llu\n", grad, son.N_gercek(),
               (unsigned long long)p.kesisim, (unsigned long long)p.gorunurluk);
  s.birak();
  rtcReleaseDevice(d);
  return 0;
}

// --- yakınsama (05 §6; mimarın düzeltme turu tanımı, ölçümden önce dondurulmuş) ---
// Profiller: P[m][a][i] (gerçekleşim, açı, kutu)
using Prof = std::vector<std::vector<std::vector<double>>>;

struct Buyukluk {
  double dE, dV, dC;
};

// N ve 2N profilleri için üç büyüklük; idx: gerçekleşim indisleri (bootstrap), S: kutu kümesi
Buyukluk buyukluk(const Prof& X1, const Prof& X2, const std::vector<int>& idx, const std::vector<std::vector<char>>& S) {
  const size_t A = X1[0].size(), n = X1[0][0].size();
  const double M = double(idx.size());
  double sE = 0, sE2 = 0, sV = 0, sV2 = 0, C1 = 0, C2 = 0;
  long long nC = 0;
  std::vector<double> E1(n), E2(n), V1(n), V2(n);
  auto ev = [&](const Prof& X, size_t a, size_t i, double& E, double& V) {
    double s = 0, s2 = 0;
    for (int m : idx) {
      const double x = X[size_t(m)][a][i];
      s += x;
      s2 += x * x;
    }
    E = s / M;
    V = std::max(0.0, (s2 - s * s / M) / (M - 1));
  };
  auto ilinti = [&](const Prof& X, size_t a, size_t i, double Ea, double Eb, double Va, double Vb) {
    double c = 0;
    for (int m : idx) c += (X[size_t(m)][a][i] - Ea) * (X[size_t(m)][a][i + 1] - Eb);
    return c / (M - 1) / std::sqrt(Va * Vb);
  };
  for (size_t a = 0; a < A; ++a) {
    for (size_t i = 0; i < n; ++i)
      if (S[a][i]) {
        ev(X1, a, i, E1[i], V1[i]);
        ev(X2, a, i, E2[i], V2[i]);
        sE += std::fabs(E2[i] - E1[i]);
        sE2 += E2[i];
        sV += std::fabs(V2[i] - V1[i]);
        sV2 += V2[i];
      }
    for (size_t i = 0; i + 1 < n; ++i)
      if (S[a][i] && S[a][i + 1] && V1[i] > 0 && V1[i + 1] > 0 && V2[i] > 0 && V2[i + 1] > 0) {
        C1 += ilinti(X1, a, i, E1[i], E1[i + 1], V1[i], V1[i + 1]);
        C2 += ilinti(X2, a, i, E2[i], E2[i + 1], V2[i], V2[i + 1]);
        ++nC;
      }
  }
  const double c1 = nC ? C1 / double(nC) : 0, c2 = nC ? C2 / double(nC) : 0;
  return {sE2 > 0 ? sE / sE2 : 0.0, sV2 > 0 ? sV / sV2 : 0.0, std::fabs(c2 - c1)};
}

int komut_yakinsama(const Arg& a) {
  qos_yuksek();
  const std::string sahne_ad = a.s("sahne", "1e5");
  const int menzil = int(a.i("menzil", 7));
  const int D = int(a.i("D", 2));
  const bool tam = a.s("kademe", "tam") == "tam";
  const bool benek = a.i("benek", 1) != 0;
  const auto Nl = a.liste("N", "625,1250,2500,5000,10000,20000,40000,80000,160000,320000,640000");
  const int M = int(a.i("M", 32));
  const int A = int(a.i("acilar", 8));
  const int is = a.liste("is", "tumu")[0];
  const int B = int(a.i("bootstrap", 1000));
  const uint64_t taban_tohum = uint64_t(a.i("tohum", 1000));
  const uint64_t ref_tohum = uint64_t(a.i("ref_tohum", 5000));
  const int Nref = int(a.i("Nref", *std::max_element(Nl.begin(), Nl.end())));
  const std::string cikti = a.s("cikti", "");

  RTCDevice d = cihaz(false);
  Sahne s;
  s.kur(d, sahne_ad, SAHNE_TOHUMU);
  Havuz havuz(is);

  int n = 0;
  auto uret = [&](int N, uint64_t tohum0, int& Ng) {
    Prof X(static_cast<size_t>(M));
    const auto t0 = std::chrono::steady_clock::now();
    for (int m = 0; m < M; ++m) {
      SonarAyar ay = menzil_ayari(menzil);
      ay.N = N;
      ay.D = D;
      ay.tam = tam;
      ay.sade_benek = benek;
      ay.gurultu = false;  // gürültü kapalı (mimar tanımı md.4)
      ay.tohum = tohum0 + uint64_t(m);  // gerçekleşim tohumu: hücre fazı/benek N'den bağımsız
      Sonar son(s, ay);
      Ng = son.N_gercek();
      Profil p;
      for (int k = 0; k < A; ++k) {
        son.ping(uint64_t(k), (k * (400 / A) + 7) % 400, 1.0 * k, havuz, p);
        X[size_t(m)].push_back(p.I);
      }
      n = p.n_eff;
    }
    std::fprintf(stderr, "N=%d (%d) tohum=%llu: %.1f s\n", N, Ng, (unsigned long long)tohum0,
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    return X;
  };

  // Kutu kümesi S: bağımsız referans (Nref, ayrı tohumlar); açı başına en yüksek
  // ortalama enerjinin 1e-3 katından büyük kutular. Ölçülen koşulardan önce sabitlenir.
  int Nref_g = 0;
  const Prof Rf = uret(Nref, ref_tohum, Nref_g);
  std::vector<std::vector<char>> S(size_t(A), std::vector<char>(size_t(n), 0));
  long long nS = 0;
  for (int ac = 0; ac < A; ++ac) {
    std::vector<double> E(size_t(n), 0.0);
    for (int m = 0; m < M; ++m)
      for (int i = 0; i < n; ++i) E[size_t(i)] += Rf[size_t(m)][size_t(ac)][size_t(i)] / M;
    const double mx = *std::max_element(E.begin(), E.end());
    for (int i = 0; i < n; ++i)
      if (mx > 0 && E[size_t(i)] > 1e-3 * mx) {
        S[size_t(ac)][size_t(i)] = 1;
        ++nS;
      }
  }

  std::vector<Prof> X(Nl.size());
  std::vector<int> Ng(Nl.size());
  for (size_t q = 0; q < Nl.size(); ++q) X[q] = uret(Nl[q], taban_tohum, Ng[q]);

  std::vector<int> tum(static_cast<size_t>(M));
  for (int m = 0; m < M; ++m) tum[size_t(m)] = m;
  const Key kb0 = anahtar(78, 0, 0, Akis::bootstrap);
  auto ve_ort = [&](const Prof& Xq) {  // S üzerinde ortalama V/E^2
    double ve = 0;
    long long k = 0;
    for (int ac = 0; ac < A; ++ac)
      for (int i = 0; i < n; ++i)
        if (S[size_t(ac)][size_t(i)]) {
          double s1 = 0, s2 = 0;
          for (int m = 0; m < M; ++m) {
            const double x = Xq[size_t(m)][size_t(ac)][size_t(i)];
            s1 += x;
            s2 += x * x;
          }
          const double E = s1 / M, V = std::max(0.0, (s2 - s1 * s1 / M) / (M - 1));
          if (E > 0) {
            ve += V / (E * E);
            ++k;
          }
        }
    return k ? ve / double(k) : 0.0;
  };
  if (a.i("hata_tablosu", 0) != 0) {
    // Betimleyici tablo (ölçüt değil): "azaltılmış ışın sayısının bilinen hatası".
    // Referans = listedeki son N, AYNI gerçekleşim tohumlarıyla (eşli; yalnız ışın sayısı hatası).
    // Gürültü tabanı: son N'de bağımsız tohumlu referansa (Rf) göre aynı büyüklükler.
    const size_t son = Nl.size() - 1;
    FILE* fh = cikti.empty() ? nullptr : std::fopen(cikti.c_str(), "w");
    if (fh) std::fprintf(fh, "sahne,menzil_m,kademe,D,M,acilar,S_kutu,N_gercek,Nref_gercek,karsilastirma,dE,dE_ust,dV,dV_ust,dC,dC_ust,VE2\n");
    std::printf("# hata tablosu: sahne=%s menzil=%d D=%d M=%d acilar=%d |S|=%lld referans N=%d (esli)\n",
                sahne_ad.c_str(), menzil, D, M, A, nS, Ng[son]);
    auto satir = [&](const Prof& X1, const Prof& X2, int N1, const char* tur) {
      const Buyukluk nk = buyukluk(X1, X2, tum, S);
      std::vector<double> bE, bV, bC;
      std::vector<int> idx(static_cast<size_t>(M));
      for (int bb = 0; bb < B; ++bb) {
        for (int m = 0; m < M; ++m) idx[size_t(m)] = int(philox({uint32_t(N1), uint32_t(bb), uint32_t(m), 0}, kb0)[0] % uint32_t(M));
        const Buyukluk r = buyukluk(X1, X2, idx, S);
        bE.push_back(r.dE);
        bV.push_back(r.dV);
        bC.push_back(r.dC);
      }
      const double ve = ve_ort(X1);
      std::printf("N=%d %s dE=%.4f (ust %.4f) dV=%.4f (ust %.4f) dC=%.4f (ust %.4f) V/E2=%.3f\n", N1, tur, nk.dE,
                  yuzdelik(bE, 0.975), nk.dV, yuzdelik(bV, 0.975), nk.dC, yuzdelik(bC, 0.975), ve);
      if (fh)
        std::fprintf(fh, "%s,%d,%s,%d,%d,%d,%lld,%d,%d,%s,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.4f\n", sahne_ad.c_str(), menzil,
                     tam ? "tam" : "sade", D, M, A, nS, N1, Ng[son], tur, nk.dE, yuzdelik(bE, 0.975), nk.dV,
                     yuzdelik(bV, 0.975), nk.dC, yuzdelik(bC, 0.975), ve);
    };
    for (size_t q = 0; q < son; ++q) satir(X[q], X[son], Ng[q], "esli_referans");
    satir(X[son], Rf, Ng[son], "gurultu_tabani");
    if (fh) std::fclose(fh);
    s.birak();
    rtcReleaseDevice(d);
    return 0;
  }
  FILE* f = nullptr;
  if (!cikti.empty()) {
    f = std::fopen(cikti.c_str(), "w");
    std::fprintf(f,
                 "sahne,menzil_m,kademe,benek,D,M,acilar,Nref,S_kutu,N,N2,N_gercek,N2_gercek,dE,dE_ust,dV,dV_ust,dC,"
                 "dC_ust,VE2_2N,adim_gecti\n");
  }
  std::printf("# sahne=%s menzil=%d kademe=%s benek=%d D=%d M=%d acilar=%d Nref=%d |S|=%lld bootstrap=%d\n",
              sahne_ad.c_str(), menzil, tam ? "tam" : "sade", int(benek), D, M, A, Nref_g, nS, B);
  const Key kb = anahtar(77, 0, 0, Akis::bootstrap);
  std::vector<int> gecti(Nl.size(), 0);
  for (size_t q = 0; q + 1 < Nl.size(); ++q) {
    const Buyukluk nk = buyukluk(X[q], X[q + 1], tum, S);
    std::vector<double> bE, bV, bC;
    std::vector<int> idx(static_cast<size_t>(M));
    for (int bb = 0; bb < B; ++bb) {
      for (int m = 0; m < M; ++m) idx[size_t(m)] = int(philox({uint32_t(q), uint32_t(bb), uint32_t(m), 0}, kb)[0] % uint32_t(M));
      const Buyukluk r = buyukluk(X[q], X[q + 1], idx, S);
      bE.push_back(r.dE);
      bV.push_back(r.dV);
      bC.push_back(r.dC);
    }
    const double uE = yuzdelik(bE, 0.975), uV = yuzdelik(bV, 0.975), uC = yuzdelik(bC, 0.975);
    // Tanılayıcı: 2N'de S üzerinde ortalama V/E^2 (benek kontrastı karesi)
    double ve = 0;
    long long nve = 0;
    for (int ac = 0; ac < A; ++ac)
      for (int i = 0; i < n; ++i)
        if (S[size_t(ac)][size_t(i)]) {
          double s1 = 0, s2 = 0;
          for (int m = 0; m < M; ++m) {
            const double x = X[q + 1][size_t(m)][size_t(ac)][size_t(i)];
            s1 += x;
            s2 += x * x;
          }
          const double E = s1 / M, V = std::max(0.0, (s2 - s1 * s1 / M) / (M - 1));
          if (E > 0) {
            ve += V / (E * E);
            ++nve;
          }
        }
    ve = nve ? ve / double(nve) : 0;
    gecti[q] = nk.dE <= 0.05 && uE <= 0.05 && nk.dV <= 0.05 && uV <= 0.05 && nk.dC <= 0.05 && uC <= 0.05;
    std::printf("N=%d->%d dE=%.4f (ust %.4f) dV=%.4f (ust %.4f) dC=%.4f (ust %.4f) V/E2=%.3f adim=%d\n", Ng[q],
                Ng[q + 1], nk.dE, uE, nk.dV, uV, nk.dC, uC, ve, gecti[q]);
    std::fflush(stdout);
    if (f)
      std::fprintf(f, "%s,%d,%s,%d,%d,%d,%d,%d,%lld,%d,%d,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.4f,%d\n",
                   sahne_ad.c_str(), menzil, tam ? "tam" : "sade", int(benek), D, M, A, Nref_g, nS, Nl[q], Nl[q + 1],
                   Ng[q], Ng[q + 1], nk.dE, uE, nk.dV, uV, nk.dC, uC, ve, gecti[q]);
  }
  // Ölçüt: N -> 2N adımında geçer VE bir sonraki ikiye katlamada da geçer (kararlılık)
  int bulunan = -1;
  for (size_t q = 0; q + 2 < Nl.size(); ++q)
    if (gecti[q] && gecti[q + 1]) {
      bulunan = Ng[q];
      break;
    }
  std::printf("S2: en kucuk N = %d%s\n", bulunan, bulunan < 0 ? " (bulunamadi)" : "");
  if (f) std::fclose(f);
  s.birak();
  rtcReleaseDevice(d);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::fprintf(stderr, "kullanım: mp7 bilgi|sure|yakinsama|tekrar [--anahtar değer ...]\n");
      return 1;
    }
    const std::string k = argv[1];
    const Arg a(argc, argv, 2);
    if (k == "bilgi") return komut_bilgi();
    if (k == "sure") return komut_sure(a);
    if (k == "tekrar") return komut_tekrar(a);
    if (k == "yakinsama") return komut_yakinsama(a);
    if (k == "profil") return komut_profil(a);
    std::fprintf(stderr, "bilinmeyen komut: %s\n", k.c_str());
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "hata: %s\n", e.what());
    return 1;
  }
}
