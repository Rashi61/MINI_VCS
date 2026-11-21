const registerById = (id) =>
  document.getElementById(id);

function showRegisterMessage(message, ok) {
  const element =
    registerById("registerMessage");

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

async function registerUser() {
  const username =
    registerById("username").value.trim();

  const email =
    registerById("email").value.trim();

  const password =
    registerById("password").value;

  if (!username || !email || !password) {
    showRegisterMessage(
      "Username, email, and password are required.",
      false
    );
    return;
  }

  try {
    const registerResponse = await apiRequest(
      "/api/auth/register",
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

    showRegisterMessage(
      registerResponse.message ||
        "Registration successful. Go to login.",
      true
    );
  } catch (error) {
    showRegisterMessage(
      error.message,
      false
    );
  }
}

registerById("btnRegister")?.addEventListener(
  "click",
  registerUser
);