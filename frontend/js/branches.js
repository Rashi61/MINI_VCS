const branchById = (id) =>
  document.getElementById(id);

function branchRepositoryId() {
  const value =
    branchById("repositoryId")?.value;

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

function showBranchMessage(
  message,
  ok
) {
  const element =
    branchById("branchMessage");

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

function parseBranchOutput(output) {
  return output
    .split(/\r?\n/)
    .map((line) =>
      line
        .replace(/^[*\-\s]+/, "")
        .trim()
    )
    .filter(
      (branch) =>
        branch.length > 0 &&
        !branch.startsWith("Branch")
    );
}

async function loadBranches() {
  const repositoryId =
    branchRepositoryId();

  const result = await apiRequest(
    `/api/repositories/${repositoryId}/branches`
  );

  const select =
    branchById("branchSelect");

  select.innerHTML = "";

  const placeholder =
    document.createElement("option");

  placeholder.value = "";
  placeholder.textContent =
    "Select a branch";

  select.appendChild(placeholder);

  const branches = parseBranchOutput(
    result.data.output || ""
  );

  branches.forEach((branch) => {
    const option =
      document.createElement("option");

    option.value = branch;
    option.textContent = branch;

    select.appendChild(option);
  });

  showBranchMessage(
    `${branches.length} branch(es) loaded.`,
    true
  );
}

async function createBranch() {
  try {
    const repositoryId =
      branchRepositoryId();

    const name =
      branchById("newBranchName")
        ?.value.trim();

    if (!name) {
      throw new Error(
        "Enter a branch name."
      );
    }

    const result = await apiRequest(
      `/api/repositories/${repositoryId}/branches`,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          name,
        }),
      }
    );

    branchById("newBranchName").value = "";

    showBranchMessage(
      result.message,
      true
    );

    await loadBranches();
  } catch (error) {
    showBranchMessage(
      error.message,
      false
    );
  }
}

async function switchBranch() {
  try {
    const repositoryId =
      branchRepositoryId();

    const name =
      branchById("branchSelect")?.value;

    if (!name) {
      throw new Error(
        "Select a branch."
      );
    }

    const result = await apiRequest(
      `/api/repositories/${repositoryId}/branches/switch`,
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          name,
        }),
      }
    );

    showBranchMessage(
      result.message,
      true
    );

    window.dispatchEvent(
      new Event("repositoryChanged")
    );
  } catch (error) {
    showBranchMessage(
      error.message,
      false
    );
  }
}

branchById("btnCreateBranch")
  ?.addEventListener(
    "click",
    createBranch
  );

branchById("btnSwitchBranch")
  ?.addEventListener(
    "click",
    switchBranch
  );

window.addEventListener(
  "repositorySelected",
  () => {
    loadBranches().catch((error) => {
      showBranchMessage(
        error.message,
        false
      );
    });
  }
);