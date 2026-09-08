// apps/zcc-cloud/functions/api/openapi.json.js
// Cloudflare Pages Function: GET /api/openapi.json

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
  "Content-Type": "application/json;charset=utf-8"
};

export async function onRequestOptions() {
  return new Response(null, { headers: CORS_HEADERS });
}

export async function onRequestGet() {
  const spec = {
    openapi: "3.1.0",
    info: {
      title: "ZKAEDI Cloud Compiler & Verification API",
      version: "4.0.0",
      description: "Production-grade edge API for sovereign C compilation (x86-64, RISC-V 64, WebAssembly, Windows PE), SSA IR multi-pass optimization, static code security analysis, and ZK-SNARK circuit proof synthesis.",
      contact: {
        name: "ZKAEDI AI Systems",
        url: "https://zkaedi.ai",
        email: "support@zkaedi.ai"
      },
      license: {
        name: "GPL-2.0 / Commercial Pro",
        url: "https://zkaedi.ai/#pricing"
      }
    },
    servers: [
      {
        url: "https://zkaedi.ai",
        description: "Cloudflare Edge Production (Global Anycast, <1ms)"
      }
    ],
    components: {
      securitySchemes: {
        BearerAuth: {
          type: "http",
          scheme: "bearer",
          bearerFormat: "zk_live_*"
        }
      }
    },
    paths: {
      "/api/compile": {
        post: {
          summary: "Compile C Source Code",
          description: "Compiles C source code to target assembly (x86_64, riscv64, wasm32, win64), emits 3-address SSA IR, and optionally generates ZK-SNARK R1CS verification receipts.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  type: "object",
                  required: ["source"],
                  properties: {
                    source: { type: "string", description: "C source code (C99 standard)" },
                    target: { type: "string", enum: ["x86_64", "riscv64", "wasm32", "win64"], default: "x86_64" },
                    opt_level: { type: "string", enum: ["O0", "O1", "O2", "O3", "Os", "Oz"], default: "O2" },
                    prove_zk: { type: "boolean", default: false }
                  }
                }
              }
            }
          },
          responses: {
            "200": { description: "Compilation succeeded with assembly and SSA IR output." },
            "400": { description: "Invalid or empty source." },
            "422": { description: "Parse or syntax error in C code." }
          }
        }
      },
      "/api/optimize": {
        post: {
          summary: "SSA IR Multi-Pass Optimization Engine",
          description: "Runs configurable SSA IR optimization passes (Constant Folding, DCE, GVN/CSE, Z3 SMT Peephole) and returns transformed IR, assembly, and instruction savings metrics.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  type: "object",
                  required: ["source"],
                  properties: {
                    source: { type: "string" },
                    target: { type: "string", default: "x86_64" },
                    opt_level: { type: "string", default: "O2" },
                    passes: {
                      type: "array",
                      items: { type: "string" },
                      example: ["constant_folding", "dead_code_elimination", "gvn_cse", "peephole_z3"]
                    }
                  }
                }
              }
            }
          },
          responses: {
            "200": { description: "Optimization metrics, before/after IR, and clock cycle reduction." }
          }
        }
      },
      "/api/analyze": {
        post: {
          summary: "Static Security & Forensic Audit",
          description: "Audits C code for buffer overflows (CWE-120), null dereferences (CWE-476), integer overflows (CWE-190), division by zero (CWE-369), and ZCC compiler invariants (E-LEARN-015, E-LEARN-020).",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  type: "object",
                  required: ["source"],
                  properties: {
                    source: { type: "string" }
                  }
                }
              }
            }
          },
          responses: {
            "200": { description: "Structured diagnostic list with line/col, severity, and maintainability index." }
          }
        }
      },
      "/api/ast": {
        post: {
          summary: "Abstract Syntax Tree (AST) Export",
          description: "Parses C code into structured AST JSON nodes or Graphviz DOT graph notation for IDE language servers and analysis tooling.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  type: "object",
                  required: ["source"],
                  properties: {
                    source: { type: "string" },
                    format: { type: "string", enum: ["json", "dot"], default: "json" }
                  }
                }
              }
            }
          },
          responses: {
            "200": { description: "Complete AST node tree." }
          }
        }
      },
      "/api/verify": {
        post: {
          summary: "ZK-SNARK Compilation Proof Verification",
          description: "Verifies that an emitted assembly artifact was legitimately derived from source under BN254 bilinear pairing curve constraints.",
          requestBody: {
            required: true,
            content: {
              "application/json": {
                schema: {
                  type: "object",
                  required: ["source"],
                  properties: {
                    source: { type: "string" },
                    target: { type: "string", default: "x86_64" }
                  }
                }
              }
            }
          },
          responses: {
            "200": { description: "ZK verification status and Ethereum smart contract verifier reference." }
          }
        }
      },
      "/api/keys": {
        post: {
          summary: "Generate Sandbox API Key",
          description: "Generates an instant active API key with 5,000 requests/day quota for live development.",
          requestBody: {
            content: {
              "application/json": {
                schema: {
                  type: "object",
                  properties: {
                    email: { type: "string" }
                  }
                }
              }
            }
          },
          responses: {
            "201": { description: "API key generated successfully." }
          }
        },
        get: {
          summary: "Verify API Key & Quota",
          security: [{ BearerAuth: [] }],
          responses: {
            "200": { description: "Key status and remaining quota." },
            "401": { description: "Missing or invalid API key." }
          }
        }
      },
      "/api/targets": {
        get: {
          summary: "List Supported Architecture Targets",
          description: "Returns target specifications, ABI classification, default calling conventions, and register sets.",
          responses: {
            "200": { description: "Catalog of 4 production compile targets." }
          }
        }
      },
      "/api/status": {
        get: {
          summary: "Compiler Cluster Health & SLA",
          description: "Real-time edge cluster status, bootstrap byte-identity convergence (zcc2.s == zcc3.s), and SLA telemetry.",
          responses: {
            "200": { description: "Edge cluster operational status." }
          }
        }
      },
      "/api/quantum": {
        get: {
          summary: "39Q & 40Q Hyperslab Quantum Telemetry",
          description: "Returns verified 549.76B amplitude (39Q) and 1.10T amplitude (40Q) Hilbert space involution metrics, unitary match rates (U†U = I), and Gamps/s throughput.",
          responses: {
            "200": { description: "Quantum telemetry with super-slab hash chains and bandwidth." }
          }
        }
      }
    }
  };

  return new Response(JSON.stringify(spec, null, 2), {
    status: 200,
    headers: CORS_HEADERS
  });
}
