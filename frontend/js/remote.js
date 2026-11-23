const remoteById = (id) =>
  document.getElementById(id);

function remoteRepositoryId() {
  const value =
    remoteById("repositoryId")?.value;

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

function showRemoteMessage(
  message,
  ok
) {
  const element =
    remoteById("remoteMessage");

  if (!element) {
    return;
  }

  element.textContent = message;
  element.hidden = false;
  element.className = ok
    ? "msg ok"
    : "msg err";
}

async function performRemoteOperation(
  operation
) {
  try {
    const repositoryId =
      remoteRepositoryId();

    const result = await apiRequest(
      `/api/repositories/${repositoryId}/remote/${operation}`,
      {
        method: "POST",
      }
    );

    showRemoteMessage(
      result.data.output ||
        result.message,
      true
    );

    window.dispatchEvent(
      new Event("repositoryChanged")
    );
  } catch (error) {
    showRemoteMessage(
      error.message,
      false
    );
  }
}

remoteById("btnClone")?.addEventListener(
  "click",
  () => performRemoteOperation("clone")
);

remoteById("btnPush")?.addEventListener(
  "click",
  () => performRemoteOperation("push")
);

remoteById("btnPull")?.addEventListener(
  "click",
  () => performRemoteOperation("pull")
);
