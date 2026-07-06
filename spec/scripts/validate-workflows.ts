import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const specRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const workflowPath = resolve(specRoot, '..', '.github', 'workflows', 'boundary-gates.yml');
const workflow = readFileSync(workflowPath, 'utf8');

for (const needle of [
  'name: ZCC Boundary Contract Gates',
  'make boundaries-validate',
  'make workflows-validate',
  'make boundaries-matrix',
  'make boundaries-test',
  'make evidence-report ALL_RUNS=1',
]) {
  if (!workflow.includes(needle)) {
    throw new Error(`boundary-gates.yml is missing required content: ${needle}`);
  }
}

if (!workflow.includes('branches: [ "main" ]')) {
  throw new Error('boundary-gates.yml must target main branches');
}

console.log('Validated boundary workflow wiring.');
