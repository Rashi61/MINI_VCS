# MiniVCS Frontend and Server API Contract

## 1. Purpose

This document defines the HTTP API between the MiniVCS frontend and the Python server layer.

The browser communicates only with the Python HTTP API. The server validates authentication and permissions, maps repository IDs to controlled server directories, and invokes the MiniVCS backend through safe operations.

The frontend must not send arbitrary filesystem paths to the server.

## 2. Development server

Default development address:

```text
http://127.0.0.1:8000