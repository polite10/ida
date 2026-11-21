// Su altı son işlemi (tam kademe, kamera su altındayken): her piksel için kameraya olan
// uzaklık z ile I = J·e^{−β_d·(z + d_nesne)} + B∞·(1 − e^{−β_b·z}) (07 §2.5 fenomenolojik
// model; değerler A2 `ortam` iletisinden). d_nesne: nesnenin ortalama yüzeyden derinliği
// (güneş ışığının nesneye inerken soğurulması; yaklaşım).
// Tam ekran dörtgen; kameranın çocuğu, ekran ve derinlik dokusunu okur.
shader_type spatial;
render_mode unshaded, fog_disabled, cull_disabled, depth_test_disabled, depth_draw_never;

uniform sampler2D ekran : hint_screen_texture, filter_linear_mipmap;
uniform sampler2D derinlik_dokusu : hint_depth_texture, filter_nearest;
uniform vec3 beta_d = vec3(0.42, 0.085, 0.07);
uniform vec3 beta_b = vec3(0.30, 0.08, 0.065);
uniform vec3 b_sonsuz = vec3(0.008, 0.10, 0.16);
uniform float isik_olcegi = 1.0;
uniform float kamera_derinligi = 0.0;

void vertex() {
	POSITION = vec4(VERTEX.xy, 1.0, 1.0);
}

void fragment() {
	vec3 c = texture(ekran, SCREEN_UV).rgb;
	float zd = texture(derinlik_dokusu, SCREEN_UV).r;
	vec4 vp = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, zd, 1.0);
	vp.xyz /= vp.w;
	vec3 dunya = (INV_VIEW_MATRIX * vec4(vp.xyz, 1.0)).xyz;
	// Geri saçılan ışık da derinlikle azalır (kameranın derinliğindeki aşağı inen ışık).
	vec3 bs = b_sonsuz * isik_olcegi * exp(-beta_d * kamera_derinligi * 0.6);
	vec3 sonuc;
	if (zd <= 1e-7) {
		sonuc = bs;
	} else {
		float z = length(vp.xyz);
		float d_nesne = clamp(-dunya.y, 0.0, 500.0);
		sonuc = c * exp(-beta_d * (z + d_nesne)) + bs * (1.0 - exp(-beta_b * z));
	}
	ALBEDO = sonuc;
}
