# Traceix Python SDK

Wire Traceix straight into your scripts, pipelines, or notebooks with a clean, Pythonic SDK.

If you can `pip install`, you can ship AI-powered malware analysis.

---

### Minimal SDK example

This example:

1. Initializes the SDK (using `TRACEIX_API_KEY` from your environment)  
2. Uploads a file once  
3. Returns **AI prediction**, **CAPA capabilities**, and **EXIF metadata** in one call  

```python
from traceix_sdk import TraceixSdk


def main():
    filename = "/path/to/my/file"

    # Uses TRACEIX_API_KEY from the environment by default
    sdk = TraceixSdk()

    # full_upload = AI prediction + CAPA extraction + EXIF extraction
    ai_data, capa_status, exif_status = sdk.full_upload(filename)

    print("=== AI PREDICTION ===")
    print(ai_data)

    print("\n=== CAPA EXTRACTION STATUS ===")
    print(capa_status)

    print("\n=== EXIF METADATA EXTRACTION STATUS ===")
    print(exif_status)


if __name__ == "__main__":
    main()
