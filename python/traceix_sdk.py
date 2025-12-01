import os

import requests


# error classes for issues
class NoApiKey(Exception): pass


class InvalidSearchType(Exception): pass


class NoUuidProvided(Exception): pass


class TraceixSdk(object):

    """
    SDK object, simple import and call
    """

    # SDK version
    sdk_version = "0.0.0.1"

    def __init__(self, api_key=None):
        """
        initialize everything
        :param api_key: user API key, if `None` will search ENV for TRACEIX_API_KEY
        """
        if api_key is None:
            self.api_key = os.getenv("TRACEIX_API_KEY")
        else:
            self.api_key = api_key
        if self.api_key is None or self.api_key == "":
            raise NoApiKey("You did not provide an API key")
        self.base_url = "https://ai.perkinsfund.org"

    def __build_user_agent(self):
        """
        builds the user agent string to send with each request. Sends OS and Python version
        :return: str
        """
        # set the env var TRACEIX_DISABLE_TELEMETRY to 1 to disable telemetry data
        telemetry_disabled = os.environ.get("TRACEIX_DISABLE_TELEMETRY") == "1"
        user_agent = f"Traceix/{self.sdk_version}"
        if not telemetry_disabled:
            import platform

            user_agent += f" ({platform.platform()} v{platform.python_version()})"
        return user_agent

    def __build_headers(self):
        """
        builds the headers to send with each request
        :return: dict
        """
        return {
            "x-api-key": self.api_key,
            "user-agent": self.__build_user_agent()
        }

    def __build_url(self, path):
        """
        builds the full URL to send the request to
        :param path: the URL path
        :return: str
        """
        return f"{self.base_url}/{path}"

    @staticmethod
    def __build_file_data(filename):
        """
        builds file POST data to send with the request
        :param filename: the full path to the file
        :return: dict
        """
        fh = open(filename, "rb")
        files = {
            "file": (filename, fh, "application/octet-stream")
        }
        return files

    def full_upload(self, filename):
        """
        performs 3 requests to make a full upload of the file
        :param filename: the full path to the file
        :return: tuple
        """
        ai_data = self.ai_prediction(filename)
        capa_status = self.capa_extraction(filename)
        exif_data = self.exif_extraction(filename)
        return ai_data, capa_status, exif_data

    def ai_prediction(self, filename):
        """
        sends a request to the prediction endpoint
        :param filename: full path to the file
        :return: dict
        """
        url = self.__build_url("/api/traceix/v1/upload")
        headers = self.__build_headers()
        post_data = self.__build_file_data(filename)
        try:
            req = requests.post(url, headers=headers, files=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def check_status(self, uuid):
        """
        check the status of a provided UUID
        :param uuid: the UUID string
        :return: dict
        """
        if uuid is None:
            raise NoUuidProvided("You did not provide a UUID required by the endpoint")
        url = self.__build_url("/api/v1/traceix/status")
        headers = self.__build_headers()
        post_data = {"uuid": uuid}
        try:
            req = requests.post(url, headers=headers, json=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def hash_search(self, file_hash, **kwargs):
        """
        search by file hash on the provided endpoints to get the associated data
        :param file_hash: the hash of the file
        :param kwargs: search_type - the type of search either capa or exif (**default capa)
        :return: dict
        """
        search_type = kwargs.get("search_type", "capa")

        if search_type == "capa":
            url = self.__build_url("/api/traceix/v1/capa/search")
        elif search_type == "exif":
            url = self.__build_url("/api/traceix/v1/exif/search")
        else:
            url = None
        if url is None:
            raise InvalidSearchType("Search must be of type capa or exif")
        headers = self.__build_headers()
        post_data = {"sha256": file_hash}
        headers["content-type"] = "application/json"
        try:
            req = requests.post(url, headers=headers, json=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def capa_extraction(self, filename):
        """
        extract the capabilities from the filename
        :param filename: the full path to the file
        :return: dict
        """
        url = self.__build_url("/api/traceix/v1/capa")
        headers = self.__build_headers()
        post_data = self.__build_file_data(filename)
        try:
            req = requests.post(url, headers=headers, files=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def exif_extraction(self, filename):
        """
        extract the exif metadata from the filename
        :param filename: the full path to the file
        :return: dict
        """
        url = self.__build_url("/api/traceix/v1/exif")
        headers = self.__build_headers()
        post_data = self.__build_file_data(filename)
        try:
            req = requests.post(url, headers=headers, files=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def list_all_ipfs_datasets(self):
        """
        list all the public datasets available currently
        NOTE: API key is not needed for these endpoints
        :return: dict
        """
        url = self.__build_url("/api/traceix/v1/ipfs/listall")
        headers = self.__build_headers()
        try:
            req = requests.post(url, headers=headers)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def get_public_ipfs_dataset(self, cid):
        """
        get the public dataset by the CID
        :param cid: CID string
        :return: dict
        """
        url = self.__build_url("/api/traceix/v1/ipfs/search")
        headers = self.__build_headers()
        post_data = {
            "cid": cid
        }
        try:
            req = requests.post(url, headers=headers, json=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None

    def search_ipfs_dataset_by_hash(self, file_hash):
        """
        search by file hash to see if the dataset has been uploaded to public domain
        :param file_hash: hash string
        :return: dict
        """
        url = self.__build_url("/api/traceix/v1/ipfs/find")
        headers = self.__build_headers()
        post_data = {
            "sha_hash": file_hash
        }
        try:
            req = requests.post(url, headers=headers, json=post_data)
        except:
            req = None
        if req is not None:
            return req.json()
        else:
            return None
