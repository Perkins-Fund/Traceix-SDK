# Traceix Rust SDK

### Minimal SDK example

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {
    // or: let sdk = TraceixSdk::new(Some("your-api-key-here".to_string()))?;
    let sdk = TraceixSdk::new(None)?;

    let result = sdk.ai_prediction("/path/to/file.exe")?;
    println!("{result:#}");

    Ok(())
}
```