const logoutButton =
  document.getElementById("btnLogout");

logoutButton?.addEventListener(
  "click",
  async () => {
    const token = localStorage.getItem(
      "minivcs_session_token"
    );

    if (token) {
      await fetch(
        `${window.API_BASE_URL || ""}/api/auth/logout`,
        {
          method: "POST",
          headers: {
            Authorization: `Bearer ${token}`,
          },
        }
      );
    }

    localStorage.removeItem(
      "minivcs_session_token"
    );

    localStorage.removeItem(
      "minivcs_username"
    );

    window.location.href = "login.html";
  }
);