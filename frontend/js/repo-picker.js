"use strict";

// Shared helper: fills a <select> with the repositories the current visitor
// can see (public ones, plus their own when logged in).
async function loadRepositoryPicker(selectElement, onChange, messageElement) {
  const showMessage = (text, ok) => {
    if (!messageElement) {
      return;
    }
    messageElement.textContent = text;
    messageElement.className = ok ? "msg ok" : "msg err";
    messageElement.hidden = !text;
  };

  let repositories = [];

  try {
    const result = await apiRequest("/api/repositories", { method: "GET" });
    repositories = result.data?.repositories || [];
  } catch (error) {
    showMessage(error.message, false);
    return;
  }

  selectElement.innerHTML = "";

  const placeholder = document.createElement("option");
  placeholder.value = "";
  placeholder.textContent = repositories.length
    ? "Select a repository"
    : "No repositories available";
  selectElement.appendChild(placeholder);

  repositories.forEach((repository) => {
    const option = document.createElement("option");
    option.value = String(repository.id);
    option.textContent =
      `${repository.name} (${repository.visibility})`;
    selectElement.appendChild(option);
  });

  const wanted = new URLSearchParams(window.location.search).get("repo");

  if (wanted && repositories.some((r) => String(r.id) === wanted)) {
    selectElement.value = wanted;
    onChange(Number.parseInt(wanted, 10));
  }

  selectElement.addEventListener("change", () => {
    const id = Number.parseInt(selectElement.value, 10);
    onChange(Number.isInteger(id) ? id : null);
  });
}

window.loadRepositoryPicker = loadRepositoryPicker;
