const MINIVCS_API_BASE =
  window.MINIVCS_API_BASE ||
  "http://127.0.0.1:8000";

function getSessionToken() {
  return localStorage.getItem(
    "minivcs_session_token"
  );
}

function getAuthHeaders() {
  const token = getSessionToken();

  return token
    ? {
        Authorization: `Bearer ${token}`,
      }
    : {};
}

async function apiRequest(
  path,
  options = {}
) {
  const headers = {
    ...getAuthHeaders(),
    ...(options.headers || {}),
  };

  const response = await fetch(
    `${MINIVCS_API_BASE}${path}`,
    {
      ...options,
      headers,
    }
  );

  const responseBody =
    await response.json();

  if (!response.ok || !responseBody.ok) {
    throw new Error(
      responseBody.message ||
        "API request failed."
    );
  }

  return responseBody;
}