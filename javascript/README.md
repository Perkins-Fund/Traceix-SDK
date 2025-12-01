# Traceix JavaScript SDK

Drop Traceix straight into your Node.js tooling, CLIs, or backends with a tiny, promise-based SDK.

If you can `node example.js`, you can start classifying files with Traceix.

---

### Minimal SDK example

This minimal script:

1. Initializes the SDK (API key from `TRACEIX_API_KEY` by default)  
2. Sends a file to Traceix for AI classification  
3. Logs the JSON response (verdict, metadata, timing, sponsor, etc.)

```javascript
// example.js
const { TraceixSdk } = require("./traceix-sdk");

(async () => {
  try {
    // You can also do: new TraceixSdk("your-api-key-here")
    const sdk = new TraceixSdk();

    const res = await sdk.aiPrediction("/path/to/file.exe");
    console.log("Traceix AI response:");
    console.dir(res, { depth: null });
  } catch (err) {
    console.error("Traceix error:", err);
  }
})();
