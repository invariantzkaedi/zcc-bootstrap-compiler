/**
 * emit-evidence.ts
 *
 * Evidence ledger emitter with sharding, provenance, and waiver enforcement.
 */
import { readFileSync, existsSync, mkdirSync, statSync, writeFileSync, readdirSync } from 'fs';
import { resolve, dirname, basename } from 'path';
import { randomBytes, createHash } from 'crypto';
import { execSync } from 'child_process';
import os from 'os';
import Ajv from 'ajv';
import addFormats from 'ajv-formats';

const SPEC_ROOT = dirname(dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Z]:)/, '$1')));

function parseArgs(argv: string[]): Record<string, string> {
  const args: Record<string, string> = {};
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg.startsWith('--')) {
      const key = arg.slice(2);
      if (i + 1 < argv.length && !argv[i + 1].startsWith('--')) {
        args[key] = argv[i + 1];
        i++;
      } else {
        args[key] = 'true';
      }
    }
  }
  return args;
}

function sanitize(str: string): string {
  if (!str) return 'unknown';
  return str.replace(/[^A-Za-z0-9._-]/g, '');
}

function getFileHash(filepath: string): string | { exists: false } {
  if (!existsSync(filepath)) {
    return { exists: false };
  }
  const content = readFileSync(filepath);
  return 'sha256:' + createHash('sha256').update(content).digest('hex');
}

function getEnvironmentProvenance() {
  let git_sha = 'unknown';
  let is_dirty = false;
  try {
    git_sha = execSync('git rev-parse HEAD', { encoding: 'utf-8', cwd: SPEC_ROOT }).trim();
    const status = execSync('git status --porcelain', { encoding: 'utf-8', cwd: SPEC_ROOT }).trim();
    is_dirty = status.length > 0;
  } catch (e) {
    // Ignore git errors
  }

  const getToolVersion = (cmd: string) => {
    try {
      return execSync(`${cmd} --version`, { encoding: 'utf-8' }).split('\n')[0].trim();
    } catch {
      return 'unknown';
    }
  };

  const tool_versions = {
    gcc: getToolVersion('gcc'),
    node: getToolVersion('node'),
    pnpm: getToolVersion('pnpm')
  };

  const allowlist = ['LC_ALL', 'TZ', 'SOURCE_DATE_EPOCH'];
  const env_vars: Record<string, string> = {};
  for (const k of allowlist) {
    if (process.env[k] !== undefined) {
      env_vars[k] = process.env[k]!;
    }
  }

  return { git_sha, is_dirty, tool_versions, env_vars };
}

function generateReport(args: Record<string, string>): void {
  const runId = args['run-id'] ? sanitize(args['run-id']) : undefined;
  const allRuns = args['all-runs'] === 'true';
  if (!runId && !allRuns) {
    console.error('❌ ERROR: evidence-report requires --run-id=<run-id> or --all-runs');
    process.exit(1);
  }

  const runsDir = resolve(SPEC_ROOT, 'artifacts', 'evidence', 'runs');

  if (!existsSync(runsDir)) {
    console.log('📋 No evidence ledger found. Run boundary tests first.');
    process.exit(0);
  }

  let runDirs: string[] = [];
  if (runId && !allRuns) {
    runDirs = [resolve(runsDir, runId)];
  } else {
    runDirs = readdirSync(runsDir)
      .map(name => resolve(runsDir, name))
      .filter(p => statSync(p).isDirectory());
  }

  const files: string[] = [];
  for (const rdir of runDirs) {
    if (existsSync(rdir)) {
      readdirSync(rdir)
        .filter(name => name.endsWith('.jsonl'))
        .forEach(name => files.push(resolve(rdir, name)));
    }
  }

  const events: any[] = [];

  for (const file of files) {
    const lines = readFileSync(file, 'utf-8').split('\n').filter(l => l.trim());
    for (const l of lines) {
      try {
        const ev = JSON.parse(l);
        // Waiver expiry check
        if (ev.result === 'waived_fail' && ev.evidence?.expiry) {
          const expiryDate = new Date(ev.evidence.expiry);
          if (expiryDate < new Date()) {
            ev.result = 'expired_waiver';
          }
        }
        events.push(ev);
      } catch (e) {
        console.error(`❌ Invalid JSON in ${file}`);
      }
    }
  }

  if (events.length === 0) {
    console.log(`📋 No events found for run ${runId || 'ALL'}`);
    process.exit(0);
  }

  console.log('═══════════════════════════════════════════');
  console.log('  ZCC Evidence Ledger Report');
  console.log(`  Run Scope: ${runId ? runId : 'ALL'}`);
  console.log('═══════════════════════════════════════════');
  console.log('');
  console.log(`Total events: ${events.length}`);
  console.log('');

  const resultCounts: Record<string, number> = {};
  const ruleResults: Record<string, string[]> = {};
  let failureFound = false;

  for (const e of events) {
    resultCounts[e.result] = (resultCounts[e.result] || 0) + 1;
    for (const ruleId of e.rule_ids) {
      (ruleResults[ruleId] ??= []).push(e.result);
    }
    if (e.result === 'fail' || e.result === 'expired_waiver' || e.result === 'error') {
      failureFound = true;
    }
  }

  console.log('Results:');
  for (const [result, count] of Object.entries(resultCounts)) {
    if (result === 'waived_fail') {
      console.log(`  ⚠️  waived_fail: ${count} (known failures recorded as active waivers)`);
    } else {
      const icon = result === 'pass' ? '✅' : (result === 'skip' ? '⏭️ ' : '❌');
      console.log(`  ${icon} ${result}: ${count}`);
    }
  }
  console.log('');

  console.log('Rules verified:');
  for (const [ruleId, results] of Object.entries(ruleResults).sort()) {
    const passCount = results.filter(r => r === 'pass').length;
    const failCount = results.filter(r => r === 'fail' || r === 'error' || r === 'expired_waiver').length;
    const waivedCount = results.filter(r => r === 'waived_fail').length;
    const icon = failCount > 0 ? '❌' : (waivedCount > 0 ? '⚠️ ' : '✅');
    let msg = `  ${icon} ${ruleId}: ${passCount} pass, ${failCount} fail/error`;
    if (waivedCount > 0) msg += `, ${waivedCount} waived_fail`;
    console.log(msg);
  }

  if (failureFound) {
    console.error('\\n❌ FAIL: Found events with result "fail" or "expired_waiver"');
    process.exit(1);
  } else {
    console.log('\\n✅ PASS: No strict failures found');
    process.exit(0);
  }
}

function emitEvent(args: Record<string, string>): void {
  const required = ['stage', 'rules', 'target', 'result'];
  for (const key of required) {
    if (!args[key]) {
      console.error(`❌ Missing required argument: --${key}`);
      process.exit(1);
    }
  }

  const runId = sanitize(args['run-id'] || 'default-run');
  const testName = sanitize(args['test'] || 'unknown-test');

  const eventId = `evt_${randomBytes(8).toString('hex')}`;
  const event: any = {
    event_id: eventId,
    timestamp: new Date().toISOString(),
    workflow_stage: args['stage'],
    rule_ids: args['rules'].split(',').map(s => s.trim()),
    target: args['target'],
    result: args['result'],
    hostname: os.hostname(),
    environment: getEnvironmentProvenance(),
  };

  if (args['source']) {
    event.source_hash = getFileHash(args['source']);
    if (typeof event.source_hash === 'object' && event.source_hash.exists === false) {
       event.source_hash = 'sha256:0000000000000000000000000000000000000000000000000000000000000000'; // dummy for schema compliance if missing
    }
  } else if (args['source-hash']) {
    event.source_hash = args['source-hash'];
  } else {
    event.source_hash = 'sha256:0000000000000000000000000000000000000000000000000000000000000000';
  }

  const evidence: any = {};
  if (args['test']) evidence.tests = args['test'].split(',').map(s => s.trim());

  if (args['artifact']) {
    evidence.artifact_hash = getFileHash(args['artifact']);
  } else if (args['artifact-hash']) {
    evidence.artifact_hash = args['artifact-hash'];
  }

  if (args['command']) evidence.command = args['command'];
  if (args['exit-code']) evidence.exit_code = parseInt(args['exit-code'], 10);

  if (args['waiver-id']) evidence.waiver_id = args['waiver-id'];
  if (args['waiver-owner']) evidence.owner = args['waiver-owner'];
  if (args['waiver-expiry']) evidence.expiry = args['waiver-expiry'];

  if (Object.keys(evidence).length > 0) {
    event.evidence = evidence;
  }

  const schemaPath = resolve(SPEC_ROOT, 'schemas', 'compiler-event.schema.json');
  if (existsSync(schemaPath)) {
    const schema = JSON.parse(readFileSync(schemaPath, 'utf-8'));
    const ajv = new Ajv({ allErrors: true, strict: false });
    addFormats(ajv);
    const validate = ajv.compile(schema);
    if (!validate(event)) {
      console.error('❌ Event failed schema validation:');
      for (const err of validate.errors || []) {
        console.error(`   ${err.instancePath || '/'}: ${err.message}`);
      }
      process.exit(1);
    }
  }

  const runDir = resolve(SPEC_ROOT, 'artifacts', 'evidence', 'runs', runId);
  mkdirSync(runDir, { recursive: true });

  const rand = randomBytes(4).toString('hex');
  const shardName = `${testName}-${process.pid}-${rand}.jsonl`;
  const shardPath = resolve(runDir, shardName);

  writeFileSync(shardPath, JSON.stringify(event) + '\n', 'utf-8');
  console.log(`✅ Event ${eventId} saved to ${shardName}`);
}

function main(): void {
  const args = parseArgs(process.argv);
  if (args['report'] === 'true') {
    generateReport(args);
  } else {
    emitEvent(args);
  }
}

main();
