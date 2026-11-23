const loginById = (id) =>
  document.getElementById(id);

function showLoginMessage(message, ok) {
  const element =
    loginById("loginMessage");

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

async function loginUser() {
  const username =
    loginById("username").value.trim();

  const password =
    loginById("password").value;

  if (!username || !password) {
    showLoginMessage(
      "Username and password are required.",
      false
    );
    return;
  }

  try {
    const loginResponse = await apiRequest(
      "/api/auth/login",
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          username,
          password,
        }),
      }
    );

    localStorage.setItem(
      "minivcs_session_token",
      loginResponse.data.token
    );

    localStorage.setItem(
      "minivcs_username",
      loginResponse.data.username
    );

    showLoginMessage(
      "Login successful. Opening repositories...",
      true
    );

    window.setTimeout(() => {
      window.location.href =
        "repositories.html";
    }, 500);
  } catch (error) {
    showLoginMessage(
      error.message,
      false
    );
  }
}

loginById("btnLogin")?.addEventListener(
  "click",
  loginUser
);

loginById("password")?.addEventListener(
  "keydown",
  (event) => {
    if (event.key === "Enter") {
      loginUser();
    }
  }
);