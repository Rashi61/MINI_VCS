const byId = (id) => document.getElementById(id);

const API_BASE =
  window.MINIVCS_API_BASE || "http://127.0.0.1:8000";

function showMessage(message, ok) {
  const element = byId("registerMessage");

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

async function register() {
  const username = byId("username").value.trim();
  const email = byId("email").value.trim();
  const password = byId("password").value;

  if (!username || !email || !password) {
    showMessage(
      "Username, email, and password are required.",
      false
    );
    return;
  }

  try {
    const response = await fetch(
      `${API_BASE}/api/auth/register`,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          username,
          email,
          password,
        }),
      }
    );

    const result = await response.json();

    if (!response.ok || !result.ok) {
      throw new Error(
        result.message || "Registration failed."
      );
    }

    showMessage(
      "Registration successful. Go to login.",
      true
    );
  } catch (error) {
    showMessage(error.message, false);
  }
}

byId("btnRegister").addEventListener(
  "click",
  register
);