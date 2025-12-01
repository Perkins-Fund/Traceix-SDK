const fs = require("fs");
const os = require("os");
const axios = require("axios");
const FormData = require("form-data");

// Error classes
class NoApiKey extends Error {
  constructor(message = "You did not provide an API key") {
    super(message);
    this.name = "NoApiKey";
  }
}

class InvalidSearchType extends Error {
  constructor(message = "Search must be of type capa or exif") {
    super(message);
    this.name = "InvalidSearchType";
  }
}

class NoUuidProvided extends Error {
  constructor(message = "You did not provide a UUID required by the endpoint") {
    super(message);
    this.name = "NoUuidProvided";
  }
}

class TraceixSdk {
  /**
   * SDK object, simple import and call
   */
  static sdkVersion = "0.0.0.1";

  /**
   * Initialize everything
   * @param {string|null} apiKey - user API key, if null will search ENV for TRACEIX_API_KEY
   */
  constructor(apiKey = null) {
    if (apiKey == null) {
      this.apiKey = process.env.TRACEIX_API_KEY || "";
    } else {
      this.apiKey = apiKey;
    }

    if (!this.apiKey || this.apiKey === "") {
      throw new NoApiKey();
    }

    this.baseUrl = "https://ai.perkinsfund.org";
  }

  /**
   * Builds the user agent string to send with each request.
   * Sends OS and Node version unless TRACEIX_DISABLE_TELEMETRY=1
   * @return {string}
   * @private
   */
  _buildUserAgent() {
    const telemetryDisabled = process.env.TRACEIX_DISABLE_TELEMETRY === "1";
    let userAgent = `Traceix/${TraceixSdk.sdkVersion}`;

    if (!telemetryDisabled) {
      const platform = `${os.type()} ${os.release()}`;
      const nodeVersion = process.version;
      userAgent += ` (${platform} v${nodeVersion})`;
    }

    return userAgent;
  }

  /**
   * Builds the headers to send with each request
   * @return {Object}
   * @private
   */
  _buildHeaders() {
    return {
      "x-api-key": this.apiKey,
      "user-agent": this._buildUserAgent(),
    };
  }

  /**
   * Builds the full URL to send the request to
   * @param {string} path
   * @return {string}
   * @private
   */
  _buildUrl(path) {
    if (path.startsWith("/")) {
      return `${this.baseUrl}${path}`;
    }
    return `${this.baseUrl}/${path}`;
  }

  /**
   * Builds file POST data to send with the request
   * @param {string} filename
   * @return {FormData}
   * @private
   */
  _buildFileData(filename) {
    const form = new FormData();
    form.append("file", fs.createReadStream(filename), {
      filename,
      contentType: "application/octet-stream",
    });
    return form;
  }

  /**
   * Performs 3 requests to make a full upload of the file
   * @param {string} filename - full path to the file
   * @return {Promise<[any, any, any]>}
   */
  async fullUpload(filename) {
    const aiData = await this.aiPrediction(filename);
    const capaStatus = await this.capaExtraction(filename);
    const exifData = await this.exifExtraction(filename);
    return [aiData, capaStatus, exifData];
  }

  /**
   * Sends a request to the prediction endpoint
   * @param {string} filename - full path to the file
   * @return {Promise<any|null>}
   */
  async aiPrediction(filename) {
    const url = this._buildUrl("/api/traceix/v1/upload");
    const headers = this._buildHeaders();
    const form = this._buildFileData(filename);

    try {
      const resp = await axios.post(url, form, {
        headers: {
          ...headers,
          ...form.getHeaders(),
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * Check the status of a provided UUID
   * @param {string|null} uuid
   * @return {Promise<any|null>}
   */
  async checkStatus(uuid) {
    if (uuid == null) {
      throw new NoUuidProvided();
    }

    const url = this._buildUrl("/api/v1/traceix/status");
    const headers = this._buildHeaders();
    const postData = { uuid };

    try {
      const resp = await axios.post(url, postData, {
        headers: {
          ...headers,
          "content-type": "application/json",
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * Search by file hash on the provided endpoints to get the associated data
   * @param {string} fileHash - the hash of the file
   * @param {Object} options - { search_type: "capa" | "exif" }
   * @return {Promise<any|null>}
   */
  async hashSearch(fileHash, options = {}) {
    const searchType = options.search_type || "capa";
    let url;

    if (searchType === "capa") {
      url = this._buildUrl("/api/traceix/v1/capa/search");
    } else if (searchType === "exif") {
      url = this._buildUrl("/api/traceix/v1/exif/search");
    } else {
      url = null;
    }

    if (!url) {
      throw new InvalidSearchType();
    }

    const headers = this._buildHeaders();
    const postData = { sha256: fileHash };

    try {
      const resp = await axios.post(url, postData, {
        headers: {
          ...headers,
          "content-type": "application/json",
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * Extract the capabilities from the filename
   * @param {string} filename
   * @return {Promise<any|null>}
   */
  async capaExtraction(filename) {
    const url = this._buildUrl("/api/traceix/v1/capa");
    const headers = this._buildHeaders();
    const form = this._buildFileData(filename);

    try {
      const resp = await axios.post(url, form, {
        headers: {
          ...headers,
          ...form.getHeaders(),
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * Extract the exif metadata from the filename
   * @param {string} filename
   * @return {Promise<any|null>}
   */
  async exifExtraction(filename) {
    const url = this._buildUrl("/api/traceix/v1/exif");
    const headers = this._buildHeaders();
    const form = this._buildFileData(filename);

    try {
      const resp = await axios.post(url, form, {
        headers: {
          ...headers,
          ...form.getHeaders(),
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * List all the public datasets available currently
   * NOTE: API key is not needed for these endpoints (but we still send headers, like Python)
   * @return {Promise<any|null>}
   */
  async listAllIpfsDatasets() {
    const url = this._buildUrl("/api/traceix/v1/ipfs/listall");
    const headers = this._buildHeaders();

    try {
      const resp = await axios.post(url, null, { headers });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * Get the public dataset by the CID
   * @param {string} cid
   * @return {Promise<any|null>}
   */
  async getPublicIpfsDataset(cid) {
    const url = this._buildUrl("/api/traceix/v1/ipfs/search");
    const headers = this._buildHeaders();
    const postData = { cid };

    try {
      const resp = await axios.post(url, postData, {
        headers: {
          ...headers,
          "content-type": "application/json",
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }

  /**
   * Search by file hash to see if the dataset has been uploaded to public domain
   * @param {string} fileHash
   * @return {Promise<any|null>}
   */
  async searchIpfsDatasetByHash(fileHash) {
    const url = this._buildUrl("/api/traceix/v1/ipfs/find");
    const headers = this._buildHeaders();
    const postData = { sha_hash: fileHash };

    try {
      const resp = await axios.post(url, postData, {
        headers: {
          ...headers,
          "content-type": "application/json",
        },
      });
      return resp.data;
    } catch (e) {
      return null;
    }
  }
}

module.exports = {
  TraceixSdk,
  NoApiKey,
  InvalidSearchType,
  NoUuidProvided,
};
