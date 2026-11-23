"use strict";

(function () {
  const message = document.getElementById("msg");
  const rows = document.getElementById("historyRows");
  const empty = document.getElementById("empty");
  const select = document.getElementById("repoSelect");

  function cell(text, className) {
    const td = document.createElement("td");
    td.textContent = text;
    if (className) {
      td.className = className;
    }
    return td;
  }

  function badge(text, className) {
    const td = document.createElement("td");
    const span = document.createElement("span");
    span.className = `badge ${className}`;
    span.textContent = text;
    td.appendChild(span);
    return td;
  }

  async function loadHistory(repositoryId) {
    rows.innerHTML = "";
    empty.hidden = true;
    message.hidden = true;

    if (!repositoryId) {
      return;
    }

    try {
      const result = await apiRequest(
        `/api/repositories/${repositoryId}/commits`
      );
      const commits = result.data.commits || [];

      commits.forEach((commit, index) => {
        const tr = document.createElement("tr");
        tr.appendChild(cell((commit.id || "").slice(0, 8)));
        tr.appendChild(cell(commit.message || ""));
        tr.appendChild(cell(commit.date || ""));
        tr.appendChild(
          index === 0
            ? badge("latest", "latest")
            : badge(
                (commit.message || "").startsWith("Merge branch")
                  ? "merge"
                  : "commit",
                "keep"
              )
        );
        rows.appendChild(tr);
      });

      empty.hidden = commits.length > 0;
    } catch (error) {
      message.textContent = error.message;
      message.className = "msg err";
      message.hidden = false;
    }
  }

  loadRepositoryPicker(select, loadHistory, message).then(() => {
    if (select.options.length && !select.value) {
      select.options[0].textContent =
        select.options.length > 1
          ? "Select a repository"
          : select.options[0].textContent;
    }
  });
})();
