"""MP6a: ham kare dökümünü (kare_NNNNNNN.rgb / .rgba) PNG'ye çevirir.

Kullanım: python3 -I png.py <ham_dosya> <genislik> <yukseklik> <cikti.png>
"""
import sys

import numpy as np
from PIL import Image


def main() -> None:
    yol, w, h, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    veri = np.fromfile(yol, dtype=np.uint8)
    bpp = veri.size // (w * h)
    img = veri.reshape(h, w, bpp)[:, :, :3]
    Image.fromarray(img, "RGB").save(out)


if __name__ == "__main__":
    main()
