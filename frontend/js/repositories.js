const repositorySelect =
  document.getElementById(
    "repositorySelect"
  );

const repositoryIdInput =
  document.getElementById(
    "repositoryId"
  );

const repositoryListMessage =
  document.getElementById(
    "repositoryListMessage"
  );

function showRepositoryMessage(
  message,
  ok
) {
  if (!repositoryListMessage) {
    return;
  }

  repositoryListMessage.textContent = message;
  repositoryListMessage.hidden = false;
  repositoryListMessage.className = ok
    ? "msg ok"
    : "msg err";
}

function selectRepository(repository) {
  const option = document.createElement(
    "option"
  );

  option.value = repository.id;

  option.textContent =
    `${repository.name} ` +
    `(${repository.visibility})`;

  repositorySelect.appendChild(option);
}

async function loadRepositories() {
  if (!repositorySelect) {
    return;
  }

  repositorySelect.innerHTML = "";

  const placeholder =
    document.createElement("option");

  placeholder.value = "";
  placeholder.textContent =
    "Select a repository";

  repositorySelect.appendChild(
    placeholder
  );

  try {
    const result = await apiRequest(
      "/api/repositories"
    );

    const repositories =
      result.data.repositories || [];

    repositories.forEach(
      selectRepository
    );

    showRepositoryMessage(
      `${repositories.length} repository(s) loaded.`,
      true
    );
  } catch (error) {
    showRepositoryMessage(
      error.message,
      false
    );
  }
}

repositorySelect?.addEventListener(
  "change",
  () => {
    repositoryIdInput.value =
      repositorySelect.value;

    window.dispatchEvent(
      new Event("repositorySelected")
    );
  }
);

document
  .getElementById("btnRefreshRepositories")
  ?.addEventListener(
    "click",
    loadRepositories
  );

function showCreateRepositoryMessage(
  message,
  ok
) {
  const element = document.getElementById(
    "createRepositoryMessage"
  );

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

async function createRepositoryFromForm() {
  const nameElement =
    document.getElementById(
      "newRepositoryName"
    );

  const visibilityElement =
    document.getElementById(
      "newRepositoryVisibility"
    );

  const name = nameElement.value.trim();
  const visibility = visibilityElement.value;

  if (!name) {
    showCreateRepositoryMessage(
      "Repository name is required.",
      false
    );
    return;
  }

  try {
    const result = await apiRequest(
      "/api/repositories",
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          name,
          visibility,
        }),
      }
    );

    const repository =
      result.data;

    showCreateRepositoryMessage(
      `Repository created: ${repository.name}`,
      true
    );

    nameElement.value = "";

    await loadRepositories();

    const repositorySelect =
      document.getElementById(
        "repositorySelect"
      );

    const repositoryIdInput =
      document.getElementById(
        "repositoryId"
      );

    if (repositorySelect) {
      repositorySelect.value =
        String(repository.id);
    }

    if (repositoryIdInput) {
      repositoryIdInput.value =
        String(repository.id);
    }
    window.dispatchEvent(
    new Event("repositorySelected")
    );
  } catch (error) {
    showCreateRepositoryMessage(
      error.message,
      false
    );
  }
}

document
  .getElementById("btnCreateRepository")
  ?.addEventListener(
    "click",
    createRepositoryFromForm
  );

loadRepositories();