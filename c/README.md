# Traceix C SDK

### Minimal SDK example
```c
#include <stdio.h>
#include <curl/curl.h>

/* forward-declare from traceix_sdk.c */
TraceixSdk *traceix_sdk_new(const char *api_key, TraceixError *err);
void traceix_sdk_free(TraceixSdk *sdk);
TraceixError traceix_ai_prediction(TraceixSdk *sdk, const char *filename, char **out_json);

int main(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    TraceixError err;
    TraceixSdk *sdk = traceix_sdk_new(NULL, &err);  // uses TRACEIX_API_KEY env
    if (!sdk) {
        fprintf(stderr, "init error: %d\n", err);
        return 1;
    }

    char *json = NULL;
    err = traceix_ai_prediction(sdk, "/path/to/file.exe", &json);
    if (err == TRACEIX_OK) {
        printf("Response:\n%s\n", json);
        free(json);
    } else {
        fprintf(stderr, "ai_prediction error: %d\n", err);
    }

    traceix_sdk_free(sdk);
    curl_global_cleanup();
    return 0;
}
```
