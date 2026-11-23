"use strict";

(function () {
  const byId = (id) => document.getElementById(id);

  function repositoryId() {
    const value = Number.parseInt(byId("repositoryId")?.value, 10);

    if (!Number.isInteger(value) || value <= 0) {
      throw new Error("Select a repository first.");
    }

    return value;
  }

  function show(text, ok) {
    const element = byId("mergeMessage");

    if (!element) {
      return;
    }

    element.textContent = text;
    element.className = ok ? "msg ok" : "msg err";
    element.hidden = false;
  }

  function parseBranches(output) {
    const result = { current: "", others: [] };

    (output || "").split(/\r?\n/).forEach((line) => {
      if (!line.trim()) {
        return;
      }

      const isCurrent = line.trimStart().startsWith("*");
      const name = line.replace(/^[*\-\s]+/, "").trim();

      if (!name || name.startsWith("Branch")) {
        return;
      }

      if (isCurrent) {
        result.current = name;
      } else {
        result.others.push(name);
      }
    });

    return result;
  }

  async function loadMergeBranches() {
    const select = byId("mergeSourceSelect");

    if (!select) {
      return;
    }

    let id;

    try {
      id = repositoryId();
    } catch (error) {
      return;
    }

    try {
      const result = await apiRequest(`/api/repositories/${id}/branches`);
      const branches = parseBranches(result.data.output);

      select.innerHTML = "";

      const placeholder = document.createElement("option");
      placeholder.value = "";
      placeholder.textContent = branches.others.length
        ? "Select a branch"
        : "No other branches to merge";
      select.appendChild(placeholder);

      branches.others.forEach((name) => {
        const option = document.createElement("option");
        option.value = name;
        option.textContent = name;
        select.appendChild(option);
      });

      byId("currentBranchLabel").textContent =
        `Current branch: ${branches.current || "-"}`;
    } catch (error) {
      show(error.message, false);
    }
  }

  async function mergeBranch() {
    try {
      const id = repositoryId();
      const source = byId("mergeSourceSelect").value;

      if (!source) {
        throw new Error("Select a branch to merge.");
      }

      const result = await apiRequest(`/api/repositories/${id}/merge`, {
        method: "POST",
        body: JSON.stringify({ source }),
      });

      show(result.data.output || result.message, true);
      window.dispatchEvent(new Event("repositoryChanged"));
      await loadMergeBranches();
    } catch (error) {
      show(error.message, false);
    }
  }

  byId("btnMergeBranch")?.addEventListener("click", mergeBranch);

  window.addEventListener("repositorySelected", loadMergeBranches);
  window.addEventListener("repositoryChanged", loadMergeBranches);
})();
