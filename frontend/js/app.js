const byId = (id) =>
  document.getElementById(id);

function showMessage(message, ok) {
  const element = byId("uploadMessage");

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

function getRepositoryId() {
  const value = byId("repositoryId")?.value;
  const repositoryId = Number.parseInt(
    value,
    10
  );

  if (
    !Number.isInteger(repositoryId) ||
    repositoryId <= 0
  ) {
    throw new Error(
      "Select a valid repository."
    );
  }

  return repositoryId;
}

async function uploadAndStage() {
  try {
    const repositoryId = getRepositoryId();
    const path = byId("filePath")?.value.trim();
    const file = byId("fileInput")?.files?.[0];

    if (!path) {
      throw new Error(
        "Enter a repository file path."
      );
    }

    if (!file) {
      throw new Error(
        "Select a file to upload."
      );
    }

    const form = new FormData();

    form.append("path", path);
    form.append("file", file);

    const result = await apiRequest(
      `/api/repositories/${repositoryId}/files`,
      {
        method: "POST",
        body: form,
      }
    );

    showMessage(
      `Uploaded and staged: ${result.data.path}`,
      true
    );
  } catch (error) {
    showMessage(error.message, false);
  }
}

async function createCommit() {
  try {
    const repositoryId = getRepositoryId();
    const message = byId(
      "commitMessage"
    )?.value.trim();

    if (!message) {
      throw new Error(
        "Enter a commit message."
      );
    }

    const result = await apiRequest(
      `/api/repositories/${repositoryId}/commit`,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          message,
        }),
      }
    );

    showMessage(
      result.message,
      true
    );

    byId("commitMessage").value = "";
  } catch (error) {
    showMessage(error.message, false);
  }
}

byId("btnUpload")?.addEventListener(
  "click",
  uploadAndStage
);

byId("btnCreateCommit")?.addEventListener(
  "click",
  createCommit
);