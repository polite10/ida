// Su yüzeyi, üstten (tam kademe): dalga geometrisi, ayrıntı normalleri (yalnız gölgelemede),
// kırılan arka plan ve su içi yolda renk soğurması + geri saçılım (07 §2.5'teki
// fenomenolojik model: I = J·e^{−β_d·z} + B∞·(1 − e^{−β_b·z})). Yansıma Godot'nun gökyüzü
// yansımasıyla (Fresnel) gelir.
shader_type spatial;
render_mode blend_mix, cull_back, depth_draw_opaque, specular_schlick_ggx;

#include "res://golgelendiriciler/dalga.gdshaderinc"

uniform sampler2D ekran : hint_screen_texture, filter_linear_mipmap;
uniform sampler2D derinlik_dokusu : hint_depth_texture, filter_nearest;
uniform sampler2D ayrinti_1 : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D ayrinti_2 : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform vec3 beta_d = vec3(0.42, 0.085, 0.07);
uniform vec3 beta_b = vec3(0.30, 0.08, 0.065);
uniform vec3 b_sonsuz = vec3(0.008, 0.10, 0.16);
uniform float isik_olcegi = 1.0;
uniform float zaman = 0.0;          // yalnız ayrıntı dokusunun kayması (görsel)
uniform float ayrinti_gucu = 0.35;
uniform float kirilma = 0.035;

varying vec3 d_konum;
varying vec3 d_normal;

void vertex() {
	vec3 w = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec3 d = dalga_hesapla(w.xz);
	VERTEX.y += d.x;
	d_konum = w + vec3(0.0, d.x, 0.0);
	d_normal = normalize(vec3(-d.y, 1.0, -d.z));
	NORMAL = d_normal;
}

void fragment() {
	vec3 kam = INV_VIEW_MATRIX[3].xyz;
	float uzaklik = length(d_konum - kam);
	// Ayrıntı normalleri: iki kayan katman; uzakta sönümlenir (görüntü titremesin).
	vec2 uv1 = d_konum.xz * 0.11 + zaman * vec2(0.021, 0.013);
	vec2 uv2 = d_konum.xz * 0.37 + zaman * vec2(-0.017, 0.029);
	vec3 a1 = texture(ayrinti_1, uv1).xyz * 2.0 - 1.0;
	vec3 a2 = texture(ayrinti_2, uv2).xyz * 2.0 - 1.0;
	float sonum = ayrinti_gucu / (1.0 + uzaklik * 0.04);
	vec2 dd = (a1.xy + 0.6 * a2.xy) * sonum;
	// Yalnız gölgeleme: uzakta (seyrek ağda örtüşmeden gelen titremeyi azaltmak için) normal
	// yukarıya doğru yumuşatılır; geometri (yükseklik) değişmez (A2 §3.4; sol #11).
	vec3 nd = normalize(mix(d_normal, vec3(0.0, 1.0, 0.0), smoothstep(40.0, 400.0, uzaklik) * 0.8));
	vec3 n = normalize(vec3(nd.x + dd.x, nd.y, nd.z + dd.y));
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	// Kırılma: arka plandaki noktayı derinlik dokusundan bul; önde kalan nesneyi örnekleme.
	vec2 suv = SCREEN_UV + n.xz * kirilma / (1.0 + uzaklik * 0.08);
	float zd = texture(derinlik_dokusu, suv).r;
	vec4 vp = INV_PROJECTION_MATRIX * vec4(suv * 2.0 - 1.0, zd, 1.0);
	vp.xyz /= vp.w;
	if (vp.z > VERTEX.z) {
		suv = SCREEN_UV;
		zd = texture(derinlik_dokusu, suv).r;
		vp = INV_PROJECTION_MATRIX * vec4(suv * 2.0 - 1.0, zd, 1.0);
		vp.xyz /= vp.w;
	}
	vec3 arka = texture(ekran, suv).rgb;
	float yol = zd <= 1e-7 ? 1e4 : length(vp.xyz - VERTEX);
	vec3 arka_dunya = (INV_VIEW_MATRIX * vec4(vp.xyz, 1.0)).xyz;
	float nesne_derinligi = zd <= 1e-7 ? 0.0 : max(0.0, -arka_dunya.y);
	// Nesneye inen güneş ışığı (nesne derinliği) ve göze gelen yol boyunca soğurma.
	vec3 gecen = arka * exp(-beta_d * (yol + nesne_derinligi));
	vec3 sacilan = b_sonsuz * isik_olcegi * (1.0 - exp(-beta_b * yol));
	vec3 V = normalize(kam - d_konum);
	float F = 0.02 + 0.98 * pow(1.0 - clamp(dot(n, V), 0.0, 1.0), 5.0);
	ALBEDO = vec3(0.0);
	EMISSION = (gecen + sacilan) * (1.0 - F);
	METALLIC = 0.0;
	SPECULAR = 0.5;
	ROUGHNESS = 0.035 + min(0.12, uzaklik * 0.0006);
	ALPHA = 1.0;
}
