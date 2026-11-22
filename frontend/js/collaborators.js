"use strict";

const collaboratorUsername =
  document.getElementById(
    "collaboratorUsername"
  );

const collaboratorRole =
  document.getElementById(
    "collaboratorRole"
  );

const addCollaboratorButton =
  document.getElementById(
    "btnAddCollaborator"
  );

const collaboratorMessage =
  document.getElementById(
    "collaboratorMessage"
  );

const collaboratorList =
  document.getElementById(
    "collaboratorList"
  );

function collaboratorRepositoryId() {
  const id =
    localStorage.getItem(
      "minivcs_selected_repository_id"
    );

  return id ? Number(id) : null;
}

function showCollaboratorMessage(
  message,
  isError = false
) {
  if (!collaboratorMessage) {
    return;
  }

  collaboratorMessage.textContent =
    message;

  collaboratorMessage.hidden =
    false;

  collaboratorMessage.className =
    isError
      ? "msg err"
      : "msg ok";
}

function selectedRepositoryChanged() {
  loadCollaborators();
}

function roleOptions(selectedRole) {
  const roles = [
    "viewer",
    "contributor",
    "maintainer",
  ];

  return roles
    .map(
      (role) => `
        <option
          value="${role}"
          ${
            role === selectedRole
              ? "selected"
              : ""
          }
        >
          ${role}
        </option>
      `
    )
    .join("");
}

function renderCollaborators(members) {
  if (!collaboratorList) {
    return;
  }

  collaboratorList.innerHTML =
    "";

  if (!members.length) {
    collaboratorList.textContent =
      "No collaborators found.";
    return;
  }

  for (const member of members) {
    const row =
      document.createElement(
        "div"
      );

    row.className =
      "collaborator-row";

    const identity =
      document.createElement(
        "span"
      );

    identity.className =
      "collaborator-identity";

    identity.textContent =
      `${member.username} (${member.role})`;

    row.appendChild(identity);

    if (member.role === "owner") {
      const ownerLabel =
        document.createElement(
          "strong"
        );

      ownerLabel.textContent =
        "Owner";

      row.appendChild(ownerLabel);
    } else {
      const roleSelect =
        document.createElement(
          "select"
        );

      roleSelect.innerHTML =
        roleOptions(member.role);

      const updateButton =
        document.createElement(
          "button"
        );

      updateButton.type =
        "button";

      updateButton.textContent =
        "Update role";

      updateButton.addEventListener(
        "click",
        () => updateCollaboratorRole(
          member.user_id,
          roleSelect.value
        )
      );

      const removeButton =
        document.createElement(
          "button"
        );

      removeButton.type =
        "button";

      removeButton.textContent =
        "Remove";

      removeButton.addEventListener(
        "click",
        () => removeCollaborator(
          member.user_id
        )
      );

      row.appendChild(roleSelect);
      row.appendChild(updateButton);
      row.appendChild(removeButton);
    }

    collaboratorList.appendChild(row);
  }
}

async function loadCollaborators() {
  const repositoryId =
    collaboratorRepositoryId();

  if (!repositoryId) {
    if (collaboratorList) {
      collaboratorList.textContent =
        "Select a repository to view collaborators.";
    }

    return;
  }

  try {
    const result =
      await apiRequest(
        `/api/repositories/${repositoryId}/members`,
        {
          method: "GET",
        }
      );

    renderCollaborators(
      result.data?.members || []
    );
  } catch (error) {
    showCollaboratorMessage(
      error.message,
      true
    );

    if (collaboratorList) {
      collaboratorList.textContent =
        "Collaborators could not be loaded.";
    }
  }
}

async function addCollaborator() {
  const repositoryId =
    collaboratorRepositoryId();

  const username =
    collaboratorUsername?.value
      ?.trim() || "";

  const role =
    collaboratorRole?.value || "viewer";

  if (!repositoryId) {
    showCollaboratorMessage(
      "Select a repository first.",
      true
    );
    return;
  }

  if (!username) {
    showCollaboratorMessage(
      "Enter a username.",
      true
    );
    return;
  }

  try {
    await apiRequest(
      `/api/repositories/${repositoryId}/members`,
      {
        method: "POST",
        body: JSON.stringify({
          username,
          role,
        }),
      }
    );

    collaboratorUsername.value =
      "";

    showCollaboratorMessage(
      "Collaborator added."
    );

    await loadCollaborators();
  } catch (error) {
    showCollaboratorMessage(
      error.message,
      true
    );
  }
}

async function updateCollaboratorRole(
  memberUserId,
  role
) {
  const repositoryId =
    collaboratorRepositoryId();

  try {
    await apiRequest(
      `/api/repositories/${repositoryId}/members/${memberUserId}`,
      {
        method: "PATCH",
        body: JSON.stringify({
          role,
        }),
      }
    );

    showCollaboratorMessage(
      "Collaborator role updated."
    );

    await loadCollaborators();
  } catch (error) {
    showCollaboratorMessage(
      error.message,
      true
    );
  }
}

async function removeCollaborator(
  memberUserId
) {
  const repositoryId =
    collaboratorRepositoryId();

  try {
    await apiRequest(
      `/api/repositories/${repositoryId}/members/${memberUserId}`,
      {
        method: "DELETE",
      }
    );

    showCollaboratorMessage(
      "Collaborator removed."
    );

    await loadCollaborators();
  } catch (error) {
    showCollaboratorMessage(
      error.message,
      true
    );
  }
}

addCollaboratorButton?.addEventListener(
  "click",
  addCollaborator
);

window.addEventListener(
  "repositorySelected",
  selectedRepositoryChanged
);

loadCollaborators();