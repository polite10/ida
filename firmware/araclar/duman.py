#!/usr/bin/env python3
"""firmware/araclar/duman.py — kart yazılımının PC yanı duman aracı (W3-kart dilim 1, md. 7).

İki iş yapar:
  izle  Kartın A1 yayınını (IDA_BOARD_HELLO duyurusu, IDA_LINK_STATS) okur, çerçeveleri
        sağlamasıyla doğrular ve çözer.
  yuk   Ayrıca A1 Ek B altın çerçevelerinden (protocol/ida_link/veri/altin.json, sim -> kart
        yönündekiler) üretilen geçerli çerçeveleri belirli yükte gönderir: her 5 ms'lik
        periyotta ortalama --bayt bayt. Çerçevelerin yükü altın çerçeveninkidir; başlık seq'i
        her çerçevede bir artar (A1-K08) ve sağlama yeniden hesaplanır. Sonda kartın
        IDA_LINK_STATS sayaçlarının koşu boyunca artışını gönderilenle karşılaştırır.

Yalnız Python standart kitaplığı (termios; macOS'ta standart dışı hız IOSSIOSPEED ile, Linux'ta
termios.B3000000). Windows desteklenmez. Port ham kipte açılır; DTR/RTS'ye davranış bağlanmaz
(A1-K01). Çıktı: insan okunur özet ve --json ile JSON dosyası.

--olcum <ön ayar> verilirse yükten önce ve sonra kartın ölçüm bloğu SWD ile okunur
(olcum_oku.py) ve 200 Hz ızgara kaçırması ile sürücü hata sayaçlarının artışı da geçme
ölçütüne girer (A1'in grid_missed sayacı yalnız 10 ms ızgarasını sayar).

Kullanım:
  python3 -I firmware/araclar/duman.py izle --sure 5
  python3 -I firmware/araclar/duman.py yuk --sure 600 --bayt 900 --olcum fw-release --json sonuc.json
"""
import argparse
import errno
import fcntl
import json
import os
import select
import struct
import sys
import termios
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import olcum_oku  # noqa: E402

KOK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ALTIN = os.path.join(KOK, "protocol", "ida_link", "veri", "altin.json")

BAUD = 3000000
PERIYOT_S = 0.005
SIM_SYSID, SIM_COMPID = 250, 25
KART_SYSID, KART_COMPID = 1, 1
MSG_BOARD_HELLO = 46001
MSG_LINK_STATS = 46068

LINK_STATS_ADLARI = [
    "rx_bytes", "rx_frames", "rx_crc_err", "rx_unknown_msgid", "rx_iflag", "rx_bad_len",
    "rx_wrong_sys", "rx_mav1", "rx_overflow", "rx_resync_bytes", "rx_seq_gap",
    "rx_stale_session", "rx_outside_session", "stamp_coarse", "batch_closed",
    "batch_incomplete", "batch_skipped", "batch_old", "batch_dup", "batch_merged",
    "member_overflow", "ping360_incomplete", "ping360_bad", "ping360_orphan", "cmd_deferred",
    "cmd_dropped", "narrow_coalesced", "cmd_bad_field", "grid_missed", "out_fixed",
    "tx_frames", "tx_dropped", "tx_crit_stall", "events_sent", "events_resent", "sessions",
]
# Ölçüm bloğunda koşu boyunca artmaması gereken sayaçlar (--olcum).
OLCUM_SAYACLARI = ["izgara_kacirma", "ore", "fe", "ne", "pe", "dma_rx_hata",
                   "rx_belirsiz", "rx_tasma", "rx_durdu", "rx_yeniden", "besleme_ret"]
HATA_SAYACLARI = ["rx_crc_err", "rx_unknown_msgid", "rx_iflag", "rx_bad_len", "rx_wrong_sys",
                  "rx_mav1", "rx_overflow", "rx_resync_bytes"]

BOARD_HELLO_ADLARI = [
    "dialect_hash", "param_schema_hash", "peer_nonce", "nonce", "boot_count", "reset_flags",
    "core_version", "fw_version", "fw_git", "param_blob_crc", "param_gen", "last_cmd_id",
    "ctrl_period_ns", "param_blob_len", "port_major", "port_minor", "det_schema_major",
    "det_schema_minor", "prev_sess", "sessions_since_boot", "proto_major", "proto_minor", "role",
    "reset_cause", "consecutive_faults", "boot_count_ok", "param_src", "init_rc",
]
BOARD_HELLO_BICIM = "<QQ" + "I" * 11 + "H" * 7 + "B" * 8
RESET_ADLARI = {0: "UNKNOWN", 1: "POWER_ON", 2: "PIN", 3: "SOFTWARE", 4: "WATCHDOG",
                5: "FAULT", 6: "BROWNOUT", 7: "SIL_START", 8: "SIL_REBOOT"}


def x25(veri, crc=0xFFFF):
    for b in veri:
        t = (b ^ crc) & 0xFF
        t = (t ^ (t << 4)) & 0xFF
        crc = ((crc >> 8) ^ (t << 8) ^ (t << 3) ^ (t >> 4)) & 0xFFFF
    return crc


def tablo_oku():
    with open(ALTIN, encoding="utf-8") as f:
        d = json.load(f)
    bilgi = {}
    havuz = []
    for c in d["cerceveler"]:
        bilgi[c["msgid"]] = (c["crc_extra"], c["yuk_azami"])
        # Oturum çerçeveleri (46000–46004) yük havuzuna girmez: kart IDLE'da el sıkışmaya
        # girmesin (dilim 2'de oturum ida_link üst katmanındadır).
        if c["sysid"] == SIM_SYSID and c["compid"] == SIM_COMPID and not 46000 <= c["msgid"] <= 46004:
            b = bytes.fromhex(c["bayt"].replace(" ", ""))
            havuz.append((c["msgid"], c["crc_extra"], b[10:10 + b[1]], c["ad"]))
    return bilgi, havuz


def cerceve_yap(msgid, crc_extra, yuk, seq):
    bas = bytes([0xFD, len(yuk), 0, 0, seq & 0xFF, SIM_SYSID, SIM_COMPID,
                 msgid & 0xFF, (msgid >> 8) & 0xFF, (msgid >> 16) & 0xFF])
    crc = x25(bytes([crc_extra]), x25(yuk, x25(bas[1:])))
    return bas + yuk + bytes([crc & 0xFF, crc >> 8])


class Seri:
    def __init__(self, yol, baud):
        self.fd = os.open(yol, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            fcntl.ioctl(self.fd, termios.TIOCEXCL)
        except OSError:
            pass
        t = termios.tcgetattr(self.fd)
        t[0] = 0                                    # iflag
        t[1] = 0                                    # oflag
        t[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        t[3] = 0                                    # lflag
        t[6][termios.VMIN] = 0
        t[6][termios.VTIME] = 0
        if sys.platform == "darwin":
            t[4] = t[5] = termios.B115200
            termios.tcsetattr(self.fd, termios.TCSANOW, t)
            iossiospeed = 0x80085402                 # _IOW('T', 2, speed_t), speed_t 8 bayt
            fcntl.ioctl(self.fd, iossiospeed, struct.pack("Q", baud))
        else:
            hiz = getattr(termios, "B%d" % baud)
            t[4] = t[5] = hiz
            termios.tcsetattr(self.fd, termios.TCSANOW, t)
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    def oku(self):
        try:
            return os.read(self.fd, 65536)
        except BlockingIOError:
            return b""

    def yaz(self, b):
        try:
            return os.write(self.fd, b)
        except BlockingIOError:
            return 0
        except OSError as e:
            if e.errno in (errno.EAGAIN, errno.ENOBUFS):
                return 0
            raise

    def bosalt(self):
        termios.tcdrain(self.fd)

    def kapat(self):
        os.close(self.fd)


class Ayristirici:
    """Kart -> PC çerçeveleri (MAVLink 2, imzasız)."""

    def __init__(self, bilgi):
        self.bilgi = bilgi
        self.tampon = bytearray()
        self.crc_hata = 0
        self.bilinmeyen = 0
        self.atlanan = 0
        self.kimlik_hata = 0
        self.sayi = {}

    def besle(self, veri, t):
        self.tampon += veri
        cikan = []
        while True:
            i = self.tampon.find(0xFD)
            if i < 0:
                self.atlanan += len(self.tampon)
                self.tampon.clear()
                break
            if i > 0:
                self.atlanan += i
                del self.tampon[:i]
            if len(self.tampon) < 10:
                break
            n = self.tampon[1]
            msgid = self.tampon[7] | (self.tampon[8] << 8) | (self.tampon[9] << 16)
            if msgid not in self.bilgi:
                self.bilinmeyen += 1
                self.atlanan += 1
                del self.tampon[:1]
                continue
            if len(self.tampon) < 12 + n:
                break
            extra, yuk_azami = self.bilgi[msgid]
            crc = x25(bytes([extra]), x25(self.tampon[1:10 + n]))
            if crc != (self.tampon[10 + n] | (self.tampon[11 + n] << 8)):
                self.crc_hata += 1
                self.atlanan += 1
                del self.tampon[:1]
                continue
            if self.tampon[5] != KART_SYSID or self.tampon[6] != KART_COMPID:
                self.kimlik_hata += 1
            yuk = bytes(self.tampon[10:10 + n]) + bytes(max(0, yuk_azami - n))
            cikan.append((msgid, self.tampon[4], yuk, t))
            self.sayi[msgid] = self.sayi.get(msgid, 0) + 1
            del self.tampon[:12 + n]
        return cikan


def link_stats_coz(yuk):
    return dict(zip(LINK_STATS_ADLARI, struct.unpack("<36I", yuk[:144])))


def board_hello_coz(yuk):
    d = dict(zip(BOARD_HELLO_ADLARI, struct.unpack(BOARD_HELLO_BICIM, yuk[:82])))
    d["reset_cause_ad"] = RESET_ADLARI.get(d["reset_cause"], "?")
    return d


def egim(noktalar):
    """En küçük kareler: y = a + b x; b döner."""
    n = len(noktalar)
    if n < 3:
        return None
    mx = sum(p[0] for p in noktalar) / n
    my = sum(p[1] for p in noktalar) / n
    sxx = sum((p[0] - mx) ** 2 for p in noktalar)
    sxy = sum((p[0] - mx) * (p[1] - my) for p in noktalar)
    return sxy / sxx if sxx else None


def kos(arg):
    bilgi, havuz = tablo_oku()
    port = arg.port or os.environ.get("IDA_SERI_UC") or ""
    if not port:
        cands = sorted(p for p in os.listdir("/dev") if p.startswith("cu.usbmodem"))
        port = "/dev/" + cands[0] if cands else ""
    if not port:
        print("duman: seri port bulunamadı", file=sys.stderr)
        return 2
    olcum_once = olcum_oku.ozet_oku(arg.olcum) if (arg.olcum and arg.kip == "yuk") else None
    s = Seri(port, BAUD)
    # Port kapalıyken ST-LINK köprüsünde biriken eski baytlar açılışta gelir (A1 H31; kartta
    # gözlendi): ilk 0,5 s'nin baytları atılır.
    t_at = time.monotonic() + 0.5
    while time.monotonic() < t_at:
        select.select([s.fd], [], [], 0.05)
        s.oku()
    ayr = Ayristirici(bilgi)
    stats = []          # (pc_t, sayaclar)
    hellolar = []
    gonderilen_cerceve = 0
    gonderilen_bayt = 0
    bekleyen = bytearray()
    seq = 0
    havuz_i = 0
    onbellek = {}
    yuk_bas = None
    yuk_son = None
    taban = None
    bitis_stats = None
    gec_yazma = 0
    en_buyuk_bekleyen = 0

    t_bas = time.monotonic()
    sonraki = t_bas
    kredi = 0.0
    yuk_modu = arg.kip == "yuk"
    # yük kipinde: önce taban için bir LINK_STATS beklenir (en çok 3 s), sonra yük başlar
    durum = "taban" if yuk_modu else "izle"
    # Ölçüm bloğu kipi (--olcum): sayaçlar SWD ile okunur; kart IDLE'da LINK_STATS göndermez
    # (dilim 2), bu yüzden taban beklenmez.
    olcum_modu = yuk_modu and arg.olcum is not None
    if olcum_modu:
        durum = "yuk"
        yuk_bas = time.monotonic()
        yuk_son = yuk_bas + arg.sure
        sonraki = yuk_bas
    t_izle_son = t_bas + arg.sure
    while True:
        simdi = time.monotonic()
        if durum == "izle" and simdi >= t_izle_son:
            break
        if durum == "taban" and simdi - t_bas > 3.0:
            print("duman: 3 s içinde IDA_LINK_STATS gelmedi", file=sys.stderr)
            s.kapat()
            return 3
        if durum == "yuk":
            if simdi >= yuk_son:
                # kalan baytları yaz, hattı boşalt, sonra son sayaçları bekle
                while bekleyen:
                    w = s.yaz(bytes(bekleyen))
                    del bekleyen[:w]
                    if not w:
                        select.select([], [s.fd], [], 0.01)
                s.bosalt()
                durum = "son"
                t_son = time.monotonic()
                continue
            while simdi >= sonraki:
                kredi += arg.bayt
                while True:
                    msgid, extra, yuk, _ad = havuz[havuz_i]
                    k = (havuz_i, seq)
                    c = onbellek.get(k)
                    if c is None:
                        c = cerceve_yap(msgid, extra, yuk, seq)
                        onbellek[k] = c
                    if len(c) > kredi:
                        break
                    bekleyen += c
                    kredi -= len(c)
                    gonderilen_cerceve += 1
                    gonderilen_bayt += len(c)
                    seq = (seq + 1) & 0xFF
                    havuz_i = (havuz_i + 1) % len(havuz)
                sonraki += PERIYOT_S
                if simdi - sonraki > 0.05:
                    gec_yazma += 1
            if len(bekleyen) > en_buyuk_bekleyen:
                en_buyuk_bekleyen = len(bekleyen)
        if durum == "son" and olcum_modu and time.monotonic() - t_son >= 1.5:
            break
        if durum == "son" and time.monotonic() - t_son > 15.0:
            print("duman: yük sonrası IDA_LINK_STATS gelmedi", file=sys.stderr)
            break
        yazilacak = [s.fd] if bekleyen else []
        bekle = max(0.0, min(sonraki - time.monotonic(), 0.005)) if durum == "yuk" else 0.02
        r, w, _ = select.select([s.fd], yazilacak, [], bekle)
        if w and bekleyen:
            n = s.yaz(bytes(bekleyen[:4096]))
            del bekleyen[:n]
        if r:
            veri = s.oku()
            t = time.monotonic()
            for msgid, _seq, yuk, tt in ayr.besle(veri, t):
                if msgid == MSG_LINK_STATS:
                    sy = link_stats_coz(yuk)
                    stats.append((tt, sy))
                    if durum == "taban":
                        taban = (tt, sy)
                        durum = "yuk"
                        yuk_bas = time.monotonic()
                        yuk_son = yuk_bas + arg.sure
                        sonraki = yuk_bas
                    elif durum == "son" and tt - t_son >= 1.5:
                        bitis_stats = (tt, sy)
                elif msgid == MSG_BOARD_HELLO:
                    hellolar.append((tt, board_hello_coz(yuk)))
            if durum == "son" and bitis_stats is not None:
                break
    s.kapat()
    olcum_sonra = None
    if olcum_once is not None:
        time.sleep(1.2)  # blok 1 Hz'te güncellenir
        olcum_sonra = olcum_oku.ozet_oku(arg.olcum)

    ozet = {
        "port": port,
        "kip": arg.kip,
        "pc_cerceve": ayr.sayi,
        "pc_crc_hata": ayr.crc_hata,
        "pc_bilinmeyen_kimlik": ayr.bilinmeyen,
        "pc_atlanan_bayt": ayr.atlanan,
        "pc_kimlik_hata": ayr.kimlik_hata,
        "link_stats_sayisi": len(stats),
        "board_hello_sayisi": len(hellolar),
    }
    if hellolar:
        ozet["son_board_hello"] = hellolar[-1][1]
        ozet["dialect_hash_hex"] = "0x%016X" % hellolar[-1][1]["dialect_hash"]
    if stats:
        ozet["son_link_stats"] = stats[-1][1]
    # Kart saati / PC monotonic saati: 1 Hz yayının geliş zamanlarının eğimi (yayın kartın duvar
    # saatinin 1 000 000 µs ızgarasındadır). Ardışık yayınlar arasında atlama varsa sayılır.
    if len(stats) >= 3:
        nokta = [(i, stats[i][0]) for i in range(len(stats))]
        b = egim(nokta)
        if b:
            ozet["yayin_periyodu_pc_s"] = b
            ozet["kart_saati_pc_ppm"] = (1.0 / b - 1.0) * 1e6
        araliklar = [stats[i + 1][0] - stats[i][0] for i in range(len(stats) - 1)]
        ozet["yayin_araligi_en_kucuk_s"] = min(araliklar)
        ozet["yayin_araligi_en_buyuk_s"] = max(araliklar)
    if olcum_modu and olcum_sonra is not None:
        esle = {a: "say_" + a for a in LINK_STATS_ADLARI}
        esle["rx_stale_session"] = "say_stale_session"
        esle["rx_outside_session"] = "say_outside_session"
        taban = (0.0, {a: olcum_once.get(esle[a], 0) for a in LINK_STATS_ADLARI})
        bitis_stats = (0.0, {a: olcum_sonra.get(esle[a], 0) for a in LINK_STATS_ADLARI})
    if yuk_modu and taban is not None:
        ozet["yuk_bayt_periyot"] = arg.bayt
        ozet["yuk_sure_s"] = arg.sure
        ozet["gonderilen_cerceve"] = gonderilen_cerceve
        ozet["gonderilen_bayt"] = gonderilen_bayt
        ozet["gercek_bayt_s"] = gonderilen_bayt / arg.sure
        ozet["kapasite_orani"] = gonderilen_bayt / arg.sure / (BAUD / 10)
        ozet["pc_gec_periyot"] = gec_yazma
        ozet["pc_en_buyuk_bekleyen_bayt"] = en_buyuk_bekleyen
        if bitis_stats is not None:
            fark = {a: (bitis_stats[1][a] - taban[1][a]) & 0xFFFFFFFF for a in LINK_STATS_ADLARI}
            ozet["kart_fark"] = fark
            hatalar = {a: fark[a] for a in HATA_SAYACLARI}
            ozet["kart_hata_farki"] = hatalar
            gecti = (fark["rx_frames"] == gonderilen_cerceve and
                     fark["rx_bytes"] == gonderilen_bayt and
                     all(v == 0 for v in hatalar.values()) and
                     fark["rx_seq_gap"] <= 1 and fark["grid_missed"] == 0 and
                     ayr.crc_hata == 0)
            if olcum_sonra is not None:
                ofark = {a: (olcum_sonra[a] - olcum_once[a]) & 0xFFFFFFFF for a in OLCUM_SAYACLARI}
                ozet["olcum_fark"] = ofark
                ozet["olcum_izgara_islenen"] = (olcum_sonra["izgara_sayisi"] -
                                                olcum_once["izgara_sayisi"]) & 0xFFFFFFFF
                gecti = gecti and all(v == 0 for v in ofark.values())
            ozet["sonuc"] = "GECTI" if gecti else "GECMEDI"
        else:
            ozet["sonuc"] = "EKSIK"
    if arg.json:
        with open(arg.json, "w", encoding="utf-8") as f:
            json.dump(ozet, f, indent=2, ensure_ascii=False)
    for k, v in ozet.items():
        if isinstance(v, dict):
            print(k + ":")
            for kk, vv in v.items():
                print("    %s = %s" % (kk, vv))
        else:
            print("%s = %s" % (k, v))
    if yuk_modu:
        return 0 if ozet.get("sonuc") == "GECTI" else 1
    return 0 if stats or hellolar else 1


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("kip", choices=["izle", "yuk"])
    p.add_argument("--sure", type=float, default=5.0, help="izleme ya da yük süresi (s)")
    p.add_argument("--bayt", type=int, default=900, help="periyot (5 ms) başına ortalama bayt")
    p.add_argument("--port", default=None)
    p.add_argument("--json", default=None)
    p.add_argument("--olcum", default=None, metavar="ONAYAR",
                   help="yükten önce/sonra ölçüm bloğunu SWD ile oku (ör. fw-release)")
    return kos(p.parse_args(argv))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
