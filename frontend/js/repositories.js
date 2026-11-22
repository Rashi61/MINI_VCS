"use strict";

const nameInput =
  document.getElementById(
    "repositoryName"
  );

const descriptionInput =
  document.getElementById(
    "repositoryDescription"
  );

const visibilityInput =
  document.getElementById(
    "RepositoryVisibility"
  );

const createButton =
  document.getElementById(
    "btnCreateRepository"
  );

const createMessage =
  document.getElementById(
    "createRepositoryMessage"
  );

const listMessage =
  document.getElementById(
    "repositoryListMessage"
  );

const refreshButton =
  document.getElementById(
    "btnRefreshRepositories"
  );

const repositorySelect =
  document.getElementById(
    "repositorySelect"
  );

const repositoryIdInput =
  document.getElementById(
    "repositoryId"
  );

function showCreateMessage(
  message,
  isError = false
) {
  if (!createMessage) {
    return;
  }

  createMessage.textContent =
    message;

  createMessage.hidden =
    false;

  createMessage.className =
    isError
      ? "msg err"
      : "msg ok";
}

function showListMessage(
  message,
  isError = false
) {
  if (!listMessage) {
    return;
  }

  listMessage.textContent =
    message;

  listMessage.hidden =
    false;

  listMessage.className =
    isError
      ? "msg err"
      : "msg";
}

function getRepositoriesFromResponse(
  result
) {
  if (
    Array.isArray(
      result?.data?.repositories
    )
  ) {
    return result.data.repositories;
  }

  if (
    Array.isArray(result?.data)
  ) {
    return result.data;
  }

  if (
    Array.isArray(
      result?.repositories
    )
  ) {
    return result.repositories;
  }

  return [];
}

function repositoryId(repository) {
  return (
    repository.id ??
    repository.repository_id
  );
}

function repositoryName(repository) {
  return (
    repository.name ??
    repository.repository_name ??
    "Unnamed repository"
  );
}

function repositoryDescription(
  repository
) {
  return (
    repository.description ||
    "No description provided."
  );
}

function saveSelectedRepository(
  repository
) {
  const id =
    repositoryId(repository);

  if (!id) {
    return;
  }

  localStorage.setItem(
    "minivcs_selected_repository_id",
    String(id)
  );

  localStorage.setItem(
    "minivcs_selected_repository",
    JSON.stringify(repository)
  );

  if (repositoryIdInput) {
    repositoryIdInput.value =
      String(id);
  }

  window.dispatchEvent(
    new CustomEvent(
      "repositorySelected",
      {
        detail: repository
      }
    )
  );
}


function populateRepositorySelect(
  repositories
) {
  if (!repositorySelect) {
    return;
  }

  repositorySelect.innerHTML =
    "";

  const emptyOption =
    document.createElement(
      "option"
    );

  emptyOption.value =
    "";

  emptyOption.textContent =
    "Select a repository";

  repositorySelect.appendChild(
    emptyOption
  );

  for (const repository of repositories) {
    const option =
      document.createElement(
        "option"
      );

    const id =
      repositoryId(repository);

    option.value =
      String(id);

    option.textContent =
      `${repositoryName(repository)} (#${id})`;

    option.dataset.repository =
      JSON.stringify(repository);

    repositorySelect.appendChild(
      option
    );
  }

  const savedId =
    localStorage.getItem(
      "minivcs_selected_repository_id"
    );

  if (savedId) {
    repositorySelect.value =
      savedId;

    const selectedOption =
      repositorySelect.selectedOptions[0];

    if (
      selectedOption &&
      selectedOption.dataset.repository
    ) {
      try {
        const savedRepository =
          JSON.parse(
            selectedOption.dataset.repository
          );

        saveSelectedRepository(
          savedRepository
        );
      } catch {
        if (repositoryIdInput) {
          repositoryIdInput.value =
            savedId;
        }
      }
    }
  }
}

async function loadRepositories() {
  try {
    const result =
      await apiRequest(
        "/api/repositories",
        {
          method: "GET"
        }
      );

    const repositories =
      getRepositoriesFromResponse(
        result
      );

    populateRepositorySelect(
      repositories
    );

    if (!repositories.length) {
      showListMessage(
        "No repositories found."
      );
    } else {
      showListMessage(
        `${repositories.length} ` +
        "repository/repositories loaded."
      );
    }
  } catch (error) {
    showListMessage(
      error.message,
      true
    );
  }
}

async function createRepository() {
  const name =
    nameInput?.value
      ?.trim() || "";

  const description =
    descriptionInput?.value
      ?.trim() || "";

  const visibility =
    visibilityInput?.value ||
    "private";

  if (!name) {
    showCreateMessage(
      "Repository name is required.",
      true
    );
    return;
  }

  if (description.length > 500) {
    showCreateMessage(
      "Description must be 500 characters or fewer.",
      true
    );
    return;
  }

  if (
    visibility !== "public" &&
    visibility !== "private"
  ) {
    showCreateMessage(
      "Visibility must be public or private.",
      true
    );
    return;
  }

  if (createButton) {
    createButton.disabled =
      true;
  }

  try {
    const result =
      await apiRequest(
        "/api/repositories",
        {
          method: "POST",
          body: JSON.stringify({
            name: name,
            description: description,
            visibility: visibility
          })
        }
      );

    const createdRepository =
      result.data?.repository ??
      result.data;

    showCreateMessage(
      result.message ||
      "Repository created successfully.",
      false
    );

    if (nameInput) {
      nameInput.value =
        "";
    }

    if (descriptionInput) {
      descriptionInput.value =
        "";
    }

    await loadRepositories();

    if (createdRepository) {
      saveSelectedRepository(
        createdRepository
      );

      if (repositorySelect) {
        repositorySelect.value =
          String(
            repositoryId(
              createdRepository
            )
          );
      }
    }
  } catch (error) {
    showCreateMessage(
      error.message,
      true
    );
  } finally {
    if (createButton) {
      createButton.disabled =
        false;
    }
  }
}

function handleRepositorySelection() {
  if (!repositorySelect) {
    return;
  }

  const option =
    repositorySelect.selectedOptions[0];

  if (
    !option ||
    !option.value
  ) {
    return;
  }

  let repository;

  try {
    repository =
      JSON.parse(
        option.dataset.repository
      );
  } catch {
    repository = {
      id: Number(
        option.value
      ),
      name: option.textContent
    };
  }

  saveSelectedRepository(
    repository
  );
  const selectedDescription =
    document.getElementById(
      "selectedRepositoryDescription"
    );

  if (selectedDescription) {
    selectedDescription.textContent =
      repository.description ||
      "No description provided.";
  }

  showListMessage(
    `Selected repository: ${
      repositoryName(repository)
    }`
  );
}

if (createButton) {
  createButton.addEventListener(
    "click",
    createRepository
  );
}

if (refreshButton) {
  refreshButton.addEventListener(
    "click",
    loadRepositories
  );
}

if (repositorySelect) {
  repositorySelect.addEventListener(
    "change",
    handleRepositorySelection
  );
}

window.loadRepositories =
  loadRepositories;

window.getSelectedRepositoryId =
  function () {
    const id =
      localStorage.getItem(
        "minivcs_selected_repository_id"
      );

    return id
      ? Number(id)
      : null;
  };

loadRepositories();