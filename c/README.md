# Traceix C SDK

Bring Traceix straight into your low level tooling, sandboxes, or agents with a lightweight C SDK built on top of `libcurl`.

If you can compile a C program, you can talk to Traceix.

---

### Minimal SDK example

Below is a tiny end-to-end example that:

1. Initializes the SDK (using your `TRACEIX_API_KEY` env var)  
2. Uploads a file for AI classification  
3. Prints the raw JSON response  
4. Cleans everything up

```c
#include <stdio.h>
#include <curl/curl.h>

/* forward-declare from traceix_sdk.c */
TraceixSdk *traceix_sdk_new(const char *api_key, TraceixError *err);
void traceix_sdk_free(TraceixSdk *sdk);
TraceixError traceix_ai_prediction(TraceixSdk *sdk, const char *filename, char **out_json);

int main(void) {
    // Initialize global libcurl state
    curl_global_init(CURL_GLOBAL_DEFAULT);

    TraceixError err;
    // Pass NULL to use the TRACEIX_API_KEY environment variable
    TraceixSdk *sdk = traceix_sdk_new(NULL, &err);
    if (!sdk) {
        fprintf(stderr, "Traceix init error: %d\n", err);
        curl_global_cleanup();
        return 1;
    }

    char *json = NULL;
    err = traceix_ai_prediction(sdk, "/path/to/file.exe", &json);
    if (err == TRACEIX_OK) {
        printf("Traceix AI response:\n%s\n", json);
        free(json);
    } else {
        fprintf(stderr, "traceix_ai_prediction error: %d\n", err);
    }

    // Clean up SDK + libcurl
    traceix_sdk_free(sdk);
    curl_global_cleanup();
    return 0;
}
