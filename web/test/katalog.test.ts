// Üretilmiş katalog dosyası dondurulmuş JSON ile güncel mi; katalog özeti doğru mu.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { test } from 'node:test';
import { KANALLAR, KATALOG_OZETI, YUKLER } from '../src/a3/katalog';

test('src/a3/katalog.ts, a3-kanallar.json ile güncel', () => {
  execFileSync(process.execPath, ['arac/uret-katalog.mjs', '--denetle'], { stdio: 'pipe' });
});

test('KATALOG_OZETI dosya baytlarının SHA-256 özeti (A3-K16)', () => {
  const b = readFileSync(resolve(process.cwd(), '..', 'docs', 'sozlesmeler', 'sema', 'a3-kanallar.json'));
  assert.equal(KATALOG_OZETI, 'sha256:' + createHash('sha256').update(b).digest('hex'));
});

test('her kanalın yükü katalogda; kanal kimlikleri tekil', () => {
  const ids = new Set<number>();
  for (const k of KANALLAR) {
    assert.ok(k.yuk in YUKLER, k.ad);
    assert.ok(!ids.has(k.kanal_id), `yinelenen kanal_id ${k.kanal_id}`);
    ids.add(k.kanal_id);
  }
});
