"use strict";

(function () {
  const status = document.getElementById("apiStatus");
  const loggedIn = Boolean(
    typeof getSessionToken === "function" && getSessionToken()
  );

  if (loggedIn) {
    document.getElementById("homeLogin").hidden = true;
    document.getElementById("homeRegister").hidden = true;
    document.getElementById("homeRepos").hidden = false;
    document.getElementById("welcome").textContent =
      "Welcome back. You are signed in.";
  }

  fetch(`${API_BASE_URL}/api/health`)
    .then((response) => response.json())
    .then((body) => {
      status.textContent = body.ok
        ? "Server status: online"
        : "Server status: problem detected";
    })
    .catch(() => {
      status.textContent = "Server status: unreachable";
    });
})();
