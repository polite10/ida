#!/usr/bin/env python3
"""firmware/araclar/sahte_sim.py — kartın karşısında koşan sahte sim (W3-kart dilim 2, md. 6).

Gerçek sim sunucusunun köprüsü yokken kartın A1 oturumunu, partilerini ve komut yolunu sınar.
Mesajlar protocol/ida.xml'den kodlanır (MAVLink 2 alan sırası: boya göre azalan, kararlı);
CRC_EXTRA değerleri A1 Ek B'den (protocol/ida_link/veri/altin.json). Fizik yoktur: araç
durgundur (IMU: sıfır açısal hız, özgül kuvvet (0, 0, −9,81) m/s²; basınç yüzeyde; sabit
manyetik alan; batarya dolu; sızıntı yok; emniyet girdileri kapalı).

Kipler:
  oturum   el sıkışma; --sure saniye boyunca 200 Hz parti (TF + IMU + BARO + MAG her partide,
           BATT ve SAFETY 10 Hz, LEAK 1 Hz); --komut ile ARM, SET_MODE(GUIDED), GUIDED_SP
           (derinlik 3 m, yön 0), DISARM; sonunda SESSION_CLOSE(RUN_END) ve kartın IDLE'a dönüşü.
  tbag     el sıkışma, 3 s parti, sonra gönderim durur: ızgara çıktıları (ASYNC, IS_SAFE) ve
           T_bağ sonunda kartın oturumu bitirmesi; ardından yeni el sıkışma.
  param    PARAM_SET (T_imu_stale = 25) → COMMIT → SAVE; kaydın bitmesi (STATUS.param_store);
           yeni el sıkışmada REBOOT; kart yeniden açılınca boot_count, param_gen, param_blob_crc ve
           PARAM_GET ile değer.
  yeniden  oturum sürerken port kapatılır (kablo çekilmeden program kapanışı), 1 s sonra açılır:
           yeni el sıkışma kurulur.

Yalnız Python standart kitaplığı; seri port duman.py'nin Seri sınıfıyla. Çıktı: özet ve --json.
"""
import argparse
import gc
import json
import os
import random
import select
import struct
import sys
import time
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import duman  # noqa: E402
import kabul  # noqa: E402
import olcum_oku  # noqa: E402

KOK = duman.KOK
XML = os.path.join(KOK, "protocol", "ida.xml")
P_NS = 5000000
DIALECT_HASH = 0x3939788984A5FF80

TIP = {"uint8_t": ("B", 1), "int8_t": ("b", 1), "uint16_t": ("H", 2), "int16_t": ("h", 2),
       "uint32_t": ("I", 4), "int32_t": ("i", 4), "float": ("f", 4), "uint64_t": ("Q", 8),
       "int64_t": ("q", 8), "char": ("c", 1)}


class Mesaj:
    def __init__(self, kimlik, ad, alanlar, extra):
        self.kimlik = kimlik
        self.ad = ad
        self.extra = extra
        # MAVLink 2 tel sırası: taban tip boyuna göre azalan, eşitlikte tanım sırası.
        self.alanlar = sorted(alanlar, key=lambda a: -TIP[a[1]][1])
        self.bicim = "<" + "".join((str(n) if n > 1 else "") + TIP[t][0] for _, t, n in self.alanlar)
        self.boy = struct.calcsize(self.bicim)

    def kodla(self, **d):
        degerler = []
        for ad, t, n in self.alanlar:
            v = d.get(ad, 0)
            if n > 1:
                v = list(v) if isinstance(v, (list, tuple)) else [v] * n
                v = (v + [0] * n)[:n]
                degerler.extend(float(x) if t == "float" else int(x) for x in v)
            else:
                degerler.append(float(v) if t == "float" else int(v))
        b = struct.pack(self.bicim, *degerler)
        n = len(b)
        while n > 1 and b[n - 1] == 0:  # A1-K05: sondaki sıfırlar kırpılır
            n -= 1
        return b[:n]

    def coz(self, yuk):
        yuk = bytes(yuk) + bytes(max(0, self.boy - len(yuk)))
        v = struct.unpack(self.bicim, yuk[:self.boy])
        d = {}
        i = 0
        for ad, _t, n in self.alanlar:
            if n > 1:
                d[ad] = list(v[i:i + n])
                i += n
            else:
                d[ad] = v[i]
                i += 1
        return d


def dialect_oku():
    bilgi, _ = duman.tablo_oku()
    kok = ET.parse(XML).getroot()
    mesajlar = {}
    for m in kok.find("messages"):
        alanlar = []
        for f in m.findall("field"):
            t = f.get("type")
            n = 1
            if "[" in t:
                t, n = t[:-1].split("[")
                n = int(n)
            alanlar.append((f.get("name"), t, n))
        k = int(m.get("id"))
        mesajlar[m.get("name")] = Mesaj(k, m.get("name"), alanlar, bilgi[k][0])
    return mesajlar


M = dialect_oku()
KIMLIK = {m.kimlik: m for m in M.values()}


class Sim:
    def __init__(self, port):
        self.s = duman.Seri(port, duman.BAUD)
        bilgi, _ = duman.tablo_oku()
        self.ayr = duman.Ayristirici(bilgi)
        self.seq = 0
        self.gelen = []   # (t, ad, alanlar)
        self.cikis = bytearray()
        self.bos_al(0.5)  # A1-K12: açılışta eski baytlar atılır

    def kapat(self):
        self.s.kapat()

    def bos_al(self, sure):
        t = time.monotonic() + sure
        while time.monotonic() < t:
            select.select([self.s.fd], [], [], 0.05)
            self.s.oku()

    def gonder(self, ad, **alan):
        m = M[ad]
        yuk = m.kodla(**alan)
        bas = bytes([0xFD, len(yuk), 0, 0, self.seq & 0xFF, 250, 25,
                     m.kimlik & 0xFF, (m.kimlik >> 8) & 0xFF, (m.kimlik >> 16) & 0xFF])
        crc = duman.x25(bytes([m.extra]), duman.x25(yuk, duman.x25(bas[1:])))
        self.cikis += bas + yuk + bytes([crc & 0xFF, crc >> 8])
        self.seq = (self.seq + 1) & 0xFF

    def yaz(self):
        while self.cikis:
            n = self.s.yaz(bytes(self.cikis[:4096]))
            del self.cikis[:n]
            if not n:
                select.select([], [self.s.fd], [], 0.002)

    def oku(self, bekle):
        r, _, _ = select.select([self.s.fd], [], [], bekle)
        yeni = []
        if r:
            veri = self.s.oku()
            t = time.monotonic()
            for msgid, _sq, yuk, tt in self.ayr.besle(veri, t):
                m = KIMLIK.get(msgid)
                if m is not None:
                    yeni.append((tt, m.ad, m.coz(yuk)))
        self.gelen.extend(yeni)
        return yeni

    def bekle_mesaj(self, kosul, sure):
        t_son = time.monotonic() + sure
        while time.monotonic() < t_son:
            for g in self.oku(0.01):
                if kosul(g):
                    return g
        return None


def el_sikis(sim, sonuc=None, sess=None):
    """SIM_HELLO'yu T_hello ile yineler; eşleşen BOARD_HELLO'ya ACCEPT. Dönüş: (sess, hello)."""
    nonce = random.randint(1, 0xFFFFFFFF)
    t_son = time.monotonic() + 5.0
    hello = None
    while time.monotonic() < t_son and hello is None:
        sim.gonder("IDA_SIM_HELLO", dialect_hash=DIALECT_HASH, nonce=nonce, run_id=7,
                   ctrl_period_ns=P_NS, proto_major=1, proto_minor=0)
        sim.yaz()
        g = sim.bekle_mesaj(lambda g: g[1] == "IDA_BOARD_HELLO" and g[2]["peer_nonce"] == nonce, 0.1)
        if g is not None:
            hello = g[2]
    if hello is None:
        return None, None
    if sonuc is None:
        sonuc = 0  # OK
    if sess is None:
        sess = random.randint(1, 0xFFFF)
        while sess == hello["prev_sess"]:
            sess = random.randint(1, 0xFFFF)
    sim.gonder("IDA_SESSION_ACCEPT", run_id=7, nonce_sim=nonce, nonce_board=hello["nonce"],
               sess=sess, result=sonuc, run_mode=2)
    sim.yaz()
    return sess, hello


class Parti:
    def __init__(self, sim, sess):
        self.sim = sim
        self.sess = sess
        self.n = 0
        self.t0 = 1000000000
        self.seq = {"imu": 0, "baro": 0, "mag": 0, "batt": 0, "leak": 0, "emn": 0}
        self.tf_zaman = {}
        self.ek_uye = 0  # pencerede gönderilen komutlar da partinin üyesidir (A1-K28, K30)
        self.ara_en_buyuk = 0.0
        self.takilma = []  # önceki partiden 2 periyottan uzun sonra gönderilen partiler

    def gonder(self):
        s = self.sim
        t = self.t0 + self.n * P_NS
        uye = 0
        s.gonder("IDA_IMU", t_meas_ns=t, seq=self.seq["imu"], window_ns=P_NS, gyro=[0, 0, 0],
                 accel=[0.0, 0.0, -9.81], temp_c=20.0, flags=1, inst=0)
        self.seq["imu"] += 1
        uye += 1
        s.gonder("IDA_BARO", t_meas_ns=t, seq=self.seq["baro"], window_ns=P_NS, p_abs=101325.0,
                 temp_c=15.0, flags=1, inst=0)
        self.seq["baro"] += 1
        uye += 1
        s.gonder("IDA_MAG", t_meas_ns=t, seq=self.seq["mag"], field=[2.0e-5, 0.0, 4.0e-5], flags=1,
                 inst=0)
        self.seq["mag"] += 1
        uye += 1
        if self.n % 20 == 0:
            s.gonder("IDA_BATT", t_meas_ns=t, seq=self.seq["batt"], window_ns=P_NS * 20, voltage=16.4,
                     current=1.0, temp_c=25.0, flags=1, inst=0)
            self.seq["batt"] += 1
            s.gonder("IDA_SAFETY", t_meas_ns=t, seq=self.seq["emn"], flags=1, inst=0, estop=0,
                     tube=[1 | 2 | 4, 1 | 2 | 4], safety_rev=1)
            self.seq["emn"] += 1
            uye += 2
        if self.n % 200 == 0:
            s.gonder("IDA_LEAK", t_meas_ns=t, seq=self.seq["leak"], flags=1, inst=0, mask=0)
            self.seq["leak"] += 1
            uye += 1
        uye += self.ek_uye
        self.ek_uye = 0
        s.gonder("IDA_TIME_FRAME", t_sim_ns=t, batch_seq=self.n, sess=self.sess, n_members=uye,
                 tf_flags=1, run_mode=2)
        s.yaz()
        simdi = time.monotonic()
        self.tf_zaman[self.n] = simdi
        if self.n > 0:
            # PC'nin iki parti arasındaki aralığı: takılma (ör. işletim sistemi zamanlaması)
            # partilerin art arda gitmesine ve kartta birleşmesine (A4-K102) yol açar.
            ara = simdi - self.tf_zaman.get(self.n - 1, simdi)
            if ara > self.ara_en_buyuk:
                self.ara_en_buyuk = ara
            if ara > 2.0 * P_NS / 1e9:
                self.takilma.append(self.n)
        self.n += 1


def kos_parti(sim, parti, sure, komutlar=None, durdur_sonra=None):
    """sure saniye 200 Hz parti; komutlar: [(t_s, ad, alanlar)]. Gelen mesajlar sim.gelen'de."""
    bas = time.monotonic()
    sonraki = bas
    komutlar = sorted(komutlar or [], key=lambda k: k[0])
    ki = 0
    while True:
        simdi = time.monotonic()
        if simdi - bas >= sure:
            break
        if ki < len(komutlar) and simdi - bas >= komutlar[ki][0]:
            _, ad, alan = komutlar[ki]
            alan = dict(alan)
            alan.setdefault("sess", parti.sess)
            sim.gonder(ad, **alan)
            parti.ek_uye += 1
            ki += 1
        if simdi >= sonraki:
            parti.gonder()
            sonraki += P_NS / 1e9
            if simdi - sonraki > 0.05:
                sonraki = simdi  # PC geride kaldı: atlanmaz, sıradaki parti hemen
        sim.oku(max(0.0, min(sonraki - time.monotonic(), 0.005)))


def aktuator_analiz(sim, parti, t_bas):
    act = [g for g in sim.gelen if g[1] == "IDA_ACTUATOR" and g[0] >= t_bas]
    say = {}
    birlesik = set()  # n_merged ile en yeni partinin çağrısına katılan partiler (A1-K45, A4-K102)
    rtt = []
    flags = 0
    asenk = 0
    for t, _, a in act:
        if a["act_flags"] & 16:  # ASYNC (ızgara tick'inin güvenli çıktısı; partiye ait değil)
            asenk += 1
            continue
        say[a["batch_seq"]] = say.get(a["batch_seq"], 0) + 1
        for k in range(1, a["n_merged"] + 1):
            birlesik.add(a["batch_seq"] - k)
        tz = parti.tf_zaman.get(a["batch_seq"])
        if tz is not None:
            rtt.append((t - tz) * 1000.0)
        flags |= a["act_flags"]
    gonderilen = list(range(parti.n))
    yanitsiz = [n for n in gonderilen if say.get(n, 0) == 0]
    eksik = [n for n in yanitsiz if n not in birlesik]
    birlesen = [n for n in yanitsiz if n in birlesik]
    # Birleşen her partinin ardından gelen (onu kapsayan) parti PC'de takılmadan sonra mı gitti?
    tz = parti.tf_zaman
    pc_kaynakli = all(m + 1 in tz and tz[m + 1] - tz[m] < 0.002 for m in birlesen)
    yinelenen = [n for n, k in say.items() if k > 1]
    rtt.sort()

    def yuzde(p):
        return rtt[min(len(rtt) - 1, int(p * len(rtt)))] if rtt else None
    rx_to_tx = sorted(a["rx_to_tx_us"] for _, _, a in act if not a["act_flags"] & 16)
    return {
        "parti": parti.n, "aktuator": len(act) - asenk, "asenk_aktuator": asenk, "eksik": len(eksik), "yinelenen": len(yinelenen),
        "ilk_eksikler": eksik[:10],
        "birlesen_parti": len(birlesen), "ilk_birlesenler": birlesen[:10],
        "birlesmeler_pc_art_arda_gonderiminden": pc_kaynakli,
        "pc_parti_araligi_en_buyuk_ms": parti.ara_en_buyuk * 1000.0,
        "pc_takilma_2periyot_ustu": len(parti.takilma),
        "pc_rtt_ms_p50": yuzde(0.5), "pc_rtt_ms_p99": yuzde(0.99), "pc_rtt_ms_max": rtt[-1] if rtt else None,
        "pc_rtt_10ms_ustu": sum(1 for x in rtt if x > 10.0),
        "kart_rx_to_tx_us_max": rx_to_tx[-1] if rx_to_tx else None,
        "kart_rx_to_tx_us_p50": rx_to_tx[len(rx_to_tx) // 2] if rx_to_tx else None,
        "act_flags_birlesim": flags,
    }


def son_mesaj(sim, ad, t_bas=0.0):
    for g in reversed(sim.gelen):
        if g[1] == ad and g[0] >= t_bas:
            return g[2]
    return None


def link_stats(sim, t_bas=0.0):
    m = son_mesaj(sim, "IDA_LINK_STATS", t_bas)
    return dict(zip(duman.LINK_STATS_ADLARI, m["cnt"])) if m else None


def link_stats_ilk(sim, t_bas=0.0):
    for g in sim.gelen:
        if g[1] == "IDA_LINK_STATS" and g[0] >= t_bas:
            return dict(zip(duman.LINK_STATS_ADLARI, g[2]["cnt"]))
    return None


HATA_ADLARI = ("rx_crc_err", "rx_unknown_msgid", "rx_iflag", "rx_bad_len", "rx_wrong_sys",
               "rx_overflow", "rx_stale_session", "batch_incomplete", "batch_skipped",
               "batch_old", "batch_dup", "batch_merged", "member_overflow", "grid_missed",
               "tx_dropped", "tx_crit_stall")
# Ölçüm bloğundaki karşılıkları (olcum.h say_*; tx_* gonderim sayaçları).
OLCUM_ADI = {"rx_stale_session": "say_stale_session", "tx_dropped": "gon_tx_dropped",
             "tx_crit_stall": "gon_crit_stall"}


def olcum_fark(once, sonra):
    f = {}
    for a in HATA_ADLARI:
        k = OLCUM_ADI.get(a, "say_" + a)
        f[a] = (sonra[k] - once[k]) & 0xFFFFFFFF
    for k in ("izgara_kacirma", "step_butce_asimi", "tick_butce_asimi", "besleme_ret", "ore", "fe",
              "ne", "rx_tasma", "rx_belirsiz", "say_batch_closed", "say_rx_frames", "gon_zorunlu_ret",
              "gon_kritik_ret"):
        f[k] = (sonra[k] - once[k]) & 0xFFFFFFFF
    return f


def oturum_kipi(arg, sim, ozet):
    once = olcum_oku.ozet_oku(arg.olcum) if arg.olcum else None
    sess, hello = el_sikis(sim)
    if sess is None:
        ozet["sonuc"] = "EL_SIKISMA_YOK"
        return False
    ozet["board_hello"] = hello
    t_bas = time.monotonic()
    parti = Parti(sim, sess)
    komutlar = []
    if arg.komut:
        # A4-K48: yineleme penceresi (son 32 komut) açılıştan beri sürer; kimlikler kartın
        # duyurduğu last_cmd_id'nin üstünden başlar.
        b = hello["last_cmd_id"]
        komutlar = [
            (2.0, "IDA_CMD_SIMPLE", {"cmd_id": b + 1, "type": 2, "source": 255}),            # ARM
            (3.0, "IDA_CMD_SIMPLE", {"cmd_id": b + 2, "type": 4, "arg": 6, "source": 255}),  # GUIDED
            (4.0, "IDA_CMD_GUIDED", {"cmd_id": b + 3, "mask": 2 | 32, "depth": 3.0, "yaw": 0.0,
                                     "source": 255}),                                  # GUIDED_SP
            (8.0, "IDA_CMD_SIMPLE", {"cmd_id": b + 4, "type": 3, "source": 255}),            # DISARM
        ]
    kos_parti(sim, parti, arg.sure, komutlar)
    ls_once = link_stats(sim, t_bas)
    # Kalan yanıtlar
    t_son = time.monotonic()
    while time.monotonic() - t_son < 1.2:
        sim.oku(0.02)
    ozet["aktuator"] = aktuator_analiz(sim, parti, t_bas)
    ozet["komut_sonuclari"] = [g[2] for g in sim.gelen if g[1] == "IDA_CMD_RESULT" and g[0] >= t_bas]
    ozet["status"] = son_mesaj(sim, "IDA_STATUS", t_bas)
    ozet["link_stats"] = link_stats(sim, t_bas) or ls_once
    ozet["call_trace"] = sum(1 for g in sim.gelen if g[1] == "IDA_CALL_TRACE" and g[0] >= t_bas)
    ozet["kart_session_close"] = [g[2] for g in sim.gelen if g[1] == "IDA_SESSION_CLOSE" and g[0] >= t_bas]
    # Oturum sonu (RUN_END) ve IDLE duyurusu.
    sim.gonder("IDA_SESSION_CLOSE", sess=sess, reason=5)
    sim.yaz()
    g = sim.bekle_mesaj(lambda g: g[1] == "IDA_BOARD_HELLO" and g[2]["peer_nonce"] == 0, 2.0)
    ozet["kapanis_sonrasi_duyuru"] = g[2] if g else None
    a = ozet["aktuator"]
    if once is not None:
        time.sleep(1.2)
        sonra = olcum_oku.ozet_oku(arg.olcum)
        fark = olcum_fark(once, sonra)
        ozet["olcum_fark"] = fark
        ozet["olcum_sonra"] = olcum_oku.ozet(sonra)
        hata = {k: fark[k] for k in HATA_ADLARI}
        ek = fark["izgara_kacirma"] + fark["besleme_ret"] + fark["ore"] + fark["fe"] + fark["rx_tasma"]
        partiler = fark["say_batch_closed"] == parti.n
    else:
        # Yalnız oturumdaki ilk ve son LINK_STATS arası (ilk ≈ 1 s görülmez).
        l0 = link_stats_ilk(sim, t_bas) or {}
        l1 = ozet["link_stats"] or {}
        hata = {k: (l1.get(k, 0) - l0.get(k, 0)) & 0xFFFFFFFF for k in HATA_ADLARI}
        ek = 0
        partiler = True
    ozet["kart_hata"] = hata
    temel = (a["eksik"] == 0 and a["yinelenen"] == 0 and ek == 0 and partiler and
             not ozet["kart_session_close"] and g is not None)
    gecti = temel and all(v == 0 for v in hata.values()) and a["birlesen_parti"] == 0
    # Birleşme yalnız PC'nin takıldığı yerdeyse kartın sözleşmedeki davranışıdır (A4-K102):
    # ayrı sonuç olarak verilir; kabul için koşu PC sakinken yinelenir.
    birlesme_pc = (temel and a["birlesmeler_pc_art_arda_gonderiminden"] and
                   hata["batch_merged"] == a["birlesen_parti"] and
                   all(v == 0 for k, v in hata.items() if k not in ("batch_merged", "member_overflow")))
    ozet["sonuc"] = "GECTI" if gecti else ("GECTI_PC_TAKILMASI_BIRLESME" if birlesme_pc else "GECMEDI")
    return gecti


def tbag_kipi(arg, sim, ozet):
    sess, _ = el_sikis(sim)
    if sess is None:
        ozet["sonuc"] = "EL_SIKISMA_YOK"
        return False
    parti = Parti(sim, sess)
    kos_parti(sim, parti, 3.0)
    t_dur = time.monotonic()
    # Gönderim durdu: kartın çıktıları ve oturum sonu.
    # t_dur: son partinin (TF) yazıldığı an; kartın T_bağ'ı son geçerli çerçeveden sayar.
    t_dur = parti.tf_zaman.get(parti.n - 1, t_dur)
    g = sim.bekle_mesaj(lambda g: g[1] == "IDA_BOARD_HELLO" and g[2]["peer_nonce"] == 0, 3.0)
    t_idle = g[0] - t_dur if g else None
    asenk = [(x[0] - t_dur, x[2]["act_flags"], list(x[2]["pwm_us"]), x[2]["batch_seq"])
             for x in sim.gelen if x[0] >= t_dur and x[1] == "IDA_ACTUATOR" and x[2]["act_flags"] & 16]
    sess2, _ = el_sikis(sim)
    if sess2 is not None:
        sim.gonder("IDA_SESSION_CLOSE", sess=sess2, reason=5)
        sim.yaz()
    gecti, ayrinti = kabul.tbag_degerlendir(asenk, t_idle, parti.n - 1, sess2 is not None)
    ayrinti["asenk_son_s"] = asenk[-1][0] if asenk else None
    ozet["tbag"] = ayrinti
    ozet["sonuc"] = "GECTI" if gecti else "GECMEDI"
    return gecti


def komut_bekle(sim, cmd_id, sure=2.0):
    g = sim.bekle_mesaj(lambda g: g[1] == "IDA_CMD_RESULT" and g[2]["cmd_id"] == cmd_id, sure)
    return g[2] if g else None


def param_kipi(arg, sim, ozet):
    sess, hello = el_sikis(sim)
    if sess is None:
        ozet["sonuc"] = "EL_SIKISMA_YOK"
        return False
    ozet["once_board_hello"] = hello
    parti = Parti(sim, sess)
    deger = arg.deger
    sonuclar = {}

    def komut_partide(cmd_id, ad, **alan):
        alan.setdefault("sess", sess)
        sim.gonder(ad, cmd_id=cmd_id, source=250, **alan)
        parti.ek_uye += 1
        t = time.monotonic()
        while time.monotonic() - t < 1.0:
            parti.gonder()
            for g in sim.oku(0.005):
                if g[1] == "IDA_CMD_RESULT" and g[2]["cmd_id"] == cmd_id:
                    return g[2]
        return None

    kos_parti(sim, parti, 0.5)
    b = hello["last_cmd_id"]  # A4-K48: kimlikler açılıştan beri görülenlerin üstünden
    sonuclar["set"] = komut_partide(b + 1, "IDA_CMD_PARAM_SET", param_id=0x0101, param_type=2,
                                    value_bits=deger)
    sonuclar["commit"] = komut_partide(b + 2, "IDA_CMD_SIMPLE", type=13)
    sonuclar["save"] = komut_partide(b + 3, "IDA_CMD_SIMPLE", type=14)
    # Kayıt bitene kadar parti (STATUS.param_store: 1 BUSY, 2 OK, 3 FAIL)
    t = time.monotonic()
    durum = None
    while time.monotonic() - t < 3.0:
        kos_parti(sim, parti, 0.1)
        st = son_mesaj(sim, "IDA_STATUS")
        if st and st["param_store"] in (2, 3):
            durum = st
            break
    ozet["komutlar"] = sonuclar
    ozet["kayit_status"] = durum
    sim.gonder("IDA_SESSION_CLOSE", sess=sess, reason=5)
    sim.yaz()
    sim.bekle_mesaj(lambda g: g[1] == "IDA_BOARD_HELLO" and g[2]["peer_nonce"] == 0, 2.0)
    if arg.olcum and not arg.kayit_iwdg:
        # Kayıt süreleri yeniden başlatmada sıfırlanır: oturum kapandıktan sonra, REBOOT'tan
        # önce oku (SWD okuması partileri durdurur; oturum açıkken okunmaz).
        time.sleep(1.2)  # ölçüm bloğu 1 Hz'te güncellenir
        o = olcum_oku.ozet(olcum_oku.ozet_oku(arg.olcum))
        ozet["kayit_olcum"] = {k: o[k] for k in ("kalici", "flash", "dongu", "izgara", "step",
                                                  "skart", "cagri", "gonderim")}
    if arg.kayit_iwdg:
        ozet["sonuc"] = "KAYIT_IWDG_SINAMASI"
        return True
    # REBOOT (A1-K17/K18: sessions_since_boot > 0)
    _, h2 = el_sikis(sim, sonuc=1)
    g = sim.bekle_mesaj(lambda g: g[1] == "IDA_BOARD_HELLO" and g[2]["peer_nonce"] == 0 and
                        g[2]["sessions_since_boot"] == 0, 5.0)
    yeni = g[2] if g else None
    ozet["reboot_sonrasi_duyuru"] = yeni
    sess3, h3 = el_sikis(sim)
    get = None
    if sess3 is not None:
        p3 = Parti(sim, sess3)
        kos_parti(sim, p3, 0.3)
        sim.gonder("IDA_CMD_SIMPLE", cmd_id=b + 4, sess=sess3, type=12, arg=0x0101, source=250)
        p3.ek_uye += 1
        t = time.monotonic()
        while time.monotonic() - t < 1.0 and get is None:
            p3.gonder()
            for x in sim.oku(0.005):
                if x[1] == "IDA_CMD_RESULT" and x[2]["cmd_id"] == b + 4:
                    get = x[2]
        sim.gonder("IDA_SESSION_CLOSE", sess=sess3, reason=5)
        sim.yaz()
    ozet["param_get"] = get
    sv = sonuclar.get("save") or {}
    gecti = (yeni is not None and hello is not None and yeni["boot_count"] > hello["boot_count"] and
             yeni["reset_cause"] == 3 and sv.get("ack") == 1 and
             yeni["param_gen"] == sv.get("save_gen") and yeni["param_blob_crc"] == sv.get("save_crc") and
             get is not None and get["value_bits"] == deger and durum is not None and
             durum["param_store"] == 2)
    ozet["sonuc"] = "GECTI" if gecti else "GECMEDI"
    return gecti


def yeniden_kipi(arg, sim, ozet, port):
    sess, _ = el_sikis(sim)
    if sess is None:
        ozet["sonuc"] = "EL_SIKISMA_YOK"
        return False
    parti = Parti(sim, sess)
    kos_parti(sim, parti, 2.0)
    sim.kapat()  # program kapanır; SESSION_CLOSE gönderilmez
    time.sleep(1.0)
    sim2 = Sim(port)
    sess2, h2 = el_sikis(sim2)
    ok = sess2 is not None
    if ok:
        p2 = Parti(sim2, sess2)
        t_bas = time.monotonic()
        kos_parti(sim2, p2, 2.0)
        t = time.monotonic()
        while time.monotonic() - t < 0.5:
            sim2.oku(0.02)
        ozet["ikinci_oturum"] = aktuator_analiz(sim2, p2, t_bas)
        ozet["ikinci_hello"] = h2
        sim2.gonder("IDA_SESSION_CLOSE", sess=sess2, reason=5)
        sim2.yaz()
        ok = ozet["ikinci_oturum"]["eksik"] == 0
    sim2.kapat()
    ozet["sonuc"] = "GECTI" if ok else "GECMEDI"
    return ok


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("kip", choices=["oturum", "tbag", "param", "yeniden"])
    p.add_argument("--sure", type=float, default=10.0)
    p.add_argument("--komut", action="store_true")
    p.add_argument("--deger", type=int, default=25)
    p.add_argument("--kayit-iwdg", action="store_true", dest="kayit_iwdg",
                   help="param: kayıttan sonra yeniden başlatma isteme (IWDG sınaması dışarıdan)")
    p.add_argument("--port", default=None)
    p.add_argument("--json", default=None)
    p.add_argument("--olcum", default=None, metavar="ONAYAR",
                   help="oturum: önce/sonra ölçüm bloğunu SWD ile oku; param: kayıttan sonra, "
                        "REBOOT'tan önce oku (ör. fw-release)")
    arg = p.parse_args(argv)
    port = arg.port or os.environ.get("IDA_SERI_UC") or ""
    if not port:
        c = sorted(x for x in os.listdir("/dev") if x.startswith("cu.usbmodem"))
        port = "/dev/" + c[0] if c else ""
    random.seed()
    # Gelen mesaj listesi 600 s'de yüz binlerce sözlüğe ulaşır; Python'un çevrim toplayıcısı
    # bunu tararken onlarca ms takılır (200 Hz partiler art arda gider, kartta birleşir).
    # Araç çevrim üretmez: toplayıcı kapatılır.
    gc.disable()
    sim = Sim(port)
    ozet = {"kip": arg.kip, "port": port}
    if arg.kip == "oturum":
        oturum_kipi(arg, sim, ozet)
    elif arg.kip == "tbag":
        tbag_kipi(arg, sim, ozet)
    elif arg.kip == "param":
        param_kipi(arg, sim, ozet)
    else:
        yeniden_kipi(arg, sim, ozet, port)
        sim = None
    if sim is not None:
        ozet["pc_crc_hata"] = sim.ayr.crc_hata
        ozet["pc_atlanan_bayt"] = sim.ayr.atlanan
        sim.kapat()
    if arg.json:
        with open(arg.json, "w", encoding="utf-8") as f:
            json.dump(ozet, f, indent=2, ensure_ascii=False, default=str)
    print(json.dumps(ozet, indent=2, ensure_ascii=False, default=str))
    return 0 if ozet.get("sonuc") in ("GECTI", "KAYIT_IWDG_SINAMASI") else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
