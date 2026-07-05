import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const specRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const mapPath = resolve(specRoot, 'rule-id-map.json');

const map = JSON.parse(readFileSync(mapPath, 'utf8')) as Record<string, string>;
const boundaryCount = Object.keys(map).length;

if (boundaryCount === 0) {
  throw new Error('rule-id-map.json is empty');
}

const seen = new Set<string>();
for (const [boundaryId, ruleId] of Object.entries(map)) {
  if (!/^boundary-\d{3}$/.test(boundaryId)) {
    throw new Error(`Invalid boundary id: ${boundaryId}`);
  }
  if (!/^ZCC-[A-Z]+-\d{3}$/.test(ruleId)) {
    throw new Error(`Invalid rule id: ${ruleId}`);
  }
  if (seen.has(ruleId)) {
    throw new Error(`Duplicate rule id: ${ruleId}`);
  }
  seen.add(ruleId);
}

console.log(`Validated ${boundaryCount} boundary-to-rule mappings.`);
