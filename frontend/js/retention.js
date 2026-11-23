"use strict";

(function () {
  const message = document.getElementById("msg");
  const rows = document.getElementById("retentionRows");
  const empty = document.getElementById("empty");
  const select = document.getElementById("repoSelect");
  const daysInput = document.getElementById("days");
  let repositoryId = null;

  function show(text, ok) {
    message.textContent = text;
    message.className = ok ? "msg ok" : "msg err";
    message.hidden = !text;
  }

  function cell(text) {
    const td = document.createElement("td");
    td.textContent = text;
    return td;
  }

  function badgeCell(text) {
    const td = document.createElement("td");
    const span = document.createElement("span");
    span.className = "badge " + text.toLowerCase().split(" ")[0];
    span.textContent = text;
    td.appendChild(span);
    return td;
  }

  function render(data) {
    rows.innerHTML = "";

    if (data.days) {
      daysInput.value = data.days;
    }

    (data.snapshots || []).forEach((snapshot) => {
      const tr = document.createElement("tr");
      tr.appendChild(cell(snapshot.id.slice(0, 8)));
      tr.appendChild(cell(snapshot.date));
      tr.appendChild(
        cell(snapshot.age_days < 0 ? "-" : `${snapshot.age_days} d`)
      );
      tr.appendChild(badgeCell(snapshot.result));
      rows.appendChild(tr);
    });

    empty.hidden = (data.snapshots || []).length > 0;
  }

  async function refresh() {
    rows.innerHTML = "";
    empty.hidden = true;

    if (!repositoryId) {
      return;
    }

    try {
      const result = await apiRequest(
        `/api/repositories/${repositoryId}/retention`
      );
      render(result.data);
      show("", true);
    } catch (error) {
      show(error.message, false);
    }
  }

  async function post(path, body, successText) {
    if (!repositoryId) {
      show("Select a repository first.", false);
      return;
    }

    try {
      const result = await apiRequest(path, {
        method: "POST",
        body: body ? JSON.stringify(body) : undefined,
      });
      render(result.data);
      show(successText || result.message, true);
    } catch (error) {
      show(error.message, false);
    }
  }

  document.getElementById("btnSaveDays").addEventListener("click", () => {
    post(
      `/api/repositories/${repositoryId}/retention`,
      { days: Number.parseInt(daysInput.value, 10) },
      "Retention period saved."
    );
  });

  document.getElementById("btnCleanup").addEventListener("click", () => {
    if (!repositoryId) {
      show("Select a repository first.", false);
      return;
    }

    if (!window.confirm("Permanently delete all expired snapshots?")) {
      return;
    }

    post(`/api/repositories/${repositoryId}/retention/cleanup`, null);
  });

  loadRepositoryPicker(
    select,
    (id) => {
      repositoryId = id;
      refresh();
    },
    message
  );
})();
