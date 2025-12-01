# Traceix Javascript SDK

### Minimal SDK example
```javascript
// example.js
const { TraceixSdk } = require("./traceix-sdk");

(async () => {
  try {
    // or: new TraceixSdk("your-api-key-here")
    const sdk = new TraceixSdk();

    const res = await sdk.aiPrediction("/path/to/file.exe");
    console.log(res);
  } catch (err) {
    console.error("Error:", err);
  }
})();
```