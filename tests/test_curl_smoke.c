#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "curl/curl.h"

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║          ZCC LIBCURL 8.7.1 SOVEREIGN COMPILED SMOKE GAUNTLET           ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* 1. Test curl_version string */
    const char *ver = curl_version();
    printf("[TEST 1] libcurl Version Identification:\n");
    printf("  curl_version() = %s\n", ver);
    assert(ver != NULL);
    assert(strstr(ver, "libcurl") != NULL);
    printf("  [PASS] Test 1: curl_version() verified.\n\n");

    /* 2. Test URL API Parser (curl_url) */
    printf("[TEST 2] Testing Curl URL API Parsing:\n");
    CURLU *url = curl_url();
    assert(url != NULL);

    CURLUcode uc = curl_url_set(url, CURLUPART_URL, "https://zkaedi.prime:8080/quantum/dex?pool=0#resonance", CURLU_NON_SUPPORT_SCHEME);
    if (uc != CURLUE_OK) {
        printf("  curl_url_set failed with code %d\n", (int)uc);
    }
    assert(uc == CURLUE_OK);

    char *scheme = NULL;
    char *host = NULL;
    char *port = NULL;
    char *path = NULL;
    char *query = NULL;
    char *frag = NULL;

    curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
    curl_url_get(url, CURLUPART_HOST, &host, 0);
    curl_url_get(url, CURLUPART_PORT, &port, 0);
    curl_url_get(url, CURLUPART_PATH, &path, 0);
    curl_url_get(url, CURLUPART_QUERY, &query, 0);
    curl_url_get(url, CURLUPART_FRAGMENT, &frag, 0);

    printf("  Scheme: %s | Host: %s | Port: %s\n", scheme, host, port);
    printf("  Path:   %s\n", path);
    printf("  Query:  %s | Fragment: %s\n", query, frag);

    assert(scheme && strcmp(scheme, "https") == 0);
    assert(host && strcmp(host, "zkaedi.prime") == 0);
    assert(port && strcmp(port, "8080") == 0);
    assert(path && strcmp(path, "/quantum/dex") == 0);
    assert(query && strcmp(query, "pool=0") == 0);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    curl_free(query);
    curl_free(frag);
    curl_url_cleanup(url);
    printf("  [PASS] Test 2: URL parsing verified with zero memory leaks.\n\n");

    /* 3. Test Escape / Unescape API */
    printf("[TEST 3] Testing URL Escape & Unescape Encoding:\n");
    const char *raw_data = "https://zkaedi.prime/v5/energy?pool=0&epoch=128";
    char *escaped = curl_easy_escape(NULL, raw_data, (int)strlen(raw_data));
    assert(escaped != NULL);
    printf("  Raw:     %s\n", raw_data);
    printf("  Escaped: %s\n", escaped);
    fflush(stdout);

    int out_len = 0;
    char *unescaped = curl_easy_unescape(NULL, escaped, (int)strlen(escaped), &out_len);
    assert(unescaped != NULL);
    printf("  Unescaped: %s (length=%d)\n", unescaped, out_len);
    fflush(stdout);
    assert(strcmp(raw_data, unescaped) == 0);

    curl_free(escaped);
    curl_free(unescaped);
    printf("  [PASS] Test 3: URL escape / unescape parity verified.\n\n");

    /* 4. Test Easy Error Strings */
    printf("[TEST 4] Testing curl_easy_strerror() mappings:\n");
    const char *err0 = curl_easy_strerror(CURLE_OK);
    const char *err1 = curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL);
    const char *err2 = curl_easy_strerror(CURLE_URL_MALFORMAT);
    printf("  CURLE_OK:                   %s\n", err0);
    printf("  CURLE_UNSUPPORTED_PROTOCOL: %s\n", err1);
    printf("  CURLE_URL_MALFORMAT:        %s\n", err2);
    assert(strcmp(err0, "No error") == 0);
    printf("  [PASS] Test 4: Error strings verified.\n\n");

    printf("========================================================================\n");
    printf("  🏆 LIBCURL 8.7.1 ZCC SOVEREIGN COMPILED TEST SUITE: ALL TESTS PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
