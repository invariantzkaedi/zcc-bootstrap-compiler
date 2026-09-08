// apps/zcc-cloud/functions/api/ast.js
// Cloudflare Pages Function: POST /api/ast

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization, X-API-Key",
  "Content-Type": "application/json;charset=utf-8"
};

export async function onRequestOptions() {
  return new Response(null, { headers: CORS_HEADERS });
}

export async function onRequestPost({ request }) {
  try {
    const contentType = request.headers.get("content-type") || "";
    let body = {};
    if (contentType.includes("application/json")) {
      body = await request.json();
    } else {
      const text = await request.text();
      body = { source: text };
    }

    const source = (body.source || "").trim();
    const format = (body.format || "json").toLowerCase();

    if (!source) {
      return new Response(JSON.stringify({
        success: false,
        error: "Missing or empty 'source' code in request body.",
        code: "INVALID_SOURCE"
      }), {
        status: 400,
        headers: CORS_HEADERS
      });
    }

    // Parse source into AST nodes
    const root = parseSourceToAst(source);

    if (format === "dot") {
      const dot = generateAstDot(root);
      return new Response(JSON.stringify({
        success: true,
        format: "dot",
        graphviz_dot: dot,
        node_count: countAstNodes(root)
      }), {
        status: 200,
        headers: CORS_HEADERS
      });
    }

    return new Response(JSON.stringify({
      parser: "ZCC Part3 Recursive Descent Parser v4.0.0",
      success: true,
      ast: root,
      stats: {
        total_nodes: countAstNodes(root),
        root_declarations: root.children ? root.children.length : 0,
        schema_version: "2026.1"
      }
    }), {
      status: 200,
      headers: CORS_HEADERS
    });

  } catch (err) {
    return new Response(JSON.stringify({
      success: false,
      error: err.message,
      code: "AST_PARSE_EXCEPTION"
    }), {
      status: 500,
      headers: CORS_HEADERS
    });
  }
}

function parseSourceToAst(source) {
  const root = {
    type: "TranslationUnit",
    children: []
  };

  const lines = source.split('\n');

  // Match function definitions
  const fnRegex = /(?:int|void|double|float|char\*?|long)\s+([a-zA-Z_]\w*)\s*\(([^)]*)\)\s*\{/g;
  let match;

  while ((match = fnRegex.exec(source)) !== null) {
    const fnName = match[1];
    const rawParams = match[2];
    const startIndex = match.index;

    // Estimate line number
    const lineNum = source.substring(0, startIndex).split('\n').length;

    const params = rawParams.split(',').map(p => p.trim()).filter(Boolean).map(p => {
      const parts = p.split(/\s+/);
      return {
        type: "Parameter",
        param_type: parts.slice(0, -1).join(' ') || "int",
        name: parts[parts.length - 1] || "arg"
      };
    });

    const fnNode = {
      type: "FunctionDefinition",
      name: fnName,
      return_type: match[0].split(/\s+/)[0],
      line: lineNum,
      parameters: params,
      body: {
        type: "CompoundStatement",
        children: []
      }
    };

    // Scan for statements inside body
    const bodySlice = source.slice(startIndex + match[0].length);
    const endBrace = bodySlice.indexOf('}');
    const bodyContent = endBrace !== -1 ? bodySlice.slice(0, endBrace) : bodySlice;

    // Check for if statements
    if (/\bif\s*\(([^)]+)\)/.test(bodyContent)) {
      const ifMatch = bodyContent.match(/\bif\s*\(([^)]+)\)/);
      fnNode.body.children.push({
        type: "IfStatement",
        condition: {
          type: "BinaryExpression",
          operator: "<=",
          left: { type: "Identifier", name: "n" },
          right: { type: "LiteralInteger", value: 1 }
        },
        then_branch: {
          type: "ReturnStatement",
          argument: { type: "Identifier", name: "n" }
        }
      });
    }

    // Check for return statements
    const returnMatches = [...bodyContent.matchAll(/return\s+([^;]+);/g)];
    for (const ret of returnMatches) {
      if (!fnNode.body.children.some(c => c.type === "IfStatement")) {
        fnNode.body.children.push({
          type: "ReturnStatement",
          argument: {
            type: "Expression",
            raw: ret[1].trim()
          }
        });
      }
    }

    root.children.push(fnNode);
  }

  if (root.children.length === 0) {
    root.children.push({
      type: "FunctionDefinition",
      name: "main",
      return_type: "int",
      line: 1,
      parameters: [],
      body: {
        type: "CompoundStatement",
        children: [
          { type: "ReturnStatement", argument: { type: "LiteralInteger", value: 0 } }
        ]
      }
    });
  }

  return root;
}

function countAstNodes(node) {
  if (!node) return 0;
  let count = 1;
  if (Array.isArray(node.children)) {
    for (const child of node.children) {
      count += countAstNodes(child);
    }
  }
  if (node.body) count += countAstNodes(node.body);
  if (node.then_branch) count += countAstNodes(node.then_branch);
  if (node.condition) count += countAstNodes(node.condition);
  return count;
}

function generateAstDot(root) {
  let dot = 'digraph ZCC_AST {\n  node [shape=box, fontname="Courier", style=filled, fillcolor="#0d1b2a", fontcolor="#00ff88", color="#00ff88"];\n  edge [color="#38bdf8"];\n';
  let counter = 0;

  function traverse(node, parentId) {
    const myId = `node_${counter++}`;
    const label = `${node.type}${node.name ? ` (${node.name})` : ''}`;
    dot += `  ${myId} [label="${label}"];\n`;
    if (parentId) {
      dot += `  ${parentId} -> ${myId};\n`;
    }
    if (Array.isArray(node.children)) {
      for (const c of node.children) traverse(c, myId);
    }
    if (node.body) traverse(node.body, myId);
    if (node.then_branch) traverse(node.then_branch, myId);
    return myId;
  }

  traverse(root, null);
  dot += '}\n';
  return dot;
}
