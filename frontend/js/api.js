"use strict";

const API_BASE_URL = "http://127.0.0.1:8000";

const SESSION_TOKEN_KEY =
  "minivcs_session_token";

function getSessionToken() {
  return localStorage.getItem(
    SESSION_TOKEN_KEY
  );
}

function saveSessionToken(token) {
  localStorage.setItem(
    SESSION_TOKEN_KEY,
    token
  );
}

function removeSessionToken() {
  localStorage.removeItem(
    SESSION_TOKEN_KEY
  );
}

async function apiRequest(
  path,
  options = {}
) {
  const token =
    getSessionToken();

  const headers = {
    ...(options.headers || {})
  };

  const isFormData =
    options.body instanceof FormData;

  if (
    options.body &&
    !isFormData &&
    !headers["Content-Type"]
  ) {
    headers["Content-Type"] =
      "application/json";
  }

  if (token) {
    headers["Authorization"] =
      `Bearer ${token}`;
  }

  const requestUrl =
    `${API_BASE_URL}${path}`;

  let response;

  try {
    response = await fetch(
      requestUrl,
      {
        ...options,
        headers
      }
    );
  } catch (error) {
    throw new Error(
      "Could not connect to the Flask API. " +
      "Make sure Flask is running at " +
      `${API_BASE_URL}.`
    );
  }

  const contentType =
    response.headers.get(
      "content-type"
    ) || "";

  const responseText =
    await response.text();

  let result = {};

  if (responseText.trim()) {
    if (
      !contentType.includes(
        "application/json"
      )
    ) {
      throw new Error(
        "The server returned HTML instead of JSON. " +
        `Request URL: ${requestUrl}. ` +
        "Check that the request is sent to Flask " +
        "on port 8000, not the frontend server on port 5500."
      );
    }

    try {
      result =
        JSON.parse(responseText);
    } catch (error) {
      throw new Error(
        "The Flask API returned invalid JSON. " +
        `HTTP status: ${response.status}.`
      );
    }
  }

  if (!response.ok) {
    throw new Error(
      result.message ||
      result.error ||
      result.data?.message ||
      `Request failed with HTTP ${response.status}.`
    );
  }

  return result;
}

window.API_BASE_URL =
  API_BASE_URL;

window.apiRequest =
  apiRequest;

window.getSessionToken =
  getSessionToken;

window.saveSessionToken =
  saveSessionToken;

window.removeSessionToken =
  removeSessionToken;