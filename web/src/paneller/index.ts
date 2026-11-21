// Kök sayfa: panel listesi. Bağlantı açmaz; jetonu panel bağlantılarına taşır.

import { el } from '../ortak/dom';
import { MARKA_SVG, PANELLER } from '../ortak/kabuk';

const jetonVar = /(^|[#&])erisim=[0-9a-f]{64}($|&)/.test(location.hash);
const marka = el('div', { class: 'marka', style: 'border:0;padding:0' });
marka.innerHTML = MARKA_SVG;
marka.append(el('span', { class: 'marka-ad' }, 'İDA'));

const kartlar = el('div', { class: 'panel-kartlari' });
PANELLER.forEach((p, i) => {
  kartlar.append(
    el(
      'a',
      { class: 'kart panel-karti', href: `/panel/${p.ad}.html${location.hash}` },
      el('span', { class: 'no' }, String(i + 1).padStart(2, '0')),
      el('span', { class: 'ad' }, p.baslik),
      el('span', { class: 'acik' }, p.aciklama),
    ),
  );
});

document.title = 'Paneller · İDA';
document.body.append(
  el(
    'div',
    { class: 'liste-kok' },
    el('div', { class: 'liste-baslik' }, marka, el('h1', null, 'Laboratuvar panelleri'), el('span', { class: 'kart-not' }, 'her panel ayrı pencerede açılabilir')),
    kartlar,
    el(
      'p',
      { class: 'liste-not' },
      jetonVar
        ? 'Erişim jetonu adresin # sonrasında; panel bağlantıları onu taşır ve sunucuya göndermez (A3-K05).'
        : 'Adreste erişim jetonu yok: sim sunucusunun yazdığı açılış adresini (…/#erisim=<jeton>) kullanın.',
    ),
  ),
);
