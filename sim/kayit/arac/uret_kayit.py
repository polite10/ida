#!/usr/bin/env python3
# sim/kayit kanal kataloğu üreteci (W5-kayit).
#
# Girdi : docs/sozlesmeler/sema/a6-kanallar.json (A6 kataloğu), docs/sozlesmeler/sema/a3-kanallar.json
#         (A3 kataloğu; yalnız ortak tanımların eşitliği denetlenir) ve sim/a3/arac/uret_katalog.py
#         (A3'ün üreteci; modül olarak yüklenir).
# Çıktı : <cikti>/include/ida/kayit/yukler.hpp  A6'ya özgü yük yapıları, Kanal kimlikleri, kanal → yük
#         <cikti>/yukler.cpp                    A6'ya özgü kodlayıcılar, katalog tabloları, gömülü katalog
#   --sinama ile:
#         <cikti>/sinama_uretim.hpp/.cpp        örnek yük doldurucuları ve A6 kataloğundan üretilmiş
#                                               ortak yük kodlayıcıları (yalnız testler; W5-K36)
#
# W5-K36: A3 ile ortak yükler (aynı adlı yük iki katalogda derin eşit olmalıdır; değilse üretim
# durur) `ida::a3` yapıları ve `ida::a3::kodla` ile yazılır; yeniden üretilmez. A6'ya özgü yüklerin
# yapı ve kodlayıcı metni A3 üretecinin AYNI işlevleriyle (`Uretec.yapi_yaz`, `boyut_yaz`,
# `kodlayici_yaz`, `dok_yaz`, `cozucu_yaz`, `tablo_yaz`) üretilir ve aynı `ida::a3::Yazici`'yı
# kullanır. Alan sırası, tipler, sayaçlar ve sınırlar yalnız katalogdan gelir.
#
# W5-K37: `kamera/kare` kanalı ve `yuk_kamera_kare` bu dilimde üretilmez.
#
# Derleme zamanında koşar (Python 3.9+, yalnız standart kitaplık).
import importlib.util
import json
import sys
from pathlib import Path

DISLANAN_KANALLAR = {"kamera/kare"}  # W5-K37
DISLANAN_YUKLER = {"yuk_kamera_kare"}

KOSUL_METINLERI = {
    None: "yok",
    "yalnız çekirdeği PC'de çağıran kiplerde: `sil`, `tekrar_girdi_sil`, `tekrar_yeniden_kurma`": "cekirdek_pc_kipleri",
    "`cekirdek/baslatma` ile aynı kiplerde": "cekirdek_pc_kipleri",
}
# Kademeye bağlı içerik süzgeci olan kanallar (Tablo 3.3 "Ne zaman yazılır"); süzgeçler
# include/ida/kayit/yazici.hpp'dedir. Katalogda bu biçimde başka bir kanal belirirse üretim durur.
SUZGECLI_KANALLAR = {"hil/bariyer", "algilama/parti"}
SUZGEC_ON_EKI = "olaylar kademesinde yalnız"
TEKRAR_BITLERI = {"izleme": 1, "fizik": 2, "girdi": 4, "yeniden_kurma": 8}
KADEMELER = ("olaylar", "telemetri", "tam")
ONLUK_U32_DESENI = (
    "^(0|[1-9][0-9]{0,8}|[1-3][0-9]{9}|4[01][0-9]{8}|42[0-8][0-9]{7}|429[0-3][0-9]{6}|4294[0-8][0-9]{5}"
    "|42949[0-5][0-9]{4}|429496[0-6][0-9]{3}|4294967[01][0-9]{2}|42949672[0-8][0-9]|429496729[0-5])$"
)


def dur(ileti):
    raise SystemExit(f"uret_kayit: {ileti}")


class HamSayi(str):
    """Katalogdaki kayan noktalı sayının kaynak metni; kanonik JSON'a olduğu gibi yazılır."""


def kanonik_json(v):
    # A6-K07: anahtarları sıralanmış, boşluksuz, UTF-8 JSON metni. Sayılar kataloğun kaynak
    # metnindeki yazımıyla kalır; dizeler JSON'ın en kısa kaçışıyla (ASCII dışı ham UTF-8).
    if isinstance(v, dict):
        return "{" + ",".join(json.dumps(k, ensure_ascii=False) + ":" + kanonik_json(v[k]) for k in sorted(v)) + "}"
    if isinstance(v, list):
        return "[" + ",".join(kanonik_json(x) for x in v) + "]"
    if isinstance(v, HamSayi):
        return str(v)
    if isinstance(v, bool):
        return "true" if v else "false"
    if v is None:
        return "null"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, str):
        return json.dumps(v, ensure_ascii=False)
    dur(f"kanonik JSON'a çevrilemeyen değer: {v!r}")


def cpp_dize(s):
    # UTF-8 baytlarını C++ dar dize değişmezine çevirir (kaynak kod sayfasından bağımsız).
    c = []
    onceki_onaltilik = False
    for b in s.encode("utf-8"):
        ch = chr(b)
        if b in (0x22, 0x5C):
            c.append("\\" + ch)
            onceki_onaltilik = False
        elif 0x20 <= b < 0x7F:
            if onceki_onaltilik and ch in "0123456789abcdefABCDEF":
                c.append('""')  # önceki \x kaçışını sonlandır
            c.append(ch)
            onceki_onaltilik = False
        else:
            c.append(f"\\x{b:02x}")
            onceki_onaltilik = True
    return '"' + "".join(c) + '"'


def cpp_uzun_dize(s, girinti="    "):
    # MSVC'nin dize değişmezi sınırına (C2026) takılmamak için parçalara böler.
    parcalar = []
    bayt = s.encode("utf-8")
    i = 0
    while i < len(bayt):
        j = min(i + 1500, len(bayt))
        while j < len(bayt) and (bayt[j] & 0xC0) == 0x80:  # UTF-8 dizisinin ortasında bölme
            j -= 1
        parcalar.append(girinti + cpp_dize(bayt[i:j].decode("utf-8")))
        i = j
    return "\n".join(parcalar) if parcalar else girinti + '""'


def a3_uretecini_yukle(yol):
    spec = importlib.util.spec_from_file_location("ida_a3_uret_katalog", yol)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    # A6'ya özgü yüklerde A3 kataloğunda bulunmayan bir "yok" değeri var: u16 için 0xFFFF
    # (`yuk_hil_dusen.sensor_id`, `yuk_sonar_gercek_ping360.katkilar[].govde_id`). A3 üretecinin
    # tablosu burada genişletilir (sim/a3 değişmez): `AlanTanimi::yok` kodu 8, yalnız `ida::kayit`
    # tablolarında görülür.
    mod.YOK_KODU["0xFFFF"] = 8
    a3_ilklendirici = mod.yok_ilklendirici

    def ilklendirici(tip, yok):
        if yok == "0xFFFF":
            if tip != "u16":
                dur(f"0xFFFF yok değeri {tip} alanında")
            return "0xFFFFu"
        return a3_ilklendirici(tip, yok)

    mod.yok_ilklendirici = ilklendirici
    return mod


def ortak_denetle(k6, k3):
    y6, y3 = k6["x_ida_yukler"], k3["x_ida_yukler"]
    for ad, alanlar in y6.items():
        if ad in y3 and y3[ad] != alanlar:
            dur(f"`{ad}` iki katalogda farklı tanımlı (A6 §7: ortak yükler derin eşit olmalı)")
    a3_kanallari = {kn["ad"]: kn for kn in k3["x_ida_kanallar"]}
    for kn in k6["x_ida_kanallar"]:
        if not kn["a3_ortak"]:
            continue
        o = a3_kanallari.get(kn["ad"])
        if o is None:
            dur(f"`{kn['ad']}` a3_ortak ama A3 kataloğunda yok")
        for alan in ("yuk", "kanal_surumu", "zaman_anlami"):
            if o[alan] != kn[alan]:
                dur(f"`{kn['ad']}` kanalının `{alan}` değeri iki katalogda farklı")
        if kn["yuk"] not in y3:
            dur(f"`{kn['ad']}` a3_ortak ama yükü A3 kataloğunda yok")


def ust_veri_tablolari(k6):
    # Anahtar listesi x_ida_ust_veri'den, zorunluluk ve biçim $defs'ten; ikisi tutarlı olmalı.
    defs = k6["$defs"]
    tablolar = {}
    for kayit in k6["x_ida_ust_veri"]:
        ad = kayit["kayit"]
        sema = defs.get("ust_veri_" + ad)
        if sema is None:
            dur(f"$defs/ust_veri_{ad} yok")
        if sema.get("type") != "object" or sema.get("additionalProperties") is not False:
            dur(f"$defs/ust_veri_{ad}: kapalı nesne değil")
        ozellikler = sema["properties"]
        zorunlu = set(sema.get("required", []))
        anahtarlar = [a["ad"] for a in kayit["anahtarlar"]]
        if sorted(anahtarlar) != sorted(ozellikler):
            dur(f"{ad}: x_ida_ust_veri ile $defs anahtarları farklı")
        satirlar = []
        for a in kayit["anahtarlar"]:
            o = dict(ozellikler[a["ad"]])
            if o.pop("type", None) != "string":
                dur(f"{ad}.{a['ad']}: tip string değil")
            bicim = 0
            desen = o.pop("pattern", None)
            if desen is not None:
                if desen != ONLUK_U32_DESENI:
                    dur(f"{ad}.{a['ad']}: tanınmayan desen (uret_kayit.py güncellenmeli)")
                bicim = 1
            en_az = int(o.pop("minLength", 0))
            if o:
                dur(f"{ad}.{a['ad']}: tanınmayan şema anahtarları {sorted(o)}")
            evet = a["zorunlu"] == "evet"
            if evet != (a["ad"] in zorunlu):
                dur(f"{ad}.{a['ad']}: zorunluluk x_ida_ust_veri ile $defs arasında farklı")
            satirlar.append((a["ad"], evet, bicim, en_az))
        bilinen = {"type", "properties", "additionalProperties", "required", "description", "allOf", "minProperties"}
        if set(sema) - bilinen:
            dur(f"$defs/ust_veri_{ad}: tanınmayan anahtarlar {sorted(set(sema) - bilinen)}")
        if "allOf" in sema:
            beklenen = [{"if": {"properties": {"kip": {"const": "sil"}}, "required": ["kip"]},
                         "then": {"required": ["sil_n_bg"]}}]
            if ad != "ida_kayit" or sema["allOf"] != beklenen:
                dur(f"$defs/ust_veri_{ad}: tanınmayan allOf (uret_kayit.py güncellenmeli)")
        if int(sema.get("minProperties", 0)) not in (0, 1):
            dur(f"$defs/ust_veri_{ad}: tanınmayan minProperties")
        tablolar[ad] = satirlar
    if set(tablolar) != {"ida_kayit", "ida_kayit_guncelleme", "ida_kayit_kapanis"}:
        dur("x_ida_ust_veri: beklenen üç kayıt yok")
    if "allOf" not in defs["ust_veri_ida_kayit"]:
        dur("$defs/ust_veri_ida_kayit: `kip = sil` → `sil_n_bg` kuralı yok")
    return tablolar


class Uretim:
    def __init__(self, a3mod, k6, k6_baytlari):
        self.m = a3mod
        self.k = k6
        self.kb = k6_baytlari
        self.yukler = {y: a for y, a in k6["x_ida_yukler"].items() if y not in DISLANAN_YUKLER}
        self.kanallar = [kn for kn in k6["x_ida_kanallar"] if kn["ad"] not in DISLANAN_KANALLAR]
        for kn in self.kanallar:
            if kn["yuk"] not in self.yukler:
                dur(f"`{kn['ad']}` kanalının yükü yok ya da dışlanmış")
            if kn["kademe"] not in KADEMELER:
                dur(f"`{kn['ad']}`: bilinmeyen kademe")
            if kn["kosul"] not in KOSUL_METINLERI:
                dur(f"`{kn['ad']}`: tanınmayan koşul metni (uret_kayit.py güncellenmeli)")
            suzgecli = str(kn["kayit_hizi"]).startswith(SUZGEC_ON_EKI)
            if suzgecli != (kn["ad"] in SUZGECLI_KANALLAR):
                dur(f"`{kn['ad']}`: kademeye bağlı süzgeç listesi katalogla uyuşmuyor")
        kullanilan = {kn["yuk"] for kn in self.kanallar}
        for y in list(self.yukler):
            if y not in kullanilan:
                dur(f"`{y}` hiçbir kanalda kullanılmıyor")

    def ozgu(self, a3_yukleri):
        return {y: a for y, a in self.yukler.items() if y not in a3_yukleri}

    # ---- ürün çıktısı ----
    def uret(self, a3_yukleri):
        m = self.m
        ozgu = self.ozgu(a3_yukleri)
        u = m.Uretec(self.k, b"", b"")
        for y, alanlar in ozgu.items():
            u.yapi_topla(y, alanlar)
        yapilar = list(u.yapilar)

        soz = self.k["x_ida_sozlesme"]
        mcap = soz["mcap"]
        h = []
        h.append("// ÜRETİLDİ: sim/kayit/arac/uret_kayit.py; elle değiştirilmez.")
        h.append("// Kaynak: docs/sozlesmeler/sema/a6-kanallar.json (A6 §3.3, §3.4).")
        h.append("#pragma once")
        h.append("")
        h.append('#include "ida/a3/yukler.hpp"')
        h.append("")
        h.append("#include <array>")
        h.append("#include <cstddef>")
        h.append("#include <cstdint>")
        h.append("#include <limits>")
        h.append("#include <span>")
        h.append("#include <string>")
        h.append("#include <string_view>")
        h.append("#include <utility>")
        h.append("#include <vector>")
        h.append("")
        h.append("namespace ida::kayit {")
        h.append("")
        h.append(f"inline constexpr std::string_view kA6Surumu = {cpp_dize(soz['surum'])};")
        h.append(f"inline constexpr std::string_view kMcapProfili = {cpp_dize(mcap['profil'])};")
        h.append(f"inline constexpr std::string_view kIletiKodlamasi = {cpp_dize(mcap['ileti_kodlamasi'])};")
        h.append(f"inline constexpr std::string_view kSemaKodlamasi = {cpp_dize(mcap['sema_kodlamasi'])};")
        if mcap["sikistirma"] != "zstd":
            dur("katalogdaki sıkıştırma zstd değil")
        h.append(f"inline constexpr std::uint64_t kParcaHedefiBayt = {int(mcap['parca_hedefi_bayt'])}u;")
        h.append(f"inline constexpr std::int64_t kParcaKapatmaNs = {int(mcap['parca_kapatma_duvar_ms'])} * 1000000LL;")
        h.append("")
        h.append("// A6'ya özgü yükler. A3 ile ortak yükler `ida::a3` yapılarıdır (W5-K36).")
        u.hpp = h
        for yapi_adi, alanlar in yapilar:
            u.yapi_yaz(yapi_adi, alanlar)
        h.append("// Kanal kimlikleri: `kanallar()` içindeki sıra (katalog sırası; MCAP kanal kimliği değildir).")
        h.append("enum class Kanal : std::uint16_t {")
        for i, kn in enumerate(self.kanallar):
            h.append(f"    {m.kanal_sembolu(kn['ad'])} = {i},")
        h.append("};")
        h.append(f"inline constexpr std::size_t kKanalSayisi = {len(self.kanallar)};")
        h.append("")
        h.append("// Kodlayıcılar: yükü katalogdaki sırayla, doldurmasız ve küçük uçlu yazar (A6-K08; A3 §3.1).")
        h.append("// `kodla`, kapasite yetmezse ya da bir sayaç `azami`'yi aşarsa false döner. Bellek ayırmaz.")
        for y in ozgu:
            h.append(f"std::size_t boyut(const {y}& y) noexcept;")
            h.append(f"bool kodla(const {y}& y, std::uint8_t* hedef, std::size_t kapasite, std::size_t& yazilan) noexcept;")
        for yapi_adi, _ in yapilar:
            if yapi_adi not in ozgu:
                h.append(f"std::size_t boyut(const {yapi_adi}& y) noexcept;")
        h.append("")
        h.append("// Sınamalar için değer dökümü (JSON; A3'ün `json_dok` biçimi).")
        for y in ozgu:
            h.append(f"std::string json_dok(const {y}& y);")
        h.append("")
        h.append("// Çözücüler (yalnız dizi içermeyen yükler): metin ve bayt alanları kaynağı gösterir.")
        for y, alanlar in ozgu.items():
            if not any(a["tip"] == "dizi" for a in alanlar):
                h.append(f"bool coz(const std::uint8_t* kaynak, std::size_t uzunluk, {y}& y) noexcept;")
        h.append("")
        h.append("// Kanal → yük eşlemesi.")
        h.append("template <Kanal K> struct KanalYuku;")
        for i, kn in enumerate(self.kanallar):
            y = kn["yuk"]
            tip = y if y in ozgu else f"::ida::a3::{y}"
            h.append(f"template <> struct KanalYuku<Kanal::{m.kanal_sembolu(kn['ad'])}> {{ using tip = {tip}; }};")
        h.append("")
        h.append("// Üst veri kayıtlarının çağıran tarafından verilen biçimi (A6 §3.5): her anahtar bir metin.")
        tablolar = ust_veri_tablolari(self.k)
        h.append("struct AcilisUstVerisi {")
        for ad_, _, _, _ in tablolar["ida_kayit"]:
            h.append(f"    std::string {m.ad(ad_)};")
        h.append("};")
        h.append("// Anahtar → değer çiftleri, katalog sırasıyla (boş değerler dahil).")
        h.append("std::vector<std::pair<std::string_view, std::string_view>> ust_veri_ciftleri(const AcilisUstVerisi& u);")
        h.append("// Anahtarın alanı; anahtar katalogda yoksa nullptr.")
        h.append("std::string* ust_veri_alani(AcilisUstVerisi& u, std::string_view anahtar) noexcept;")
        h.append("")
        h.append("}  // namespace ida::kayit")
        h.append("")

        c = []
        u.cpp = c
        c.append("// ÜRETİLDİ: sim/kayit/arac/uret_kayit.py; elle değiştirilmez.")
        c.append('#include "ida/kayit/yukler.hpp"')
        c.append('#include "ida/kayit/katalog.hpp"')
        c.append('#include "ida/a3/kodlayici.hpp"')
        c.append("")
        c.append("#include <iterator>")
        c.append("#include <utility>")
        c.append("")
        c.append("namespace ida::kayit {")
        c.append("")
        c.append("// A3 ile aynı tel tipi yazıcısı ve tablo tipleri (W5-K36).")
        for s in ("Yazici", "Okuyucu", "AlanTanimi", "DegerAdi", "YukTanimi", "Tip", "dok_tam", "dok_64", "dok_kayan",
                  "dok_kayan_yok", "dok_metin"):
            c.append(f"using ::ida::a3::{s};")
        c.append("")
        for yapi_adi, alanlar in yapilar:
            u.boyut_yaz(yapi_adi, alanlar)
        c.append("namespace {")
        c.append("")
        c.append(f"constexpr std::uint8_t k_katalog[{len(self.kb)}] = {{")
        for i in range(0, len(self.kb), 24):
            c.append("    " + ",".join(str(x) for x in self.kb[i:i + 24]) + ",")
        c.append("};")
        c.append("")
        for y, alanlar in self.yukler.items():
            u.tablo_yaz(y, alanlar)
        c.append("constexpr YukTanimi k_yukler[] = {")
        for y in self.yukler:
            c.append(f'    {{"{y}", k_{y}_alanlar, static_cast<std::uint16_t>(std::size(k_{y}_alanlar))}},')
        c.append("};")
        yuk_indeks = {y: i for i, y in enumerate(self.yukler)}
        # Şema kaydı: her (yük, kanal_surumu) için bir tane (A6 §3.2).
        semalar = []
        sema_indeks = {}
        for kn in self.kanallar:
            anahtar = (kn["yuk"], int(kn["kanal_surumu"]))
            if anahtar not in sema_indeks:
                sema_indeks[anahtar] = len(semalar)
                semalar.append(anahtar)
        for i, (y, surum) in enumerate(semalar):
            c.append(f"constexpr char k_sema_{i}[] =")
            c.append(cpp_uzun_dize(kanonik_json(self.yukler[y])) + ";")
        c.append("constexpr SemaTanimi k_semalar[] = {")
        for i, (y, surum) in enumerate(semalar):
            c.append(f'    {{"ida.{y}.v{surum}", &k_yukler[{yuk_indeks[y]}], {surum}, '
                     f"std::string_view(k_sema_{i}, sizeof k_sema_{i} - 1)}},")
        c.append("};")
        c.append("constexpr KanalTanimi k_kanallar[] = {")
        for i, kn in enumerate(self.kanallar):
            bit = 0
            for d in kn["tekrar_duzeyi"]:
                if d not in TEKRAR_BITLERI:
                    dur(f"`{kn['ad']}`: bilinmeyen tekrar düzeyi {d}")
                bit |= TEKRAR_BITLERI[d]
            c.append(
                "    {"
                f'"{kn["ad"]}", {int(kn["kanal_surumu"])}, &k_yukler[{yuk_indeks[kn["yuk"]]}], '
                f'{sema_indeks[(kn["yuk"], int(kn["kanal_surumu"]))]}, Kademe::{kn["kademe"]}, "{kn["kademe"]}", '
                f'"{kn["zaman_anlami"]}", {"true" if kn["a3_ortak"] else "false"}, {bit}, '
                f'KanalKosulu::{KOSUL_METINLERI[kn["kosul"]]}, {i}'
                "},"
            )
        c.append("};")
        for ad_, satirlar in tablolar.items():
            c.append(f"constexpr UstVeriAnahtari k_{ad_}_anahtarlari[] = {{")
            for anahtar, zorunlu, bicim, en_az in satirlar:
                c.append(f'    {{"{anahtar}", {"true" if zorunlu else "false"}, '
                         f'UstVeriBicimi::{"onluk_u32" if bicim == 1 else "serbest"}, {en_az}}},')
            c.append("};")
        c.append("constexpr EkTanimi k_ekler[] = {")
        for e in self.k["x_ida_ekler"]:
            c.append(f'    {{{cpp_dize(e["ad"])}, {cpp_dize(e["media_type"])}, {"true" if e["zorunlu"] == "evet" else "false"}}},')
        c.append("};")
        c.append("")
        for yapi_adi, alanlar in yapilar:
            u.kodlayici_yaz(yapi_adi, alanlar, yapi_adi in ozgu)
        for yapi_adi, alanlar in yapilar:
            u.dok_yaz(yapi_adi, alanlar)
        c.append("}  // namespace")
        c.append("")
        for y in ozgu:
            u.kodla_sarmalayici_yaz(y)
            c.append(f"std::string json_dok(const {y}& y)")
            c.append("{")
            c.append("    std::string c;")
            c.append("    dok_ic(y, c);")
            c.append("    return c;")
            c.append("}")
            c.append("")
        for y, alanlar in ozgu.items():
            u.cozucu_yaz(y, alanlar)
        c.append("std::span<const std::uint8_t> katalog_baytlari() noexcept { return k_katalog; }")
        c.append("std::span<const YukTanimi> yukler() noexcept { return k_yukler; }")
        c.append("std::span<const SemaTanimi> semalar() noexcept { return k_semalar; }")
        c.append("std::span<const KanalTanimi> kanallar() noexcept { return k_kanallar; }")
        c.append("std::span<const UstVeriAnahtari> acilis_anahtarlari() noexcept { return k_ida_kayit_anahtarlari; }")
        c.append("std::span<const UstVeriAnahtari> guncelleme_anahtarlari() noexcept { return k_ida_kayit_guncelleme_anahtarlari; }")
        c.append("std::span<const UstVeriAnahtari> kapanis_anahtarlari() noexcept { return k_ida_kayit_kapanis_anahtarlari; }")
        c.append("std::span<const EkTanimi> ekler() noexcept { return k_ekler; }")
        c.append("")
        c.append("std::vector<std::pair<std::string_view, std::string_view>> ust_veri_ciftleri(const AcilisUstVerisi& u)")
        c.append("{")
        c.append("    return {")
        for ad_, _, _, _ in tablolar["ida_kayit"]:
            c.append(f'        {{"{ad_}", u.{m.ad(ad_)}}},')
        c.append("    };")
        c.append("}")
        c.append("")
        c.append("std::string* ust_veri_alani(AcilisUstVerisi& u, std::string_view anahtar) noexcept")
        c.append("{")
        for ad_, _, _, _ in tablolar["ida_kayit"]:
            c.append(f'    if (anahtar == "{ad_}") {{ return &u.{m.ad(ad_)}; }}')
        c.append("    return nullptr;")
        c.append("}")
        c.append("")
        c.append("}  // namespace ida::kayit")
        c.append("")
        return "\n".join(h), "\n".join(c)

    # ---- sınama çıktısı ----
    def doldurucu(self, c, yapi_adi, alanlar):
        # Şablon: aynı metin `ida::a3`, `ida::kayit` ve `sinama_a6` yapılarını doldurur (alan adları
        # katalogdan; yapı tipinden bağımsız).
        m = self.m
        for a in alanlar:
            if a["tip"] == "dizi":
                self.doldurucu(c, f"{yapi_adi}_{a['ad']}", a["oge"])
        c.append(f"template <class Y> void doldur_{yapi_adi}(Y& y, Depo& d, Rastgele& r)")
        c.append("{")
        c.append("    (void)d;")
        c.append("    (void)r;")
        for a in alanlar:
            t = a["tip"]
            alan = f"y.{m.ad(a['ad'])}"
            yok = a.get("yok_degeri")
            if t in m.TAMSAYI:
                temel = m.TAMSAYI[t][0]
                ilk = m.yok_ilklendirici(t, yok) if yok is not None else None
                if yok == "0":
                    ilk = "0"
                if ilk is not None:
                    uret = f"r.tam_yoklu<{temel}>(static_cast<{temel}>({ilk}))"
                else:
                    uret = f"r.tam<{temel}>()"
                if "adet" in a:
                    c.append(f"    for (auto& v : {alan}) {{ v = {uret}; }}")
                else:
                    c.append(f"    {alan} = {uret};")
            elif t in m.KAYAN:
                temel = m.KAYAN[t][0]
                uret = f"r.kayan<{temel}>({'true' if yok == 'NaN' else 'false'})"
                if "adet" in a:
                    c.append(f"    for (auto& v : {alan}) {{ v = {uret}; }}")
                else:
                    c.append(f"    {alan} = {uret};")
            elif t == "metin":
                c.append(f"    {alan} = d.metin(r, {int(a['azami'])}u);")
            elif t == "bayt_dizisi":
                c.append(f"    {alan} = d.baytlar(r, {int(a['azami'])}u);")
            elif t == "dizi":
                oge = f"{yapi_adi}_{a['ad']}"
                c.append("    {")
                c.append(f"        using Oge = std::remove_const_t<typename decltype({alan})::element_type>;")
                c.append(f"        std::vector<Oge> v(r.adet({int(a['azami'])}u));")
                c.append(f"        for (auto& e : v) {{ doldur_{oge}(e, d, r); }}")
                c.append(f"        {alan} = d.tut(std::move(v));")
                c.append("    }")
            else:
                dur(f"bilinmeyen tip {t}")
        c.append("}")
        c.append("")

    def uret_sinama(self, a3_yukleri):
        m = self.m
        ozgu = self.ozgu(a3_yukleri)
        ortak = {y: a for y, a in self.yukler.items() if y in a3_yukleri}
        # Ortak yüklerin A6 kataloğundan üretilmiş ikinci kodlayıcısı (yalnız karşılaştırma için).
        u = m.Uretec(self.k, b"", b"")
        for y, alanlar in ortak.items():
            u.yapi_topla(y, alanlar)
        yapilar = list(u.yapilar)

        h = []
        h.append("// ÜRETİLDİ: sim/kayit/arac/uret_kayit.py --sinama; elle değiştirilmez. Yalnız testler.")
        h.append("#pragma once")
        h.append("")
        h.append('#include "ida/kayit/yazici.hpp"')
        h.append('#include "ida/kayit/yukler.hpp"')
        h.append('#include "sinama_destek.hpp"')
        h.append("")
        h.append("#include <cstdint>")
        h.append("#include <string>")
        h.append("#include <vector>")
        h.append("")
        h.append("namespace ida::kayit::test {")
        h.append("")
        h.append("// Kanalın yükünden tohumla belirlenen bir örnek: ürün kodlayıcısının baytları ve değer dökümü.")
        h.append("struct Ornek {")
        h.append("    std::vector<std::uint8_t> bayt;")
        h.append("    std::string json;")
        h.append("};")
        h.append("Ornek ornek_uret(Kanal k, std::uint64_t tohum);")
        h.append("// Aynı örneği üreticinin tipli `yaz<K>` yolundan yazar (`kayit/eksik` yazılmaz: `suzuldu`).")
        h.append("YazmaSonucu ornek_yaz(Uretici& u, Kanal k, std::uint64_t tohum, std::int64_t sim_t_ns, std::uint64_t adim);")
        h.append("// W5-K36: ortak yükü A3 kodlayıcısıyla ve A6 kataloğundan üretilmiş kodlayıcıyla kodlar;")
        h.append("// baytlar aynıysa true. `yuk_adi` ortak değilse false ve `ortak` false.")
        h.append("bool ortak_kodlayici_esit(std::string_view yuk_adi, std::uint64_t tohum, bool& ortak, std::size_t& bayt);")
        h.append("std::vector<std::string_view> ortak_yuk_adlari();")
        h.append("// Yük yapısını tohumdan doldurur; yapının gösterdiği bellek `d` yaşadıkça geçerlidir.")
        for y in self.yukler:
            tip = y if y in ozgu else f"::ida::a3::{y}"
            h.append(f"void ornek_doldur({tip}& y, Depo& d, std::uint64_t tohum);")
        h.append("")
        h.append("}  // namespace ida::kayit::test")
        h.append("")

        c = []
        c.append("// ÜRETİLDİ: sim/kayit/arac/uret_kayit.py --sinama; elle değiştirilmez. Yalnız testler.")
        c.append('#include "sinama_uretim.hpp"')
        c.append("")
        c.append('#include "ida/a3/kodlayici.hpp"')
        c.append("")
        c.append("#include <limits>")
        c.append("#include <span>")
        c.append("#include <type_traits>")
        c.append("#include <utility>")
        c.append("")
        c.append("namespace ida::kayit::test {")
        c.append("")
        c.append("namespace sinama_a6 {")
        c.append("using ::ida::a3::Yazici;")
        u.hpp = c
        for yapi_adi, alanlar in yapilar:
            u.yapi_yaz(yapi_adi, alanlar)
        u.cpp = c
        c.append("namespace {")
        for yapi_adi, alanlar in yapilar:
            u.kodlayici_yaz(yapi_adi, alanlar, yapi_adi in ortak)
        c.append("}  // namespace")
        for y in ortak:
            u.kodla_sarmalayici_yaz(y)
        c.append("}  // namespace sinama_a6")
        c.append("")
        c.append("namespace {")
        c.append("")
        for y, alanlar in self.yukler.items():
            self.doldurucu(c, y, alanlar)
        c.append("template <class Y> Ornek ornek_kodla(const Y& y)")
        c.append("{")
        c.append("    Ornek o;")
        c.append("    o.bayt.resize(boyut(y));")
        c.append("    std::size_t yazilan = 0;")
        c.append("    if (!kodla(y, o.bayt.data(), o.bayt.size(), yazilan) || yazilan != o.bayt.size()) {")
        c.append("        o.bayt.clear();")
        c.append('        o.json = "KODLANAMADI";')
        c.append("        return o;")
        c.append("    }")
        c.append("    o.json = json_dok(y);")
        c.append("    return o;")
        c.append("}")
        c.append("")
        c.append("}  // namespace")
        c.append("")
        c.append("Ornek ornek_uret(Kanal k, std::uint64_t tohum)")
        c.append("{")
        c.append("    Depo d;")
        c.append("    Rastgele r(tohum);")
        c.append("    switch (k) {")
        for kn in self.kanallar:
            s = m.kanal_sembolu(kn["ad"])
            c.append(f"    case Kanal::{s}: {{")
            c.append(f"        KanalYuku<Kanal::{s}>::tip y;")
            c.append(f"        doldur_{kn['yuk']}(y, d, r);")
            c.append("        return ornek_kodla(y);")
            c.append("    }")
        c.append("    }")
        c.append("    return {};")
        c.append("}")
        c.append("")
        c.append("YazmaSonucu ornek_yaz(Uretici& u, Kanal k, std::uint64_t tohum, std::int64_t sim_t_ns, std::uint64_t adim)")
        c.append("{")
        c.append("    Depo d;")
        c.append("    Rastgele r(tohum);")
        c.append("    switch (k) {")
        for kn in self.kanallar:
            s = m.kanal_sembolu(kn["ad"])
            if kn["ad"] == "kayit/eksik":
                # Yalnız kayıt yazıcısı yazar; üretici yolundan yazılamaz (derleme hatası).
                c.append(f"    case Kanal::{s}:")
                c.append("        return YazmaSonucu::suzuldu;")
                continue
            c.append(f"    case Kanal::{s}: {{")
            c.append(f"        KanalYuku<Kanal::{s}>::tip y;")
            c.append(f"        doldur_{kn['yuk']}(y, d, r);")
            c.append(f"        return u.yaz<Kanal::{s}>(sim_t_ns, adim, y);")
            c.append("    }")
        c.append("    }")
        c.append("    return YazmaSonucu::kapali;")
        c.append("}")
        c.append("")
        c.append("bool ortak_kodlayici_esit(std::string_view yuk_adi, std::uint64_t tohum, bool& ortak, std::size_t& bayt)")
        c.append("{")
        c.append("    ortak = true;")
        c.append("    bayt = 0;")
        for y in ortak:
            c.append(f'    if (yuk_adi == "{y}") {{')
            c.append("        Depo d1;")
            c.append("        Depo d2;")
            c.append("        Rastgele r1(tohum);")
            c.append("        Rastgele r2(tohum);")
            c.append(f"        ::ida::a3::{y} a;")
            c.append(f"        sinama_a6::{y} b;")
            c.append(f"        doldur_{y}(a, d1, r1);")
            c.append(f"        doldur_{y}(b, d2, r2);")
            c.append("        std::vector<std::uint8_t> ba(::ida::a3::boyut(a));")
            c.append("        std::vector<std::uint8_t> bb(ba.size() + 16u);")
            c.append("        std::size_t na = 0;")
            c.append("        std::size_t nb = 0;")
            c.append("        if (!::ida::a3::kodla(a, ba.data(), ba.size(), na) || !sinama_a6::kodla(b, bb.data(), bb.size(), nb)) {")
            c.append("            return false;")
            c.append("        }")
            c.append("        bayt = na;")
            c.append("        bb.resize(nb);")
            c.append("        return na == ba.size() && ba == bb;")
            c.append("    }")
        c.append("    ortak = false;")
        c.append("    return false;")
        c.append("}")
        c.append("")
        for y in self.yukler:
            tip = y if y in ozgu else f"::ida::a3::{y}"
            c.append(f"void ornek_doldur({tip}& y, Depo& d, std::uint64_t tohum)")
            c.append("{")
            c.append("    Rastgele r(tohum);")
            c.append(f"    doldur_{y}(y, d, r);")
            c.append("}")
            c.append("")
        c.append("std::vector<std::string_view> ortak_yuk_adlari()")
        c.append("{")
        c.append("    return {")
        for y in ortak:
            c.append(f'        "{y}",')
        c.append("    };")
        c.append("}")
        c.append("")
        c.append("}  // namespace ida::kayit::test")
        c.append("")
        return "\n".join(h), "\n".join(c)


def main():
    arg = sys.argv[1:]
    sinama = "--sinama" in arg
    arg = [a for a in arg if a != "--sinama"]
    if len(arg) != 4:
        dur("kullanım: uret_kayit.py [--sinama] <a6-kanallar.json> <a3-kanallar.json> <a3/arac/uret_katalog.py> <cikti>")
    k6_yolu, k3_yolu, a3_uretec, cikti = map(Path, arg)
    k6_baytlari = k6_yolu.read_bytes()
    k6 = json.loads(k6_baytlari.decode("utf-8"), parse_float=HamSayi)
    k3 = json.loads(k3_yolu.read_bytes().decode("utf-8"), parse_float=HamSayi)
    ortak_denetle(k6, k3)
    if k6["x_ida_tipler"].keys() != k3["x_ida_tipler"].keys():
        dur("iki katalogun tel tipi kümeleri farklı")
    a3mod = a3_uretecini_yukle(a3_uretec)
    ur = Uretim(a3mod, k6, k6_baytlari)
    a3_yukleri = set(k3["x_ida_yukler"])
    if sinama:
        hpp, cpp = ur.uret_sinama(a3_yukleri)
        cikti.mkdir(parents=True, exist_ok=True)
        hedefler = ((cikti / "sinama_uretim.hpp", hpp), (cikti / "sinama_uretim.cpp", cpp))
    else:
        hpp, cpp = ur.uret(a3_yukleri)
        (cikti / "include" / "ida" / "kayit").mkdir(parents=True, exist_ok=True)
        hedefler = ((cikti / "include" / "ida" / "kayit" / "yukler.hpp", hpp), (cikti / "yukler.cpp", cpp))
    for yol, metin in hedefler:
        yol.write_text(metin, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
