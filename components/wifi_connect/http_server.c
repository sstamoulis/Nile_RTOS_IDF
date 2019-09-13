#include "./http_server.h"
#include <esp_log.h>
#include <esp_http_server.h>

const static char *TAG = "http_server";

static esp_err_t http_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, "Hello World!", -1);
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = http_get_handler,
    .user_ctx = NULL,
};

bool httpd_uri_match_glob(const char *pattern, const char *uri,
                          size_t len_uri) {
    const size_t len_pattern = strlen(pattern);
    int px = 0;
    int ux = 0;
    int nextPx = 0;
    int nextUx = 0;
    char c;
    while (px < len_pattern || ux < len_uri) {
        if (px < len_pattern) {
            c = pattern[px];
            switch (c) {
                default: // ordinary character
                    if (ux < len_uri && uri[ux] == c) {
                        px++;
                        ux++;
                        continue;
                    }
                    break;
                case '?': // single-character wildcard
                    if (ux < len_uri) {
                        px++;
                        ux++;
                        continue;
                    }
                    break;
                case '*': // zero-or-more-character wildcard
                    // Try to match at ux.
                    // If that doesn't work out,
                    // restart at ux+1 next.
                    nextPx = px;
                    nextUx = ux + 1;
                    px++;
                    continue;
            }
        }
        // Mismatch. Maybe restart.
        if (0 < nextUx && nextUx <= len_uri) {
            px = nextPx;
            ux = nextUx;
            continue;
        }
        ESP_LOGI(TAG, "not matching: '%s' - '%s'", pattern, uri);
        return false;
    }
    // Matched all of pattern to all of uri. Success.
    ESP_LOGI(TAG, "matching: '%s' - '%s'", pattern, uri);
    return true;
}

void http_server_start(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_glob;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    httpd_register_uri_handler(server, &hello);
}