#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define TRACEIX_SDK_VERSION "0.0.0.1"

typedef enum {
    TRACEIX_OK = 0,
    TRACEIX_NO_API_KEY,
    TRACEIX_INVALID_SEARCH_TYPE,
    TRACEIX_NO_UUID_PROVIDED,
    TRACEIX_HTTP_ERROR,
    TRACEIX_INTERNAL_ERROR
} TraceixError;

typedef enum {
    TRACEIX_SEARCH_CAPA = 0,
    TRACEIX_SEARCH_EXIF = 1
} TraceixSearchType;

typedef struct {
    char *api_key;
    char *base_url;
    char  user_agent[256];
} TraceixSdk;

/* --------- internal helpers --------- */

static char *traceix_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static void traceix_build_user_agent(TraceixSdk *sdk) {
    const char *disable = getenv("TRACEIX_DISABLE_TELEMETRY");
    int telemetry_disabled = (disable && strcmp(disable, "1") == 0);

    if (telemetry_disabled) {
        snprintf(sdk->user_agent, sizeof(sdk->user_agent),
                 "Traceix/%s", TRACEIX_SDK_VERSION);
    } else {
        snprintf(sdk->user_agent, sizeof(sdk->user_agent),
                 "Traceix/%s (C libcurl client)", TRACEIX_SDK_VERSION);
    }
}

static char *traceix_build_url(const TraceixSdk *sdk, const char *path) {
    size_t len_base = strlen(sdk->base_url);
    size_t len_path = strlen(path);
    char *url = (char *)malloc(len_base + len_path + 1);
    if (!url) return NULL;
    memcpy(url, sdk->base_url, len_base);
    memcpy(url + len_base, path, len_path + 1);
    return url;
}

struct TraceixMemory {
    char *data;
    size_t size;
};

static size_t traceix_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct TraceixMemory *mem = (struct TraceixMemory *)userp;

    char *ptr = (char *)realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/* POST with no body */
static TraceixError traceix_post_no_body(
    TraceixSdk *sdk,
    const char *path,
    char **out_json
) {
    *out_json = NULL;
    CURL *curl = curl_easy_init();
    if (!curl) return TRACEIX_INTERNAL_ERROR;

    char *url = traceix_build_url(sdk, path);
    if (!url) {
        curl_easy_cleanup(curl);
        return TRACEIX_INTERNAL_ERROR;
    }

    struct curl_slist *headers = NULL;
    char api_key_hdr[512];

    snprintf(api_key_hdr, sizeof(api_key_hdr), "x-api-key: %s", sdk->api_key);
    headers = curl_slist_append(headers, api_key_hdr);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, sdk->user_agent);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    struct TraceixMemory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, traceix_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl);

    free(url);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (chunk.data) free(chunk.data);
        return TRACEIX_HTTP_ERROR;
    }

    *out_json = chunk.data;
    return TRACEIX_OK;
}

/* POST with JSON body */
static TraceixError traceix_post_json(
    TraceixSdk *sdk,
    const char *path,
    const char *json_body,
    char **out_json
) {
    *out_json = NULL;
    CURL *curl = curl_easy_init();
    if (!curl) return TRACEIX_INTERNAL_ERROR;

    char *url = traceix_build_url(sdk, path);
    if (!url) {
        curl_easy_cleanup(curl);
        return TRACEIX_INTERNAL_ERROR;
    }

    struct curl_slist *headers = NULL;
    char api_key_hdr[512];
    snprintf(api_key_hdr, sizeof(api_key_hdr), "x-api-key: %s", sdk->api_key);
    headers = curl_slist_append(headers, api_key_hdr);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, sdk->user_agent);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);

    struct TraceixMemory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, traceix_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl);

    free(url);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (chunk.data) free(chunk.data);
        return TRACEIX_HTTP_ERROR;
    }

    *out_json = chunk.data;
    return TRACEIX_OK;
}

/* POST multipart with a single file field */
static TraceixError traceix_post_file(
    TraceixSdk *sdk,
    const char *path,
    const char *field_name,
    const char *filename,
    char **out_json
) {
    *out_json = NULL;
    CURL *curl = curl_easy_init();
    if (!curl) return TRACEIX_INTERNAL_ERROR;

    char *url = traceix_build_url(sdk, path);
    if (!url) {
        curl_easy_cleanup(curl);
        return TRACEIX_INTERNAL_ERROR;
    }

    struct curl_slist *headers = NULL;
    char api_key_hdr[512];
    snprintf(api_key_hdr, sizeof(api_key_hdr), "x-api-key: %s", sdk->api_key);
    headers = curl_slist_append(headers, api_key_hdr);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, sdk->user_agent);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    curl_mime *mime;
    curl_mimepart *part;

    mime = curl_mime_init(curl);
    part = curl_mime_addpart(mime);
    curl_mime_name(part, field_name);
    curl_mime_filedata(part, filename);
    curl_mime_type(part, "application/octet-stream");

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    struct TraceixMemory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, traceix_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl);

    free(url);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (chunk.data) free(chunk.data);
        return TRACEIX_HTTP_ERROR;
    }

    *out_json = chunk.data;
    return TRACEIX_OK;
}

/* --------- public API --------- */

TraceixSdk *traceix_sdk_new(const char *api_key, TraceixError *err) {
    if (err) *err = TRACEIX_OK;

    const char *key = api_key;
    if (!key || key[0] == '\0') {
        key = getenv("TRACEIX_API_KEY");
    }
    if (!key || key[0] == '\0') {
        if (err) *err = TRACEIX_NO_API_KEY;
        return NULL;
    }

    TraceixSdk *sdk = (TraceixSdk *)calloc(1, sizeof(TraceixSdk));
    if (!sdk) {
        if (err) *err = TRACEIX_INTERNAL_ERROR;
        return NULL;
    }

    sdk->api_key = traceix_strdup(key);
    sdk->base_url = traceix_strdup("https://ai.perkinsfund.org");

    if (!sdk->api_key || !sdk->base_url) {
        free(sdk->api_key);
        free(sdk->base_url);
        free(sdk);
        if (err) *err = TRACEIX_INTERNAL_ERROR;
        return NULL;
    }

    traceix_build_user_agent(sdk);
    return sdk;
}

void traceix_sdk_free(TraceixSdk *sdk) {
    if (!sdk) return;
    free(sdk->api_key);
    free(sdk->base_url);
    free(sdk);
}

/* ai_prediction: POST /api/traceix/v1/upload */
TraceixError traceix_ai_prediction(
    TraceixSdk *sdk,
    const char *filename,
    char **out_json
) {
    return traceix_post_file(
        sdk,
        "/api/traceix/v1/upload",
        "file",
        filename,
        out_json
    );
}

/* capa_extraction: POST /api/traceix/v1/capa */
TraceixError traceix_capa_extraction(
    TraceixSdk *sdk,
    const char *filename,
    char **out_json
) {
    return traceix_post_file(
        sdk,
        "/api/traceix/v1/capa",
        "file",
        filename,
        out_json
    );
}

/* exif_extraction: POST /api/traceix/v1/exif */
TraceixError traceix_exif_extraction(
    TraceixSdk *sdk,
    const char *filename,
    char **out_json
) {
    return traceix_post_file(
        sdk,
        "/api/traceix/v1/exif",
        "file",
        filename,
        out_json
    );
}

/* check_status: POST /api/v1/traceix/status */
TraceixError traceix_check_status(
    TraceixSdk *sdk,
    const char *uuid,
    char **out_json
) {
    if (!uuid || uuid[0] == '\0') {
        return TRACEIX_NO_UUID_PROVIDED;
    }

    char body[512];
    snprintf(body, sizeof(body), "{\"uuid\":\"%s\"}", uuid);
    return traceix_post_json(
        sdk,
        "/api/v1/traceix/status",
        body,
        out_json
    );
}

/* hash_search: search_type = capa or exif */
TraceixError traceix_hash_search(
    TraceixSdk *sdk,
    const char *file_hash,
    TraceixSearchType search_type,
    char **out_json
) {
    const char *path = NULL;
    switch (search_type) {
        case TRACEIX_SEARCH_CAPA:
            path = "/api/traceix/v1/capa/search";
            break;
        case TRACEIX_SEARCH_EXIF:
            path = "/api/traceix/v1/exif/search";
            break;
        default:
            return TRACEIX_INVALID_SEARCH_TYPE;
    }

    char body[512];
    snprintf(body, sizeof(body), "{\"sha256\":\"%s\"}", file_hash);
    return traceix_post_json(sdk, path, body, out_json);
}

/* list_all_ipfs_datasets: POST /api/traceix/v1/ipfs/listall */
TraceixError traceix_list_all_ipfs_datasets(
    TraceixSdk *sdk,
    char **out_json
) {
    return traceix_post_no_body(
        sdk,
        "/api/traceix/v1/ipfs/listall",
        out_json
    );
}

/* get_public_ipfs_dataset: POST /api/traceix/v1/ipfs/search */
TraceixError traceix_get_public_ipfs_dataset(
    TraceixSdk *sdk,
    const char *cid,
    char **out_json
) {
    char body[512];
    snprintf(body, sizeof(body), "{\"cid\":\"%s\"}", cid);
    return traceix_post_json(
        sdk,
        "/api/traceix/v1/ipfs/search",
        body,
        out_json
    );
}

/* search_ipfs_dataset_by_hash: POST /api/traceix/v1/ipfs/find */
TraceixError traceix_search_ipfs_dataset_by_hash(
    TraceixSdk *sdk,
    const char *file_hash,
    char **out_json
) {
    char body[512];
    snprintf(body, sizeof(body), "{\"sha_hash\":\"%s\"}", file_hash);
    return traceix_post_json(
        sdk,
        "/api/traceix/v1/ipfs/find",
        body,
        out_json
    );
}

/* full_upload: ai_prediction + capa_extraction + exif_extraction */
TraceixError traceix_full_upload(
    TraceixSdk *sdk,
    const char *filename,
    char **out_ai_json,
    char **out_capa_json,
    char **out_exif_json
) {
    *out_ai_json = NULL;
    *out_capa_json = NULL;
    *out_exif_json = NULL;

    TraceixError err;

    err = traceix_ai_prediction(sdk, filename, out_ai_json);
    if (err != TRACEIX_OK) goto fail;

    err = traceix_capa_extraction(sdk, filename, out_capa_json);
    if (err != TRACEIX_OK) goto fail;

    err = traceix_exif_extraction(sdk, filename, out_exif_json);
    if (err != TRACEIX_OK) goto fail;

    return TRACEIX_OK;

fail:
    if (*out_ai_json)   { free(*out_ai_json);   *out_ai_json = NULL; }
    if (*out_capa_json) { free(*out_capa_json); *out_capa_json = NULL; }
    if (*out_exif_json) { free(*out_exif_json); *out_exif_json = NULL; }
    return err;
}
