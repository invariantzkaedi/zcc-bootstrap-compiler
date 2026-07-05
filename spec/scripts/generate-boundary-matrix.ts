import { mkdirSync, readFileSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const specRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const mapPath = resolve(specRoot, 'rule-id-map.json');
const outPath = resolve(specRoot, 'artifacts', 'boundary-matrix.json');

const map = JSON.parse(readFileSync(mapPath, 'utf8')) as Record<string, string>;
const matrix = Object.entries(map)
  .sort(([a], [b]) => a.localeCompare(b))
  .map(([boundary_id, rule_id]) => {
    if (!/^ZCC-[A-Z]+-\d{3}$/.test(rule_id)) {
      throw new Error(`Invalid rule id in boundary matrix: ${rule_id}`);
    }

    return {
      boundary_id,
      rule_id,
      family: rule_id.split('-')[1],
    };
  });

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(matrix, null, 2) + '\n');

console.log(`Wrote ${matrix.length} rows to ${outPath}`);
