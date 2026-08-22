/**
 * ZCC GitHub Redirect Worker
 * Routes all requests to the ZCC Bootstrap Compiler GitHub repository.
 * Deploy: npx wrangler deploy
 */

export default {
  async fetch(request) {
    const url = new URL(request.url);
    const path = url.pathname;

    const GITHUB_BASE = "https://github.com/invariantzkaedi/zcc-bootstrap-compiler";

    // Route map — specific paths get targeted redirects
    const routes = {
      "/actions":     `${GITHUB_BASE}/actions`,
      "/issues":      `${GITHUB_BASE}/issues`,
      "/pulls":       `${GITHUB_BASE}/pulls`,
      "/releases":    `${GITHUB_BASE}/releases`,
      "/wiki":        `${GITHUB_BASE}/wiki`,
      "/tree":        `${GITHUB_BASE}/tree/main`,
      "/blob":        `${GITHUB_BASE}/blob/main`,
      "/explorer":    `${GITHUB_BASE}/blob/main/zcc_compiler_explorer.html`,
      "/playground":  `${GITHUB_BASE}/blob/main/zcc_compiler_explorer.html`,
      "/observatory": `${GITHUB_BASE}/blob/main/GODS_EYE_OBSERVATORY.html`,
    };

    // Check for exact route match
    if (routes[path]) {
      return Response.redirect(routes[path], 302);
    }

    // Pass through sub-paths (e.g. /blob/main/part5.c -> github.com/.../blob/main/part5.c)
    if (path !== "/" && path.length > 1) {
      return Response.redirect(`${GITHUB_BASE}${path}`, 302);
    }

    // Root "/" -> repo landing page
    return Response.redirect(GITHUB_BASE, 302);
  },
};
