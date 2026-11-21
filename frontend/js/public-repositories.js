const API_BASE =
  "http://127.0.0.1:8000";

const messageElement =
  document.getElementById(
    "publicRepositoryMessage"
  );

const listElement =
  document.getElementById(
    "publicRepositoryList"
  );

const detailsElement =
  document.getElementById(
    "publicRepositoryDetails"
  );

function showMessage(message, isError = false) {
  messageElement.textContent = message;
  messageElement.className = isError
    ? "msg err"
    : "msg";
}

function repositoryId(repository) {
  return repository.id ??
    repository.repository_id;
}

function repositoryName(repository) {
  return repository.name ??
    repository.repository_name ??
    "Unnamed repository";
}

function repositoryOwner(repository) {
  return repository.owner_username ??
    repository.owner ??
    repository.username ??
    "Unknown owner";
}

function renderRepositories(repositories) {
  listElement.innerHTML = "";

  if (!repositories.length) {
    showMessage(
      "No public repositories are available yet."
    );
    return;
  }

  showMessage("");

  for (const repository of repositories) {
    const id = repositoryId(repository);
    const card = document.createElement("article");

    card.className = "repository-card";

    card.innerHTML = `
      <h3>${escapeHtml(
        repositoryName(repository)
      )}</h3>
      <p>
        Owner:
        ${escapeHtml(
          repositoryOwner(repository)
        )}
      </p>
      <p>
        Visibility:
        ${escapeHtml(
          repository.visibility ?? "public"
        )}
      </p>
      <button type="button">
        View details
      </button>
    `;

    card
      .querySelector("button")
      .addEventListener(
        "click",
        () => loadRepositoryDetails(id)
      );

    listElement.appendChild(card);
  }
}

async function loadPublicRepositories() {
  try {
    const response = await fetch(
      `${API_BASE}/api/repositories`
    );

    const body = await response.json();

    if (!response.ok) {
      throw new Error(
        body.message ||
        "Could not load public repositories."
      );
    }

    const repositories =
      body.data?.repositories ??
      body.data ??
      body.repositories ??
      [];

    const publicRepositories =
      repositories.filter(
        (repository) =>
          repository.visibility === "public" ||
          !repository.visibility
      );

    renderRepositories(publicRepositories);
  } catch (error) {
    showMessage(
      error.message,
      true
    );
  }
}

async function loadRepositoryDetails(id) {
  if (!id) {
    showMessage(
      "The repository has no valid ID.",
      true
    );
    return;
  }

  try {
    const response = await fetch(
      `${API_BASE}/api/repositories/${id}`
    );

    const body = await response.json();

    if (!response.ok) {
      throw new Error(
        body.message ||
        "Could not load repository details."
      );
    }

    const repository =
      body.data?.repository ??
      body.data;

    if (
      repository.visibility &&
      repository.visibility !== "public"
    ) {
      throw new Error(
        "This repository is not public."
      );
    }

    document.getElementById(
      "publicRepositoryName"
    ).textContent =
      repositoryName(repository);

    document.getElementById(
      "publicRepositoryOwner"
    ).textContent =
      repositoryOwner(repository);

    document.getElementById(
      "publicRepositoryVisibility"
    ).textContent =
      repository.visibility ?? "public";

    document.getElementById(
      "publicRepositoryDescription"
    ).textContent =
      repository.description ||
      "No description provided.";

    detailsElement.hidden = false;
    detailsElement.scrollIntoView({
      behavior: "smooth"
    });
  } catch (error) {
    showMessage(
      error.message,
      true
    );
  }
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

loadPublicRepositories();