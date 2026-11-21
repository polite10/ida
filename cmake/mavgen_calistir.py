"""MAVLink C kod üretecini (pymavlink mavgen) kurulumsuz çalıştırır.

Kullanım:
    python3 -I cmake/mavgen_calistir.py <pymavlink-kaynak-kökü> <çıktı-dizini> <dialect.xml>

pymavlink'in kaynak dağıtımında paket dosyaları arşivin kökündedir; mavgen ise kendisini
`pymavlink` adlı paket içinden içe aktarır. Bu betik o kökü `pymavlink` adıyla yükler,
böylece pip kurulumu ve dizin adı değiştirme gerekmez. Yalnız standart kitaplık kullanılır.

Üretim: MAVLink 2 tel protokolü, C dili, XML şema doğrulaması kapalı (lxml gerektirir;
dialect'in kendi doğrulaması protokol kütüphanesi işinindir). Çıkış kodu: 0 başarılı.
"""

import importlib.util
import os
import sys


def main(argv):
    if len(argv) != 4:
        print(__doc__, file=sys.stderr)
        return 64
    kok, cikti, xml = (os.path.abspath(a) for a in argv[1:])
    init = os.path.join(kok, "__init__.py")
    if not os.path.isfile(init):
        print("pymavlink kökü geçersiz: " + kok, file=sys.stderr)
        return 66
    spec = importlib.util.spec_from_file_location(
        "pymavlink", init, submodule_search_locations=[kok])
    modul = importlib.util.module_from_spec(spec)
    sys.modules["pymavlink"] = modul
    spec.loader.exec_module(modul)

    from pymavlink.generator import mavgen, mavparse  # noqa: E402

    secenek = mavgen.Opts(cikti, wire_protocol=mavparse.PROTOCOL_2_0,
                          language="C", validate=False)
    return 0 if mavgen.mavgen(secenek, [xml]) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
