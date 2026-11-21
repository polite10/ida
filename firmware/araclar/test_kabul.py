"""kabul.py'nin kart gerektirmeyen sınaması (CTest: firmware.araclar.kabul)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kabul  # noqa: E402

N = [1500] * 8
GUVENLI = kabul.THR_VALID | kabul.IS_SAFE | kabul.ASYNC | 8  # SRC_TICK da kurulu olabilir


def asenk_dizisi(ilk=0.0065, n=49, son=41):
    return [(ilk + 0.01 * i, GUVENLI, N, son) for i in range(n)]


def main():
    hata = 0

    def iddia(kosul, ileti):
        nonlocal hata
        print(("GECTI " if kosul else "KALDI ") + ileti)
        if not kosul:
            hata += 1

    g, a = kabul.tbag_degerlendir(asenk_dizisi(), 0.4975, 41, True)
    iddia(g, "olağan T_bağ dizisi geçer (%s)" % a["hatalar"])
    # Sol bulgu 5 karşı örneği: hiç güvenli çıktı yokken geçmemeli.
    g, a = kabul.tbag_degerlendir([], 0.4975, 41, True)
    iddia(not g, "sıfır eşzamansız çıktı başarısız")
    g, _ = kabul.tbag_degerlendir(asenk_dizisi(ilk=0.2), 0.4975, 41, True)
    iddia(not g, "ilk güvenli çıktı geç: başarısız")
    d = asenk_dizisi()
    d[3] = (d[3][0], kabul.THR_VALID | kabul.ASYNC, N, 41)
    g, _ = kabul.tbag_degerlendir(d, 0.4975, 41, True)
    iddia(not g, "IS_SAFE eksik: başarısız")
    d = asenk_dizisi()
    d[5] = (d[5][0], GUVENLI, [1600] + [1500] * 7, 41)
    g, _ = kabul.tbag_degerlendir(d, 0.4975, 41, True)
    iddia(not g, "nötr olmayan darbe genişliği: başarısız")
    g, _ = kabul.tbag_degerlendir(asenk_dizisi(son=40), 0.4975, 41, True)
    iddia(not g, "batch_seq son kapanan parti değil: başarısız")
    g, _ = kabul.tbag_degerlendir(asenk_dizisi(), None, 41, True)
    iddia(not g, "IDLE duyurusu yok: başarısız")
    g, _ = kabul.tbag_degerlendir(asenk_dizisi(), 0.4975, 41, False)
    iddia(not g, "yeniden el sıkışma yok: başarısız")
    print("test_kabul: %s (%d kalan)" % ("GECTI" if hata == 0 else "KALDI", hata))
    return 0 if hata == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
