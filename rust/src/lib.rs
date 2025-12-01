// src/lib.rs

use reqwest::blocking::{Client};
use reqwest::blocking::multipart;
use reqwest::header::{HeaderMap, HeaderValue, USER_AGENT};
use serde_json::Value;
use std::env;
use std::fs::File;
use std::path::Path;
use std::{fmt, io};

#[derive(Debug)]
pub enum TraceixError {
    NoApiKey,
    InvalidSearchType,
    NoUuidProvided,
    Http(reqwest::Error),
    Io(io::Error),
}

impl fmt::Display for TraceixError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TraceixError::NoApiKey => write!(f, "You did not provide an API key"),
            TraceixError::InvalidSearchType => write!(f, "Search must be of type capa or exif"),
            TraceixError::NoUuidProvided => {
                write!(f, "You did not provide a UUID required by the endpoint")
            }
            TraceixError::Http(e) => write!(f, "HTTP error: {e}"),
            TraceixError::Io(e) => write!(f, "IO error: {e}"),
        }
    }
}

impl std::error::Error for TraceixError {}

impl From<reqwest::Error> for TraceixError {
    fn from(err: reqwest::Error) -> Self {
        TraceixError::Http(err)
    }
}

impl From<io::Error> for TraceixError {
    fn from(err: io::Error) -> Self {
        TraceixError::Io(err)
    }
}

#[derive(Clone, Copy, Debug)]
pub enum SearchType {
    Capa,
    Exif,
}

pub struct TraceixSdk {
    api_key: String,
    base_url: String,
    client: Client,
}

impl TraceixSdk {
    pub const SDK_VERSION: &'static str = "0.0.0.1";

    /// Initialize the SDK. If `api_key` is `None`, it will read TRACEIX_API_KEY from the environment.
    pub fn new(api_key: Option<String>) -> Result<Self, TraceixError> {
        let key = match api_key {
            Some(k) if !k.is_empty() => k,
            _ => env::var("TRACEIX_API_KEY").map_err(|_| TraceixError::NoApiKey)?,
        };

        if key.is_empty() {
            return Err(TraceixError::NoApiKey);
        }

        let client = Client::builder().build()?;

        Ok(Self {
            api_key: key,
            base_url: "https://ai.perkinsfund.org".to_string(),
            client,
        })
    }

    fn build_user_agent(&self) -> String {
        let telemetry_disabled = env::var("TRACEIX_DISABLE_TELEMETRY")
            .map(|v| v == "1")
            .unwrap_or(false);

        let mut ua = format!("Traceix/{}", Self::SDK_VERSION);
        if !telemetry_disabled {
            // Not exactly the same as Python's platform+python_version,
            // but gives OS/arch + crate version.
            let os = std::env::consts::OS;
            let arch = std::env::consts::ARCH;
            let crate_version = env!("CARGO_PKG_VERSION");
            ua.push_str(&format!(" ({}-{} v{})", os, arch, crate_version));
        }

        ua
    }

    fn build_headers(&self) -> HeaderMap {
        let mut headers = HeaderMap::new();

        headers.insert(
            "x-api-key",
            HeaderValue::from_str(&self.api_key).expect("invalid api key for header"),
        );
        headers.insert(
            USER_AGENT,
            HeaderValue::from_str(&self.build_user_agent()).expect("invalid user agent"),
        );

        headers
    }

    fn build_url(&self, path: &str) -> String {
        // Python: f"{self.base_url}/{path}"
        // Their paths start with "/", so this effectively becomes base_url + path.
        format!("{}{}", self.base_url, path)
    }

    fn build_file_form(&self, filename: &str) -> Result<multipart::Form, TraceixError> {
        let file = File::open(filename)?;
        let name = Path::new(filename)
            .file_name()
            .and_then(|s| s.to_str())
            .unwrap_or("file")
            .to_string();

        let part = multipart::Part::reader(file)
            .file_name(name)
            .mime_str("application/octet-stream")
            .map_err(TraceixError::Http)?;

        Ok(multipart::Form::new().part("file", part))
    }

    /// Full upload: prediction, CAPA extraction, and EXIF extraction.
    pub fn full_upload(
        &self,
        filename: &str,
    ) -> Result<(Value, Value, Value), TraceixError> {
        let ai_data = self.ai_prediction(filename)?;
        let capa_status = self.capa_extraction(filename)?;
        let exif_data = self.exif_extraction(filename)?;
        Ok((ai_data, capa_status, exif_data))
    }

    /// Sends a request to the prediction endpoint.
    pub fn ai_prediction(&self, filename: &str) -> Result<Value, TraceixError> {
        let url = self.build_url("/api/traceix/v1/upload");
        let headers = self.build_headers();
        let form = self.build_file_form(filename)?;

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .multipart(form)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// Check the status of a provided UUID.
    pub fn check_status(&self, uuid: &str) -> Result<Value, TraceixError> {
        if uuid.is_empty() {
            return Err(TraceixError::NoUuidProvided);
        }

        let url = self.build_url("/api/v1/traceix/status");
        let headers = self.build_headers();
        let body = serde_json::json!({ "uuid": uuid });

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .json(&body)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// Search by file hash (capa or exif).
    pub fn hash_search(
        &self,
        file_hash: &str,
        search_type: SearchType,
    ) -> Result<Value, TraceixError> {
        let path = match search_type {
            SearchType::Capa => "/api/traceix/v1/capa/search",
            SearchType::Exif => "/api/traceix/v1/exif/search",
        };

        let url = self.build_url(path);
        let mut headers = self.build_headers();
        headers.insert(
            "content-type",
            HeaderValue::from_static("application/json"),
        );

        let body = serde_json::json!({ "sha256": file_hash });

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .json(&body)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// Extract the CAPA capabilities from the filename.
    pub fn capa_extraction(&self, filename: &str) -> Result<Value, TraceixError> {
        let url = self.build_url("/api/traceix/v1/capa");
        let headers = self.build_headers();
        let form = self.build_file_form(filename)?;

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .multipart(form)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// Extract EXIF metadata from the filename.
    pub fn exif_extraction(&self, filename: &str) -> Result<Value, TraceixError> {
        let url = self.build_url("/api/traceix/v1/exif");
        let headers = self.build_headers();
        let form = self.build_file_form(filename)?;

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .multipart(form)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// List all public IPFS datasets currently available.
    ///
    /// Note: in Python you *could* skip the API key, but here we still send headers.
    pub fn list_all_ipfs_datasets(&self) -> Result<Value, TraceixError> {
        let url = self.build_url("/api/traceix/v1/ipfs/listall");
        let headers = self.build_headers();

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// Get a public IPFS dataset by CID.
    pub fn get_public_ipfs_dataset(&self, cid: &str) -> Result<Value, TraceixError> {
        let url = self.build_url("/api/traceix/v1/ipfs/search");
        let headers = self.build_headers();
        let body = serde_json::json!({ "cid": cid });

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .json(&body)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }

    /// Search by file hash to see if the dataset has been uploaded to the public domain.
    pub fn search_ipfs_dataset_by_hash(
        &self,
        file_hash: &str,
    ) -> Result<Value, TraceixError> {
        let url = self.build_url("/api/traceix/v1/ipfs/find");
        let headers = self.build_headers();
        let body = serde_json::json!({ "sha_hash": file_hash });

        let resp = self
            .client
            .post(&url)
            .headers(headers)
            .json(&body)
            .send()?
            .error_for_status()?;

        Ok(resp.json()?)
    }
}
