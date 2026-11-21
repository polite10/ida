// Su yüzeyi, üstten (sade kademe; A2 §6: düz su, D = 0 düzlemi). Ekran dokusu okumaz;
// yalnız derinlik dokusundan su içindeki yolu bulup saydamlığı ona göre verir (sığdaki
// nesne görünür, derindeki kaybolur). Yansıma Godot'nun gökyüzü yansımasıyla gelir.
shader_type spatial;
render_mode blend_mix, cull_back, depth_draw_opaque, specular_schlick_ggx;

uniform sampler2D derinlik_dokusu : hint_depth_texture, filter_nearest;
uniform vec3 su_rengi = vec3(0.008, 0.10, 0.16);
uniform float sonum = 0.19;   // β_d kanal ortalaması (1/m)

void fragment() {
	float zd = texture(derinlik_dokusu, SCREEN_UV).r;
	float yol = 1e4;
	float nesne_derinligi = 0.0;
	if (zd > 1e-7) {
		vec4 vp = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, zd, 1.0);
		vp.xyz /= vp.w;
		yol = length(vp.xyz - VERTEX);
		nesne_derinligi = max(0.0, -(INV_VIEW_MATRIX * vec4(vp.xyz, 1.0)).y);
	}
	float F = 0.02 + 0.98 * pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 5.0);
	float ortu = 1.0 - exp(-sonum * (yol + nesne_derinligi));
	ALBEDO = su_rengi;
	ROUGHNESS = 0.06;
	SPECULAR = 0.5;
	METALLIC = 0.0;
	ALPHA = clamp(max(ortu, F), 0.0, 1.0);
}
