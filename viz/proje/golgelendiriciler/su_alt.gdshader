// Su yüzeyi, alttan (iki kademe): Snell penceresi içinde kırılan gökyüzü, dışında tam iç
// yansıma. Opaktır ve derinlik yazar; böylece su altı soğurması (son işlem ya da sis) yüzeye
// olan uzaklığı bilir. Sade kademede dalga_sayisi = 0 (düz) ve Godot sisi uygulanır.
shader_type spatial;
render_mode unshaded, cull_front, depth_draw_opaque;

#include "res://golgelendiriciler/dalga.gdshaderinc"

uniform vec3 gok_ufuk = vec3(0.55, 0.70, 0.85);
uniform vec3 gok_tepe = vec3(0.15, 0.35, 0.70);
uniform vec3 gunes_yonu = vec3(0.0, 1.0, 0.0);   // Godot dünyası, sahneden güneşe
uniform vec3 gunes_rengi = vec3(1.0, 0.96, 0.9);
uniform vec3 yansima_rengi = vec3(0.004, 0.05, 0.08);
uniform float parlaklik = 1.0;
uniform vec3 sonum_rengi = vec3(1.0);   // sade: kameraya inen yolun soğurması (tamda son işlemde)

varying vec3 d_konum;
varying vec3 d_normal;

void vertex() {
	vec3 w = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 d = dalga_hesapla(w.xz);
	VERTEX.y += d.x;
	d_konum = w + vec3(0.0, d.x, 0.0);
	d_normal = normalize(vec3(-d.y, 1.0, -d.z));
}

void fragment() {
	vec3 kam = INV_VIEW_MATRIX[3].xyz;
	vec3 isin = normalize(d_konum - kam);          // yukarı doğru giden bakış ışını
	vec3 n = d_normal;                             // yukarı normal
	float cos_i = clamp(dot(isin, n), 0.0, 1.0);
	float sin_i = sqrt(1.0 - cos_i * cos_i);
	float sin_t = sin_i * 1.333;
	vec3 renk;
	if (sin_t < 1.0) {
		float cos_t = sqrt(1.0 - sin_t * sin_t);
		// Kırılan ışın havada: yatay bileşen 1,333 kat büyür.
		vec3 yatay = isin - n * cos_i;
		vec3 hava = normalize(n * cos_t + yatay * 1.333);
		float h = clamp(hava.y, 0.0, 1.0);
		vec3 gok = mix(gok_ufuk, gok_tepe, pow(h, 0.6));
		float g = pow(max(dot(hava, gunes_yonu), 0.0), 350.0) * 25.0 + pow(max(dot(hava, gunes_yonu), 0.0), 8.0) * 0.6;
		// Fresnel (su → hava), pencere kenarında artar.
		float F = 0.02 + 0.98 * pow(1.0 - cos_t, 5.0);
		renk = mix((gok + gunes_rengi * g) * parlaklik, yansima_rengi, F);
	} else {
		renk = yansima_rengi;
	}
	ALBEDO = renk * sonum_rengi;
}
