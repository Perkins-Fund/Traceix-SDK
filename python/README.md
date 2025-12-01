# Traceix Python SDK

### Minimal SDK example

```python
from traceix_sdk import TraceixSdk


def main():
    filename = "/path/to/my/file"
    sdk = TraceixSdk()
    results = sdk.full_upload(filename)
    
    print("AI PREDICTION:")
    print(results[0])
    print("CAPA EXTRACTION STATUS")
    print(results[1]) 
    print("EXIF METADATA EXTRACTION STATUS")
    print(results[2])

    
if __name__ == "__main__":
    main()
```
