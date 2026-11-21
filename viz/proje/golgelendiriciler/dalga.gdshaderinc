// Dalga yüzeyi (A2 §3.4 faz kuralı; 07 §2.7: doğrudan toplam, bileşen kesmesi yok).
//   ζ = Σ a_i cos(θ_i − (kx_i·x + kz_i·z)),  θ_i = (ω_i t + φ_i) mod 2π (CPU'da double)
// dalga_k[i] = (k sin β, −k cos β, a, k); x = Godot x (doğu), z = Godot z (−kuzey).
// Eklenti (IdaA2Istemci) her karede dalga_sayisi, dalga_k ve dalga_faz'ı yazar; paket
// viz/a2 dalga_paketle() ile CPU'daki dalga_yuksekligi() ile aynı bileşenlerden üretilir
// (host.viz.dalga). Sade kademede dalga_sayisi = 0 (düz su).
//
// Geometrik toplamda bileşen genliği hiçbir yerde değiştirilmez (sol #11): uzak, seyrek ağ
// halkalarında örnekleme kaynaklı fark yalnız ağın aralığından gelir (07 §2.7 ayrıntı düzeyi).
// Uzakta titremeyi azaltmak gerekirse bu yalnız gölgeleme normalinde yapılır (su_ust_tam).

uniform int dalga_sayisi = 0;
uniform vec4 dalga_k[256];
uniform float dalga_faz[256];

// x: yükseklik, y: ∂ζ/∂x, z: ∂ζ/∂z
vec3 dalga_hesapla(vec2 p) {
	float h = 0.0;
	vec2 g = vec2(0.0);
	for (int i = 0; i < dalga_sayisi; i++) {
		vec4 c = dalga_k[i];
		float th = dalga_faz[i] - (c.x * p.x + c.y * p.y);
		h += c.z * cos(th);
		g += c.z * sin(th) * c.xy;
	}
	return vec3(h, g);
}
