#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl/curl.h"

int main(int argc, char **argv) {
    int mode = 0;
    if (argc > 1 && strcmp(argv[1], "--neg01") == 0) {
        mode = 1;
    }

    /* ---------------------------------------------------------
     * MODE 1: NEG-01 Malformed URL Rejection Path
     * --------------------------------------------------------- */
    if (mode == 1) {
        printf("[MODE: NEG-01] Testing deliberate malformed URL rejection...\n");
        CURLU *u = curl_url();
        if (!u) {
            fprintf(stderr, "FAIL: curl_url returned NULL\n");
            return 101;
        }

        /* Deliberately malformed input: illegal scheme and host syntax */
        CURLUcode rc = curl_url_set(u, CURLUPART_URL, "://invalid..url:port#frag", 0);
        curl_url_cleanup(u);

        if (rc != CURLUE_OK) {
            printf("RESULT: curl_url_set failed deterministically as expected (code=%d)\n", (int)rc);
            return 42; /* Strict expected exit code for NEG-01 */
        }

        fprintf(stderr, "FAIL: malformed URL unexpectedly accepted\n");
        return 1;
    }

    /* ---------------------------------------------------------
     * MODE 0: Positive Control (POS-01) & Offline URL API Gauntlet
     * --------------------------------------------------------- */
    printf("[MODE: POS-01] Executing Deterministic libcurl Offline Gauntlet...\n");

    /* 1. Version Query */
    const char *ver = curl_version();
    if (!ver) {
        fprintf(stderr, "FAIL: curl_version() returned NULL\n");
        return 10;
    }
    printf("1. VERSION: present=YES, contains_libcurl=%s\n",
           strstr(ver, "libcurl") ? "YES" : "NO");

    /* 2. URL API Parsing & Component Extraction */
    CURLU *url = curl_url();
    if (!url) {
        fprintf(stderr, "FAIL: curl_url() returned NULL\n");
        return 20;
    }

    const char *raw_url = "https://zkaedi.prime:8080/quantum/dex?pool=0#resonance";
    CURLUcode uc = curl_url_set(url, CURLUPART_URL, raw_url, CURLU_NON_SUPPORT_SCHEME);
    if (uc != CURLUE_OK) {
        fprintf(stderr, "FAIL: curl_url_set failed code=%d\n", (int)uc);
        curl_url_cleanup(url);
        return 21;
    }

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

    printf("2. URL_PARTS:\n");
    printf("   scheme: %s\n", scheme ? scheme : "NULL");
    printf("   host:   %s\n", host ? host : "NULL");
    printf("   port:   %s\n", port ? port : "NULL");
    printf("   path:   %s\n", path ? path : "NULL");
    printf("   query:  %s\n", query ? query : "NULL");
    printf("   frag:   %s\n", frag ? frag : "NULL");

    int parts_ok = (scheme && strcmp(scheme, "https") == 0) &&
                   (host && strcmp(host, "zkaedi.prime") == 0) &&
                   (port && strcmp(port, "8080") == 0) &&
                   (path && strcmp(path, "/quantum/dex") == 0) &&
                   (query && strcmp(query, "pool=0") == 0) &&
                   (frag && strcmp(frag, "resonance") == 0);

    curl_free(scheme);
    curl_free(host);
    curl_free(port);
    curl_free(path);
    curl_free(query);
    curl_free(frag);
    curl_url_cleanup(url);

    if (!parts_ok) {
        fprintf(stderr, "FAIL: URL components divergence\n");
        return 22;
    }

    /* 3. RFC 3986 Escape / Unescape Roundtrip */
    const char *test_data = "https://zkaedi.prime/v5/energy?pool=0&epoch=128";
    char *escaped = curl_easy_escape(NULL, test_data, (int)strlen(test_data));
    if (!escaped) {
        fprintf(stderr, "FAIL: curl_easy_escape returned NULL\n");
        return 30;
    }

    int unescaped_len = 0;
    char *unescaped = curl_easy_unescape(NULL, escaped, (int)strlen(escaped), &unescaped_len);
    if (!unescaped) {
        fprintf(stderr, "FAIL: curl_easy_unescape returned NULL\n");
        curl_free(escaped);
        return 31;
    }

    int escape_match = (strcmp(test_data, unescaped) == 0) && (unescaped_len == (int)strlen(test_data));
    printf("3. RFC3986: escaped=%s\n", escaped);
    printf("   unescaped_match=%s (len=%d)\n", escape_match ? "YES" : "NO", unescaped_len);

    curl_free(escaped);
    curl_free(unescaped);

    if (!escape_match) {
        fprintf(stderr, "FAIL: escape/unescape roundtrip divergence\n");
        return 32;
    }

    /* 4. Error String Mapping */
    const char *e0 = curl_easy_strerror(CURLE_OK);
    const char *e1 = curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL);
    const char *e2 = curl_easy_strerror(CURLE_URL_MALFORMAT);

    printf("4. ERROR_MAPPINGS:\n");
    printf("   CURLE_OK:                   %s\n", e0 ? e0 : "NULL");
    printf("   CURLE_UNSUPPORTED_PROTOCOL: %s\n", e1 ? e1 : "NULL");
    printf("   CURLE_URL_MALFORMAT:        %s\n", e2 ? e2 : "NULL");

    if (!e0 || strcmp(e0, "No error") != 0) {
        fprintf(stderr, "FAIL: CURLE_OK string mismatch\n");
        return 40;
    }

    printf("GAUNTLET_STATUS: COMPLETE\n");
    return 0;
}
