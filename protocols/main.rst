=====================
libwavy Protocol Guide
=====================

This document provides a comprehensive overview of the custom HTTP protocols and responses used in the `libwavy` streaming and file upload system.

Endpoints
=========

The `libwavy` server exposes a series of endpoints under the `/hls/` and `/toml/` namespaces:

.. list-table:: Supported Endpoints
   :widths: 30 70
   :header-rows: 1

   * - Endpoint
     - Description
   * - ``/hls/clients``
     - Lists all available HLS clients that have uploaded content.
   * - ``/hls/<client_id>/<filename>``
     - Serves `.m3u8`, `.ts`, or `.m4s` files for a given client ID.
   * - ``/hls/audio-info/``
     - Endpoint to query audio metadata uploaded by clients.
   * - ``/hls/ping``
     - Simple ping endpoint that replies with a `200 OK` and `pong`.
   * - ``/toml/upload``
     - Accepts a `.toml` metadata file from the client. (Just an entrypoint to check if server can read TOML files or not)

Status Codes and Responses
==========================

The server uses standard HTTP/1.1 response codes along with custom plain-text messages.

.. list-table:: Status Codes
   :widths: 15 85
   :header-rows: 1

   * - Code
     - Description
   * - ``200 OK``
     - Returned on successful operations such as GET or PING.
   * - ``400 Bad Request``
     - Malformed or invalid HTTP request (e.g., missing headers).
   * - ``401 Authentication Error``
     - Access denied due to failed or missing credentials.
   * - ``404 Not Found``
     - The requested file or resource could not be located.
   * - ``405 Method Not Allowed``
     - HTTP method not supported on the requested endpoint.
   * - ``413 Payload Too Large``
     - The uploaded file exceeds the server limit (100 MiB).
   * - ``500 Internal Server Error``
     - General server-side failure (e.g., file I/O error).

Content Types
=============

These MIME types are used in various HTTP headers:

.. list-table:: MIME Types
   :widths: 30 70
   :header-rows: 1

   * - Content-Type
     - Description
   * - ``application/gzip``
     - Used for compressed `.tar.gz` payload uploads.
   * - ``application/octet-stream``
     - Used for generic binary streaming or transport streams.
   * - ``text/plain``
     - Used for ping and basic server responses.

File Conventions
================

.. list-table:: File Types and Extensions
   :widths: 30 70
   :header-rows: 1

   * - Extension
     - Usage
   * - ``.m3u8``
     - Playlist files (master or variant).
   * - ``.ts``
     - MPEG transport stream segments for lossy audio.
   * - ``.m4s``
     - Fragmented MP4 segments for lossless FLAC.
   * - ``.mp4``
     - Initialization segment for fMP4-based streams.
   * - ``.toml``
     - Configuration/metadata file uploaded by client.
   * - ``.tar.gz``
     - Compressed archive containing HLS files.
   * - ``zst``
     - Optional Zstandard-compressed file.

Server Directories
==================

.. list-table:: Server Paths
   :widths: 40 60
   :header-rows: 1

   * - Path
     - Description
   * - ``/tmp/hls_temp``
     - Temporary directory for in-progress uploads.
   * - ``/tmp/hls_storage``
     - Persistent storage for validated client uploads.
   * - ``/tmp/hls_server.lock``
     - Lock file used to ensure single server instance.
   * - ``server.crt`` / ``server.key``
     - TLS certificate and private key for HTTPS.

Special Constants
=================

.. list-table:: Internal Identifiers
   :widths: 30 70
   :header-rows: 1

   * - Constant
     - Description
   * - ``#EXTM3U``
     - Global header for M3U8 playlists.
   * - ``#EXT-X-VERSION:3``
     - Denotes supported HLS version.
   * - ``#EXT-X-STREAM-INF:``
     - Declares a new variant stream in the master playlist.
   * - ``CODECS="fLaC"``
     - Indicates a lossless FLAC stream inside `.m4s`.

Error Handling Flow
===================

.. code-block:: text

    Client Request ──▶ Server
                        ├── Valid? ──▶ Process File
                        │                └── Success ──▶ 200 OK
                        ├── Missing? ──▶ 404 Not Found
                        ├── Too Large? ──▶ 413 Payload Too Large
                        └── Other Error ──▶ 500 Internal Server Error

.. note::

   All errors are returned with human-readable messages, delimited using ``\r\n\r\n`` for text separation in network layers.

Authentication
==============

Certain endpoints (like `/hls/logs`) require secure authentication. Upon failure, the server returns:

``HTTP/1.1 401 Authentication Error\r\n\r\n``

The authentication scheme can be extended in future releases (e.g., using JWT or token-based mechanisms).

---

