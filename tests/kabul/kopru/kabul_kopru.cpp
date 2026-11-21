// tests/kabul/kopru — köprü düzeyindeki vakaların giriş noktası. Vakalar kabul_olay.cpp,
// kabul_zaman.cpp ve kabul_saglamlik.cpp'dedir; ortak yardımcılar ortak.hpp'de.
#include "kontrol.hpp"

int main(int argc, char** argv) {
    return kabul::calistir(argc, argv);
}
