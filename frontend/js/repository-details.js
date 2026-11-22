const detailsById = (id) =>
  document.getElementById(id);

function selectedRepositoryId() {
  const value =
    detailsById("repositoryId")?.value;

  const repositoryId =
    Number.parseInt(value, 10);

  if (
    !Number.isInteger(repositoryId) ||
    repositoryId <= 0
  ) {
    throw new Error(
      "Select a repository first."
    );
  }

  return repositoryId;
}

function showDetailsMessage(
  message,
  ok
) {
  const element =
    detailsById("detailsMessage");

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

async function loadRepositoryDetails() {
  try {
    const repositoryId =
      selectedRepositoryId();

    const [
      statusResult,
      historyResult,
    ] = await Promise.all([
      apiRequest(
        `/api/repositories/${repositoryId}/status`
      ),
      apiRequest(
        `/api/repositories/${repositoryId}/history`
      ),
    ]);

    detailsById(
      "repositoryStatus"
    ).textContent =
      statusResult.data.output ||
      "No status output.";

    detailsById(
      "repositoryHistory"
    ).textContent =
      historyResult.data.output ||
      "No history output.";

    showDetailsMessage(
      "Repository information refreshed.",
      true
    );
  } catch (error) {
    detailsById(
      "repositoryStatus"
    ).textContent = "Unavailable.";

    detailsById(
      "repositoryHistory"
    ).textContent = "Unavailable.";

    showDetailsMessage(
      error.message,
      false
    );
  }
}

document
  .getElementById("btnRefreshDetails")
  ?.addEventListener(
    "click",
    loadRepositoryDetails
  );

document
  .getElementById("repositorySelect")
  ?.addEventListener(
    "change",
    () => {
      window.setTimeout(
        loadRepositoryDetails,
        0
      );
    }
  );