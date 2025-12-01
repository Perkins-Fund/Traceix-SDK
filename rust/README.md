# Traceix Rust SDK

Bring Traceix into your Rust tooling, services, or agents with a type-safe, async-friendly SDK.

If you love `Result<T, E>` and strict types, you’ll feel at home here.

---

### Minimal SDK example

This tiny example:

1. Initializes the SDK (using `TRACEIX_API_KEY` from your environment by default)  
2. Sends a file to Traceix for AI classification  
3. Pretty-prints the JSON response  

```rust
use traceix_sdk::TraceixSdk;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Or: let sdk = TraceixSdk::new(Some("your-api-key-here".to_string()))?;
    let sdk = TraceixSdk::new(None)?;

    let result = sdk.ai_prediction("/path/to/file.exe")?;
    println!("Traceix AI response:\n{result:#}");

    Ok(())
}
