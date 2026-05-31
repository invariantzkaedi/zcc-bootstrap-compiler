// src/directorates/audit-authority.ts
import { createHash } from "crypto";
import * as fs from "fs";
import * as path from "path";
import type { Decision } from "../sovereignty/action-types";

export interface AuditRecord {
  index: number;
  decision: Decision;
  previousHash: string;
  timestamp: number;
  hash: string;
}

export class AuditAuthority {
  private ledgerPath = path.join(__dirname, "../../sovereign_audit_ledger.jsonl");
  private lastHash = "0000000000000000000000000000000000000000000000000000000000000000";
  private indexCount = 0;
  private GENESIS_SECRET_SEED = "zkaedi-sovereign-genesis-seed-v1";

  constructor() {
    this.initializeAndVerifyLedger();
  }

  private initializeAndVerifyLedger(): void {
    if (!fs.existsSync(this.ledgerPath)) {
      // Initialize with Genesis block
      const index = 0;
      const timestamp = Date.now();
      const genesisDecision: Decision = {
        actionId: "genesis-000",
        approved: true,
        timestamp,
        checks: { constitutional: true, trust: true, simulation: true, risk: true },
        metrics: { trustScore: 1.0, riskScore: 0.0, simulationDurationMs: 0 },
        reason: "Genesis sovereign state initialized."
      };

      const canonicalPayload = this.toCanonicalJson(genesisDecision);
      const recordPayload = `${index}:${canonicalPayload}:${this.lastHash}:${timestamp}`;
      const hash = createHash("sha256").update(recordPayload).digest("hex");

      const record: AuditRecord = {
        index,
        decision: genesisDecision,
        previousHash: this.lastHash,
        timestamp,
        hash
      };

      fs.writeFileSync(this.ledgerPath, JSON.stringify(record) + "\n");
      this.lastHash = hash;
      this.indexCount = 1;
      console.log(`[AUDIT-LEDGER] New sovereign ledger initialized. Genesis block registered.`);
      return;
    }

    // Read and verify existing ledger chain
    try {
      const data = fs.readFileSync(this.ledgerPath, "utf-8").trim();
      if (!data) {
        throw new Error("Ledger file is empty.");
      }

      const lines = data.split("\n").filter(line => line.trim() !== "");
      let expectedPrevHash = "0000000000000000000000000000000000000000000000000000000000000000";
      let expectedIndex = 0;

      for (let i = 0; i < lines.length; i++) {
        const record: AuditRecord = JSON.parse(lines[i]);

        // 1. Verify index progression
        if (record.index !== expectedIndex) {
          throw new Error(`Tamper detected: Invalid index sequence at block ${i}. Expected ${expectedIndex}, found ${record.index}.`);
        }

        // 2. Verify previous hash chaining
        if (record.previousHash !== expectedPrevHash) {
          throw new Error(`Tamper detected: Chaining broken at block ${i}. Expected previous hash ${expectedPrevHash}, found ${record.previousHash}.`);
        }

        // 3. Verify block hash recalculation parity
        const canonicalPayload = this.toCanonicalJson(record.decision);
        const recordPayload = `${record.index}:${canonicalPayload}:${record.previousHash}:${record.timestamp}`;
        const computedHash = createHash("sha256").update(recordPayload).digest("hex");

        if (record.hash !== computedHash) {
          throw new Error(`Tamper detected: Hash mismatch at block ${i}. Computed ${computedHash}, found ${record.hash}.`);
        }

        expectedPrevHash = record.hash;
        expectedIndex++;
      }

      this.lastHash = expectedPrevHash;
      this.indexCount = expectedIndex;
      console.log(`[AUDIT-LEDGER] Existing ledger verified successfully. Chained blocks: ${lines.length}.`);
    } catch (e) {
      console.error(`[AUDIT-FATAL] LEDGER CORRUPTION / TAMPERING DETECTED! Error: ${String(e)}`);
      throw e;
    }
  }

  public recordDecision(decision: Decision): AuditRecord {
    // Audit-ledger pre-flight tamper verification
    this.runIntegritySelfAudit();

    const index = this.indexCount++;
    const timestamp = Date.now();
    
    const canonicalPayload = this.toCanonicalJson(decision);
    const recordPayload = `${index}:${canonicalPayload}:${this.lastHash}:${timestamp}`;
    const hash = createHash("sha256").update(recordPayload).digest("hex");

    const record: AuditRecord = {
      index,
      decision,
      previousHash: this.lastHash,
      timestamp,
      hash
    };

    fs.appendFileSync(this.ledgerPath, JSON.stringify(record) + "\n");
    this.lastHash = hash;

    return record;
  }

  private runIntegritySelfAudit(): void {
    // Quick self-check to make sure history file on disk matches memory state before appending
    const data = fs.readFileSync(this.ledgerPath, "utf-8").trim();
    const lines = data.split("\n").filter(line => line.trim() !== "");
    
    if (lines.length !== this.indexCount) {
      throw new Error(`Tamper detected: Ledger line count ${lines.length} does not match active logical sequence ${this.indexCount}.`);
    }

    const lastRecord: AuditRecord = JSON.parse(lines[lines.length - 1]);
    if (lastRecord.hash !== this.lastHash) {
      throw new Error(`Tamper detected: Disk hash ${lastRecord.hash} does not match memory state hash ${this.lastHash}.`);
    }
  }

  /**
   * Deterministically serialize JSON objects by sorting keys alphabetically.
   */
  public toCanonicalJson(obj: any): string {
    if (typeof obj !== "object" || obj === null) {
      return JSON.stringify(obj);
    }
    if (Array.isArray(obj)) {
      return "[" + obj.map(item => this.toCanonicalJson(item)).join(",") + "]";
    }
    const keys = Object.keys(obj).sort();
    const parts = keys.map(key => `"${key}":${this.toCanonicalJson(obj[key])}`);
    return "{" + parts.join(",") + "}";
  }

  public getLedgerPath(): string {
    return this.ledgerPath;
  }
}
