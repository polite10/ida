/*
 * param_listesi.inc — bütün modüllerin parametre listelerinin tek bileşimi (W2).
 *
 * İçeren dosya önce IDA_PARAM(kimlik, ad, tip, sinif, en_az, en_cok, varsayilan, alan)
 * makrosunu tanımlar, sonra bu dosyayı içerir. Sıra kimlik bloklarının artan sırasıdır
 * (0x01xx çerçeve, 0x02xx kestirim, 0x03xx güdüm, 0x04xx görev ve arıza, 0x05xx kontrol,
 * 0x06xx atış kontrol, 0x07xx sonar); tablonun kimliğe göre kesin artan olduğu birim testle
 * denetlenir. Bu dosyaya yeni modül eklemek dışında dokunulmaz; parametre eklemek modülün
 * kendi .def dosyasında tek satırdır.
 */
#include "cerceve/cerceve_param.def"
#include "kestirim/kestirim_param.def"
#include "gudum/gudum_param.def"
#include "gorev/gorev_param.def"
#include "kontrol/kontrol_param.def"
#include "atis/atis_param.def"
#include "sonar/sonar_param.def"
