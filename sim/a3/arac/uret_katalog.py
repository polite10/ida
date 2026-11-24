#!/usr/bin/env python3
# sim/a3 kanal kataloğu üreteci (W4-a3).
#
# Girdi : docs/sozlesmeler/sema/a3-kanallar.json (A3 kataloğu) ve protocol/komutlar.json (A1 komut
#         kataloğu).
# Çıktı : <cikti>/include/ida/a3/yukler.hpp  yük yapıları, kanal kimlikleri, kodlayıcı bildirimleri
#         <cikti>/yukler.cpp                 kodlayıcılar, çözücüler, katalog tabloları, gömülü baytlar
#
# Alan sırası, tipleri, sayaçlar ve sınırlar yalnız katalogdan gelir; elle yazılmış alan sırası
# yoktur (W4-a3 kapsam 3). `ida_a3_test katalog_tablosu` üretilen tabloyu çalışma anında
# katalogla karşılaştırır. Derleme zamanında koşar (Python 3.9+, yalnız standart kitaplık).
import json
import sys
from pathlib import Path

TAMSAYI = {
    "u8": ("std::uint8_t", 1),
    "u16": ("std::uint16_t", 2),
    "u32": ("std::uint32_t", 4),
    "u64": ("std::uint64_t", 8),
    "i32": ("std::int32_t", 4),
    "i64": ("std::int64_t", 8),
}
KAYAN = {"f32": ("float", 4), "f64": ("double", 8)}
SAYAC_TIPI = {"u8": "u8", "u16": "u16", "u32": "u32"}
# AlanTanimi::yok kodları (katalog.hpp): 0 yok değeri tanımsız
YOK_KODU = {None: 0, "NaN": 1, "INT64_MIN": 2, "2^64-1": 3, "0xFFFFFFFF": 4, "255": 5, "-1": 6, "0": 7}
CPP_ANAHTAR = {
    "alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch", "char",
    "class", "const", "continue", "default", "delete", "do", "double", "else", "enum", "explicit",
    "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
    "mutable", "namespace", "new", "not", "operator", "or", "private", "protected", "public",
    "register", "return", "short", "signed", "sizeof", "static", "struct", "switch", "template",
    "this", "throw", "true", "try", "typedef", "typename", "union", "unsigned", "using", "virtual",
    "void", "volatile", "while", "xor",
}


def ad(s):
    if not s.replace("_", "").isalnum() or not s.isascii():
        raise SystemExit(f"uret_katalog: C++ adına çevrilemeyen alan adı: {s!r}")
    return s + "_" if s in CPP_ANAHTAR else s


def kanal_sembolu(s):
    return ad(s.replace("/", "_"))


def yok_ilklendirici(tip, yok):
    if yok is None:
        return None
    if tip in KAYAN:
        if yok != "NaN":
            raise SystemExit(f"uret_katalog: kayan noktalı alanda beklenmeyen yok değeri {yok}")
        return f"std::numeric_limits<{KAYAN[tip][0]}>::quiet_NaN()"
    tablo = {
        "INT64_MIN": "std::numeric_limits<std::int64_t>::min()",
        "2^64-1": "std::numeric_limits<std::uint64_t>::max()",
        "0xFFFFFFFF": "0xFFFFFFFFu",
        "255": "255u",
        "-1": "-1",
        "0": None,
    }
    if yok not in tablo:
        raise SystemExit(f"uret_katalog: bilinmeyen yok değeri {yok!r}")
    return tablo[yok]


class Uretec:
    def __init__(self, katalog, komutlar_baytlari, katalog_baytlari):
        self.k = katalog
        self.kb = katalog_baytlari
        self.kmb = komutlar_baytlari
        self.hpp = []
        self.cpp = []
        self.yapilar = []  # (yapı adı, alanlar) bağımlılık sırasıyla
        self.tablolar = []

    # ---- yapı ve tablo ----
    def yapi_topla(self, yapi_adi, alanlar):
        for a in alanlar:
            if a["tip"] == "dizi":
                self.yapi_topla(f"{yapi_adi}_{a['ad']}", a["oge"])
        self.yapilar.append((yapi_adi, alanlar))

    def alan_cpp_tipi(self, yapi_adi, a):
        t = a["tip"]
        if t in TAMSAYI or t in KAYAN:
            temel = (TAMSAYI.get(t) or KAYAN[t])[0]
            if "adet" in a:
                return f"std::array<{temel}, {int(a['adet'])}>"
            return temel
        if t == "metin":
            return "std::string_view"
        if t == "bayt_dizisi":
            return "std::span<const std::uint8_t>"
        if t == "dizi":
            return f"std::span<const {yapi_adi}_{a['ad']}>"
        raise SystemExit(f"uret_katalog: bilinmeyen tip {t}")

    def yapi_yaz(self, yapi_adi, alanlar):
        h = self.hpp
        h.append(f"struct {yapi_adi} {{")
        for a in alanlar:
            cpp = self.alan_cpp_tipi(yapi_adi, a)
            ilk = yok_ilklendirici(a["tip"], a.get("yok_degeri"))
            if ilk is None:
                h.append(f"    {cpp} {ad(a['ad'])}{{}};")
            elif "adet" in a:
                liste = ", ".join([ilk] * int(a["adet"]))
                h.append(f"    {cpp} {ad(a['ad'])}{{{liste}}};")
            else:
                temel = (TAMSAYI.get(a['tip']) or KAYAN[a['tip']])[0]
                h.append(f"    {cpp} {ad(a['ad'])}{{static_cast<{temel}>({ilk})}};")
        h.append("};")
        h.append("")

    def tablo_yaz(self, yapi_adi, alanlar):
        c = self.cpp
        for a in alanlar:
            if a["tip"] == "dizi":
                self.tablo_yaz(f"{yapi_adi}_{a['ad']}", a["oge"])
            if "degerler" in a:
                c.append(f"constexpr DegerAdi k_{yapi_adi}_{a['ad']}_degerler[] = {{")
                for kod, adi in a["degerler"].items():
                    c.append(f"    {{{int(kod)}u, \"{adi}\"}},")
                c.append("};")
        c.append(f"constexpr AlanTanimi k_{yapi_adi}_alanlar[] = {{")
        for a in alanlar:
            t = a["tip"]
            adet = int(a.get("adet", 0))
            sayac = SAYAC_TIPI.get(a.get("sayac", ""), None)
            sayac_ifade = f"Tip::{sayac}" if sayac else "Tip::u8"
            azami = int(a.get("azami", 0))
            if t == "dizi":
                oge = f"k_{yapi_adi}_{a['ad']}_alanlar"
                oge_n = f"static_cast<std::uint16_t>(std::size(k_{yapi_adi}_{a['ad']}_alanlar))"
            else:
                oge, oge_n = "nullptr", "0"
            if "degerler" in a:
                dg = f"k_{yapi_adi}_{a['ad']}_degerler"
                dg_n = f"static_cast<std::uint16_t>(std::size({dg}))"
            else:
                dg, dg_n = "nullptr", "0"
            yok_kodu = YOK_KODU[a.get("yok_degeri")]
            c.append(
                f"    {{\"{a['ad']}\", Tip::{t}, {adet}, {sayac_ifade}, {azami}u, {oge}, {oge_n}, {dg}, {dg_n}, {yok_kodu}}},"
            )
        c.append("};")

    # ---- kodlayıcı ----
    def boyut_govdesi(self, yapi_adi, alanlar, degisken):
        satirlar = []
        sabit = 0
        for a in alanlar:
            t = a["tip"]
            alan = f"{degisken}.{ad(a['ad'])}"
            if t in TAMSAYI or t in KAYAN:
                b = (TAMSAYI.get(t) or KAYAN[t])[1]
                sabit += b * int(a.get("adet", 1))
            elif t in ("metin", "bayt_dizisi"):
                sabit += {"u8": 1, "u16": 2, "u32": 4}[a["sayac"]]
                satirlar.append(f"    n += {alan}.size();")
            elif t == "dizi":
                sabit += {"u8": 1, "u16": 2, "u32": 4}[a["sayac"]]
                satirlar.append(f"    for (const auto& e : {alan}) {{ n += boyut(e); }}")
        return sabit, satirlar

    def kodla_govdesi(self, yapi_adi, alanlar, degisken):
        s = []
        for a in alanlar:
            t = a["tip"]
            alan = f"{degisken}.{ad(a['ad'])}"
            if t in TAMSAYI or t in KAYAN:
                if "adet" in a:
                    s.append(f"    for (const auto v : {alan}) {{ w.{t}(v); }}")
                else:
                    s.append(f"    w.{t}({alan});")
            elif t in ("metin", "bayt_dizisi"):
                s.append(f"    if ({alan}.size() > {int(a['azami'])}u) {{ return false; }}")
                s.append(f"    w.{a['sayac']}(static_cast<{TAMSAYI[a['sayac']][0]}>({alan}.size()));")
                s.append(f"    w.baytlar(reinterpret_cast<const std::uint8_t*>({alan}.data()), {alan}.size());")
            elif t == "dizi":
                s.append(f"    if ({alan}.size() > {int(a['azami'])}u) {{ return false; }}")
                s.append(f"    w.{a['sayac']}(static_cast<{TAMSAYI[a['sayac']][0]}>({alan}.size()));")
                s.append(f"    for (const auto& e : {alan}) {{ if (!kodla_oge(e, w)) {{ return false; }} }}")
        return s

    def boyut_yaz(self, yapi_adi, alanlar):
        c = self.cpp
        sabit, satirlar = self.boyut_govdesi(yapi_adi, alanlar, "y")
        c.append(f"std::size_t boyut(const {yapi_adi}& y) noexcept")
        c.append("{")
        c.append(f"    std::size_t n = {sabit}u;")
        c.extend(satirlar)
        if not satirlar:
            c.append("    (void)y;")
        c.append("    return n;")
        c.append("}")
        c.append("")

    def kodlayici_yaz(self, yapi_adi, alanlar, ust_duzey):
        c = self.cpp
        ad_ = "kodla_ic" if ust_duzey else "kodla_oge"
        c.append(f"bool {ad_}(const {yapi_adi}& y, Yazici& w) noexcept")
        c.append("{")
        c.extend(self.kodla_govdesi(yapi_adi, alanlar, "y"))
        c.append("    return w.tamam();")
        c.append("}")
        c.append("")

    def kodla_sarmalayici_yaz(self, yapi_adi):
        c = self.cpp
        c.append(f"bool kodla(const {yapi_adi}& y, std::uint8_t* hedef, std::size_t kapasite, std::size_t& yazilan) noexcept")
        c.append("{")
        c.append("    Yazici w(hedef, kapasite);")
        c.append("    if (!kodla_ic(y, w)) { return false; }")
        c.append("    yazilan = w.konum();")
        c.append("    return true;")
        c.append("}")
        c.append("")

    def dok_yaz(self, yapi_adi, alanlar):
        # Sınamalar için değer dökümü (JSON): 32 bit ve altı tamsayılar sayı, 64 bit tamsayılar
        # ondalık dize, "yok" değeri null, sonlu olmayan kayan nokta "NaN"/"Infinity" dizesi.
        c = self.cpp
        c.append(f"void dok_ic(const {yapi_adi}& y, std::string& c)")
        c.append("{")
        c.append("    c.push_back('{');")
        for i, a in enumerate(alanlar):
            t = a["tip"]
            alan = f"y.{ad(a['ad'])}"
            on = "," if i > 0 else ""
            c.append("    c += \"" + on + "\\\"" + a['ad'] + "\\\":\";")
            yok = a.get("yok_degeri")

            def skaler(ifade, t=t, yok=yok):
                if t in KAYAN:
                    if yok == "NaN":
                        return f"dok_kayan_yok(c, {ifade});"
                    return f"dok_kayan(c, {ifade});"
                ilk = yok_ilklendirici(t, yok) if yok is not None else None
                if yok == "0":
                    ilk = "0"
                fn = "dok_64" if t in ("u64", "i64") else "dok_tam"
                if ilk is None:
                    return f"{fn}(c, {ifade});"
                temel = TAMSAYI[t][0]
                return "if (" + ifade + " == static_cast<" + temel + ">(" + ilk + ")) { c += \"null\"; } else { " + fn + "(c, " + ifade + "); }"

            if t in TAMSAYI or t in KAYAN:
                if "adet" in a:
                    c.append("    c.push_back('[');")
                    c.append(f"    for (std::size_t i = 0; i < {alan}.size(); ++i) {{ if (i > 0) {{ c.push_back(','); }} {skaler(alan + '[i]')} }}")
                    c.append("    c.push_back(']');")
                else:
                    c.append(f"    {skaler(alan)}")
            elif t == "metin":
                c.append(f"    dok_metin(c, {alan});")
            elif t == "bayt_dizisi":
                c.append("    c.push_back('[');")
                c.append(f"    for (std::size_t i = 0; i < {alan}.size(); ++i) {{ if (i > 0) {{ c.push_back(','); }} dok_tam(c, {alan}[i]); }}")
                c.append("    c.push_back(']');")
            elif t == "dizi":
                c.append("    c.push_back('[');")
                c.append(f"    for (std::size_t i = 0; i < {alan}.size(); ++i) {{ if (i > 0) {{ c.push_back(','); }} dok_ic({alan}[i], c); }}")
                c.append("    c.push_back(']');")
        c.append("    c.push_back('}');")
        c.append("}")
        c.append("")

    def cozucu_yaz(self, yapi_adi, alanlar):
        # Yalnız dizi içermeyen yükler için (sunucu olay yükünü ve testler düz yükleri çözer).
        if any(a["tip"] == "dizi" for a in alanlar):
            return False
        c = self.cpp
        c.append(f"bool coz(const std::uint8_t* kaynak, std::size_t uzunluk, {yapi_adi}& y) noexcept")
        c.append("{")
        c.append("    Okuyucu r(kaynak, uzunluk);")
        for a in alanlar:
            t = a["tip"]
            alan = f"y.{ad(a['ad'])}"
            if t in TAMSAYI or t in KAYAN:
                if "adet" in a:
                    c.append(f"    for (auto& v : {alan}) {{ v = r.{t}(); }}")
                else:
                    c.append(f"    {alan} = r.{t}();")
            elif t == "metin":
                c.append(f"    {{ const auto n = r.{a['sayac']}(); const auto* p = r.baytlar(n); {alan} = std::string_view(reinterpret_cast<const char*>(p), p == nullptr ? 0u : static_cast<std::size_t>(n)); }}")
            elif t == "bayt_dizisi":
                c.append(f"    {{ const auto n = r.{a['sayac']}(); const auto* p = r.baytlar(n); {alan} = std::span<const std::uint8_t>(p, p == nullptr ? 0u : static_cast<std::size_t>(n)); }}")
        c.append("    return r.tamam() && r.kalan() == 0;")
        c.append("}")
        c.append("")
        return True

    def uret(self):
        k = self.k
        yukler = k["x_ida_yukler"]
        kanallar = k["x_ida_kanallar"]
        if int(k["x_ida_cerceve_basligi_bayt"]) != 24:
            raise SystemExit("uret_katalog: başlık 24 bayt değil")
        for y, alanlar in yukler.items():
            self.yapi_topla(y, alanlar)

        h = self.hpp
        h.append("// ÜRETİLDİ: sim/a3/arac/uret_katalog.py; elle değiştirilmez.")
        h.append("// Kaynak: docs/sozlesmeler/sema/a3-kanallar.json (A3 §3.3, §3.4).")
        h.append("#pragma once")
        h.append("")
        h.append("#include <array>")
        h.append("#include <cstddef>")
        h.append("#include <cstdint>")
        h.append("#include <limits>")
        h.append("#include <span>")
        h.append("#include <string>")
        h.append("#include <string_view>")
        h.append("")
        h.append("namespace ida::a3 {")
        h.append("")
        h.append(f"inline constexpr std::string_view kA3Surumu = \"{k['x_ida_sozlesme']['surum']}\";")
        h.append(f"inline constexpr std::string_view kWsAltProtokolu = \"{k['x_ida_sozlesme']['ws_alt_protokolu']}\";")
        h.append(f"inline constexpr std::string_view kHttpTabanYolu = \"{k['x_ida_sozlesme']['http_taban_yolu']}\";")
        h.append("")
        for yapi_adi, alanlar in self.yapilar:
            self.yapi_yaz(yapi_adi, alanlar)
        h.append("// Kanal kimlikleri (katalogdaki kanal_id).")
        h.append("enum class Kanal : std::uint16_t {")
        for kn in kanallar:
            h.append(f"    {kanal_sembolu(kn['ad'])} = {int(kn['kanal_id'])},")
        h.append("};")
        h.append("")
        h.append("// Kodlayıcılar: yükü katalogdaki sırayla, doldurmasız ve küçük uçlu yazar (A3-K22).")
        h.append("// `kodla`, kapasite yetmezse ya da bir sayaç `azami`'yi aşarsa false döner. Bellek ayırmaz.")
        for y in yukler:
            h.append(f"std::size_t boyut(const {y}& y) noexcept;")
            h.append(f"bool kodla(const {y}& y, std::uint8_t* hedef, std::size_t kapasite, std::size_t& yazilan) noexcept;")
        for yapi_adi, alanlar in self.yapilar:
            if yapi_adi not in yukler:
                h.append(f"std::size_t boyut(const {yapi_adi}& y) noexcept;")
        h.append("")
        h.append("// Sınamalar için değer dökümü (JSON; 64 bit tamsayılar dize, yok değeri null).")
        for y in yukler:
            h.append(f"std::string json_dok(const {y}& y);")
        h.append("")
        h.append("// Çözücüler (yalnız dizi içermeyen yükler): metin ve bayt alanları kaynağı gösterir.")
        cozulenler = []
        for y, alanlar in yukler.items():
            if not any(a["tip"] == "dizi" for a in alanlar):
                h.append(f"bool coz(const std::uint8_t* kaynak, std::size_t uzunluk, {y}& y) noexcept;")
                cozulenler.append(y)
        h.append("")
        h.append("// Kanal → yük eşlemesi; `anahtar` kanal-anahtarı (A3 §2) tek bir sayıya paketler.")
        h.append("template <Kanal K> struct KanalYuku;")
        for i, kn in enumerate(kanallar):
            y = kn["yuk"]
            alanlar = {a["ad"]: a for a in yukler[y]}
            parcalar = []
            kaydirma = 0
            for an in reversed(kn["anahtar"]):
                a = alanlar[an]
                bit = TAMSAYI[a["tip"]][1] * 8
                parcalar.append(f"(static_cast<std::uint64_t>(y.{ad(an)}) << {kaydirma}u)")
                kaydirma += bit
            if kaydirma > 64:
                raise SystemExit("uret_katalog: kanal anahtarı 64 bite sığmıyor")
            ifade = " | ".join(reversed(parcalar)) if parcalar else "0u"
            h.append(f"template <> struct KanalYuku<Kanal::{kanal_sembolu(kn['ad'])}> {{")
            h.append(f"    using tip = {y};")
            h.append(f"    static constexpr std::uint16_t indeks = {i};")
            h.append(f"    static constexpr bool olay = {'true' if kn['sinif'] == 'olay' else 'false'};")
            h.append(f"    static std::uint64_t anahtar([[maybe_unused]] const {y}& y) noexcept {{ return {ifade}; }}")
            h.append("};")
        h.append("")
        h.append("}  // namespace ida::a3")
        h.append("")

        c = self.cpp
        c.append("// ÜRETİLDİ: sim/a3/arac/uret_katalog.py; elle değiştirilmez.")
        c.append("#include \"ida/a3/yukler.hpp\"")
        c.append("#include \"ida/a3/katalog.hpp\"")
        c.append("#include \"kodlayici.hpp\"")
        c.append("")
        c.append("#include <iterator>")
        c.append("#include <utility>")
        c.append("")
        c.append("namespace ida::a3 {")
        c.append("")
        for yapi_adi, alanlar in self.yapilar:
            self.boyut_yaz(yapi_adi, alanlar)
        c.append("namespace {")
        c.append("")
        for adi, b in (("k_katalog", self.kb), ("k_komutlar", self.kmb)):
            c.append(f"constexpr std::uint8_t {adi}[{len(b)}] = {{")
            for i in range(0, len(b), 24):
                c.append("    " + ",".join(str(x) for x in b[i:i + 24]) + ",")
            c.append("};")
        c.append("")
        for y, alanlar in yukler.items():
            self.tablo_yaz(y, alanlar)
        c.append("constexpr YukTanimi k_yukler[] = {")
        for y in yukler:
            c.append(f"    {{\"{y}\", k_{y}_alanlar, static_cast<std::uint16_t>(std::size(k_{y}_alanlar))}},")
        c.append("};")
        yuk_indeks = {y: i for i, y in enumerate(yukler)}
        c.append("constexpr KanalTanimi k_kanallar[] = {")
        for i, kn in enumerate(kanallar):
            an = kn["anahtar"]
            if len(an) > 2:
                raise SystemExit("uret_katalog: ikiden çok anahtar alanı")
            a1 = f"\"{an[0]}\"" if len(an) > 0 else "\"\""
            a2 = f"\"{an[1]}\"" if len(an) > 1 else "\"\""
            c.append(
                "    {"
                f"\"{kn['ad']}\", {int(kn['kanal_id'])}, {int(kn['kanal_surumu'])}, &k_yukler[{yuk_indeks[kn['yuk']]}], "
                f"Sinif::{kn['sinif']}, {int(kn['azami_hz'] or 0)}, {int(kn['varsayilan_hz'] or 0)}, "
                f"{int(kn['kuyruk_siniri'] or 0)}u, {'true' if kn['zorunlu_abonelik'] else 'false'}, "
                f"{'true' if kn['yer_gercegi'] else 'false'}, {{{a1}, {a2}}}, {len(an)}, "
                f"\"{kn['zaman_anlami']}\", {i}"
                "},"
            )
        c.append("};")
        c.append("constexpr OlayTuru k_olay_turleri[] = {")
        for o in k["x_ida_olay_turleri"]:
            ref = o["ayrinti"]
            if not ref.startswith("#/$defs/"):
                raise SystemExit("uret_katalog: olay ayrıntı başvurusu beklenmeyen biçimde")
            c.append(f"    {{{int(o['kod'])}, \"{o['ad']}\", \"{o['varsayilan_onem']}\", \"{ref[len('#/$defs/'):]}\"}},")
        c.append("};")
        c.append("constexpr HataKodu k_hata_kodlari[] = {")
        for e in k["x_ida_hata_kodlari"]:
            c.append(f"    {{\"{e['kod']}\", {int(e['http'])}}},")
        c.append("};")
        c.append("constexpr UcTanimi k_uclar[] = {")
        for u in k["x_ida_uclar"]:
            istek = u["istek"] or ""
            if istek and not istek.startswith("#/$defs/"):
                raise SystemExit("uret_katalog: uç isteği beklenmeyen biçimde")
            istek = istek[len("#/$defs/"):] if istek else ""
            c.append(
                f"    {{\"{u['yontem']}\", \"{u['yol']}\", \"{istek}\", {'true' if u['tur'] == 'asenkron' else 'false'}, "
                f"{'true' if u['durum_degistirir'] else 'false'}}},"
            )
        c.append("};")
        c.append("constexpr std::pair<std::uint32_t, std::string_view> k_gecerlilik[] = {")
        for bit, adi in k["x_ida_gecerlilik_nedenleri"].items():
            c.append(f"    {{{int(bit)}u, \"{adi}\"}},")
        c.append("};")
        c.append("")
        for yapi_adi, alanlar in self.yapilar:
            self.kodlayici_yaz(yapi_adi, alanlar, yapi_adi in yukler)
        for yapi_adi, alanlar in self.yapilar:
            self.dok_yaz(yapi_adi, alanlar)
        c.append("}  // namespace")
        c.append("")
        for y in yukler:
            self.kodla_sarmalayici_yaz(y)
            c.append(f"std::string json_dok(const {y}& y)")
            c.append("{")
            c.append("    std::string c;")
            c.append("    dok_ic(y, c);")
            c.append("    return c;")
            c.append("}")
            c.append("")
        for y, alanlar in yukler.items():
            self.cozucu_yaz(y, alanlar)
        c.append("std::span<const std::uint8_t> katalog_baytlari() noexcept { return k_katalog; }")
        c.append("std::span<const std::uint8_t> komut_katalogu_baytlari() noexcept { return k_komutlar; }")
        c.append("std::span<const YukTanimi> yukler() noexcept { return k_yukler; }")
        c.append("std::span<const KanalTanimi> kanallar() noexcept { return k_kanallar; }")
        c.append("std::span<const OlayTuru> olay_turleri() noexcept { return k_olay_turleri; }")
        c.append("std::span<const HataKodu> hata_kodlari() noexcept { return k_hata_kodlari; }")
        c.append("std::span<const UcTanimi> uclar() noexcept { return k_uclar; }")
        c.append("std::span<const std::pair<std::uint32_t, std::string_view>> gecerlilik_nedenleri() noexcept { return k_gecerlilik; }")
        c.append("")
        c.append("}  // namespace ida::a3")
        c.append("")
        return "\n".join(self.hpp), "\n".join(self.cpp)


def main():
    if len(sys.argv) != 4:
        raise SystemExit("kullanım: uret_katalog.py <a3-kanallar.json> <komutlar.json> <cikti_dizini>")
    katalog_yolu, komutlar_yolu, cikti = map(Path, sys.argv[1:])
    kb = katalog_yolu.read_bytes()
    kmb = komutlar_yolu.read_bytes()
    katalog = json.loads(kb.decode("utf-8"))
    json.loads(kmb.decode("utf-8"))  # biçim denetimi
    hpp, cpp = Uretec(katalog, kmb, kb).uret()
    (cikti / "include" / "ida" / "a3").mkdir(parents=True, exist_ok=True)
    for yol, metin in ((cikti / "include" / "ida" / "a3" / "yukler.hpp", hpp), (cikti / "yukler.cpp", cpp)):
        yol.write_text(metin, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
