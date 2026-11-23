"use strict";

const API_BASE =
  (typeof window !== "undefined" && window.MINIVCS_API_BASE_URL) || "";

let selectedPublicRepositoryId =
  null;

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

const detailsMessageElement =
  document.getElementById(
    "publicRepositoryDetailsMessage"
  );

const branchesElement =
  document.getElementById(
    "publicRepositoryBranches"
  );

const historyElement =
  document.getElementById(
    "publicRepositoryHistory"
  );

const statusElement =
  document.getElementById(
    "publicRepositoryStatus"
  );

let publicFilesCurrentPath = "";

const publicFilesUpButton =
  document.getElementById(
    "publicFilesUp"
  );

const publicFilesCurrentPathElement =
  document.getElementById(
    "publicFilesCurrentPath"
  );

const publicFilesMessageElement =
  document.getElementById(
    "publicFilesMessage"
  );

const publicFilesListElement =
  document.getElementById(
    "publicFilesList"
  );

const publicFileTitleElement =
  document.getElementById(
    "publicFileTitle"
  );

const publicFileContentElement =
  document.getElementById(
    "publicFileContent"
  );

function showMessage(
  message,
  isError = false
) {
  if (!messageElement) {
    return;
  }

  messageElement.textContent =
    message;

  messageElement.hidden =
    false;

  messageElement.className =
    isError
      ? "msg err"
      : "msg";
}

function showDetailsMessage(
  message,
  isError = false
) {
  if (!detailsMessageElement) {
    return;
  }

  detailsMessageElement.textContent =
    message;

  detailsMessageElement.hidden =
    false;

  detailsMessageElement.className =
    isError
      ? "msg err"
      : "msg ok";
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

function repositoryOwner(repository) {
  return (
    repository.owner_username ??
    repository.owner ??
    repository.username ??
    "Unknown owner"
  );
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

async function readApiResponse(response) {
  const contentType =
    response.headers.get(
      "content-type"
    ) || "";

  const text =
    await response.text();

  if (
    !contentType.includes(
      "application/json"
    )
  ) {
    throw new Error(
      "The API returned a non-JSON response " +
      `(${response.status}).`
    );
  }

  let body;

  try {
    body = JSON.parse(text);
  } catch {
    throw new Error(
      "The API returned invalid JSON " +
      `(${response.status}).`
    );
  }

  if (!response.ok) {
    throw new Error(
      body.message ||
      body.error ||
      body.error_code ||
      `API request failed (${response.status}).`
    );
  }

  return body;
}

function extractRepositories(body) {
  if (
    Array.isArray(
      body.data?.repositories
    )
  ) {
    return body.data.repositories;
  }

  if (
    Array.isArray(body.data)
  ) {
    return body.data;
  }

  if (
    Array.isArray(
      body.repositories
    )
  ) {
    return body.repositories;
  }

  return [];
}

function renderRepositories(
  repositories
) {
  if (!listElement) {
    return;
  }

  listElement.innerHTML =
    "";

  if (!repositories.length) {
    showMessage(
      "No public repositories are available yet."
    );
    return;
  }

  showMessage("");

  for (const repository of repositories) {
    const id =
      repositoryId(repository);

    const card =
      document.createElement(
        "article"
      );

    card.className =
      "repository-card";

    const name =
      document.createElement(
        "h3"
      );

    name.textContent =
      repositoryName(repository);

    const description =
      document.createElement(
        "p"
      );

    description.textContent =
      repository.description?.trim() ||
      "This repository has no description.";

    const owner =
      document.createElement(
        "p"
      );

    owner.textContent =
      `Owner: ${repositoryOwner(
        repository
      )}`;

    const visibility =
      document.createElement(
        "p"
      );

    visibility.textContent =
      `Visibility: ${
        repository.visibility ||
        "public"
      }`;

    const button =
      document.createElement(
        "button"
      );

    button.type =
      "button";

    button.textContent =
      "View details";

    button.addEventListener(
      "click",
      function () {
        loadRepositoryDetails(id);
      }
    );

    card.appendChild(name);
    card.appendChild(description);
    card.appendChild(owner);
    card.appendChild(visibility);
    card.appendChild(button);

    listElement.appendChild(card);
  }
}

async function loadPublicRepositories() {
  try {
    const response =
      await fetch(
        `${API_BASE}/api/repositories`
      );

    const body =
      await readApiResponse(
        response
      );

    const repositories =
      extractRepositories(body);

    const publicRepositories =
      repositories.filter(
        function (repository) {
          return (
            repository.visibility ===
              "public" ||
            !repository.visibility
          );
        }
      );

    renderRepositories(
      publicRepositories
    );
  } catch (error) {
    showMessage(
      error.message,
      true
    );
  }
}

async function loadRepositoryDetails(
  id
) {
  if (!id) {
    showMessage(
      "The repository has no valid ID.",
      true
    );
    return;
  }

  selectedPublicRepositoryId =
    Number(id);

  const url =
    `${API_BASE}/api/repositories/${id}`;

  try {
    const response =
      await fetch(url);

    const body =
      await readApiResponse(
        response
      );

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
      repository.visibility ||
      "public";

    document.getElementById(
      "publicRepositoryDescription"
    ).textContent =
      repository.description?.trim() ||
      "This repository has no description.";

    if (detailsElement) {
      detailsElement.hidden =
        false;

      detailsElement.scrollIntoView({
        behavior: "smooth"
      });
    }

    showDetailsMessage(
      "Repository details loaded."
    );
    publicFilesCurrentPath =
      "";

    clearPublicFileContent();
    updatePublicFilesPath();
    loadPublicFiles("");
  } catch (error) {
    showMessage(
      error.message,
      true
    );
  }
}

async function loadPublicRepositoryData(
  dataType
) {
  if (!selectedPublicRepositoryId) {
    showDetailsMessage(
      "Click View details for a repository first.",
      true
    );
    return;
  }

  let outputElement;

  if (dataType === "branches") {
    outputElement =
      branchesElement;
  } else if (dataType === "history") {
    outputElement =
      historyElement;
  } else if (dataType === "status") {
    outputElement =
      statusElement;
  } else {
    showDetailsMessage(
      "Unsupported public repository data type.",
      true
    );
    return;
  }

  if (outputElement) {
    outputElement.textContent =
      "Loading...";
  }

  const url =
    `${API_BASE}/api/repositories/` +
    `${selectedPublicRepositoryId}/` +
    `${dataType}`;

  try {
    const response =
      await fetch(url);

    const body =
      await readApiResponse(
        response
      );

    const output =
      body.data?.output ??
      "No output was returned.";

    if (outputElement) {
      outputElement.textContent =
        output;
    }

    showDetailsMessage(
      `${dataType} loaded successfully.`
    );
  } catch (error) {
    if (outputElement) {
      outputElement.textContent =
        error.message;
    }

    showDetailsMessage(
      error.message,
      true
    );
  }
}

function attachPublicRepositoryListeners() {
  const branchesButton =
    document.getElementById(
      "loadPublicBranches"
    );

  const historyButton =
    document.getElementById(
      "loadPublicHistory"
    );

  const statusButton =
    document.getElementById(
      "loadPublicStatus"
    );

  if (branchesButton) {
    branchesButton.addEventListener(
      "click",
      function () {
        loadPublicRepositoryData(
          "branches"
        );
      }
    );
  }

  if (historyButton) {
    historyButton.addEventListener(
      "click",
      function () {
        loadPublicRepositoryData(
          "history"
        );
      }
    );
  }

  if (statusButton) {
    statusButton.addEventListener(
      "click",
      function () {
        loadPublicRepositoryData(
          "status"
        );
      }
    );
  }
}
function showPublicFilesMessage(
  message,
  isError = false
) {
  if (!publicFilesMessageElement) {
    return;
  }

  publicFilesMessageElement.textContent =
    message;

  publicFilesMessageElement.hidden =
    false;

  publicFilesMessageElement.className =
    isError
      ? "msg err"
      : "msg ok";
}

function updatePublicFilesPath() {
  if (
    publicFilesCurrentPathElement
  ) {
    publicFilesCurrentPathElement.textContent =
      publicFilesCurrentPath
        ? `/${publicFilesCurrentPath}`
        : "/";
  }

  if (publicFilesUpButton) {
    publicFilesUpButton.disabled =
      !publicFilesCurrentPath;
  }
}

function parentPublicPath(path) {
  const parts =
    path.split("/").filter(Boolean);

  parts.pop();

  return parts.join("/");
}

function encodePathForApi(path) {
  return path
    .split("/")
    .filter(Boolean)
    .map(
      (part) =>
        encodeURIComponent(part)
    )
    .join("/");
}

function clearPublicFileContent() {
  if (publicFileTitleElement) {
    publicFileTitleElement.textContent =
      "Select a file";
  }

  if (publicFileContentElement) {
    publicFileContentElement.textContent =
      "Select a text file from the repository.";
  }
}

async function loadPublicFiles(
  relativePath = ""
) {
  if (!selectedPublicRepositoryId) {
    showPublicFilesMessage(
      "Select a repository first.",
      true
    );
    return;
  }

  publicFilesCurrentPath =
    relativePath;

  updatePublicFilesPath();

  if (publicFilesListElement) {
    publicFilesListElement.textContent =
      "Loading...";
  }

  try {
    const query =
      relativePath
        ? `?path=${encodeURIComponent(
            relativePath
          )}`
        : "";

    const response =
      await fetch(
        `${API_BASE}/api/repositories/` +
        `${selectedPublicRepositoryId}/files` +
        query
      );

    const body =
      await readApiResponse(response);

    renderPublicFiles(
      body.data?.entries || []
    );

    showPublicFilesMessage(
      "Files loaded."
    );
  } catch (error) {
    if (publicFilesListElement) {
      publicFilesListElement.textContent =
        error.message;
    }

    showPublicFilesMessage(
      error.message,
      true
    );
  }
}

function renderPublicFiles(entries) {
  if (!publicFilesListElement) {
    return;
  }

  publicFilesListElement.innerHTML =
    "";

  if (!entries.length) {
    publicFilesListElement.textContent =
      "This directory is empty.";
    return;
  }

  for (const entry of entries) {
    const button =
      document.createElement(
        "button"
      );

    button.type =
      "button";

    button.className =
      "public-file-entry";

    const name =
      document.createElement(
        "span"
      );

    name.className =
      "public-file-entry-name";

    name.textContent =
      entry.type === "directory"
        ? `📁 ${entry.name}`
        : `📄 ${entry.name}`;

    const type =
      document.createElement(
        "span"
      );

    type.className =
      "public-file-entry-type";

    type.textContent =
      entry.type === "directory"
        ? "Directory"
        : `${entry.size ?? 0} bytes`;

    button.appendChild(name);
    button.appendChild(type);

    button.addEventListener(
      "click",
      () => {
        if (entry.type === "directory") {
          loadPublicFiles(entry.path);
        } else {
          loadPublicFileContent(entry.path);
        }
      }
    );

    publicFilesListElement.appendChild(
      button
    );
  }
}

async function loadPublicFileContent(
  filePath
) {
  if (!selectedPublicRepositoryId) {
    showPublicFilesMessage(
      "Select a repository first.",
      true
    );
    return;
  }

  if (publicFileTitleElement) {
    publicFileTitleElement.textContent =
      filePath;
  }

  if (publicFileContentElement) {
    publicFileContentElement.textContent =
      "Loading...";
  }

  try {
    const encodedPath =
      encodePathForApi(filePath);

    const response =
      await fetch(
        `${API_BASE}/api/repositories/` +
        `${selectedPublicRepositoryId}/files/` +
        encodedPath
      );

    const body =
      await readApiResponse(response);

    if (publicFileContentElement) {
      publicFileContentElement.textContent =
        body.data?.content ||
        "";
    }

    showPublicFilesMessage(
      "File loaded."
    );
  } catch (error) {
    if (publicFileContentElement) {
      publicFileContentElement.textContent =
        error.message;
    }

    showPublicFilesMessage(
      error.message,
      true
    );
  }
}
publicFilesUpButton?.addEventListener(
  "click",
  () => {
    loadPublicFiles(
      parentPublicPath(
        publicFilesCurrentPath
      )
    );
  }
);

attachPublicRepositoryListeners();
loadPublicRepositories();