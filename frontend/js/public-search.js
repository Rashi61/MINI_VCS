"use strict";

const searchInput =
  document.getElementById(
    "publicRepositorySearch"
  );

const clearButton =
  document.getElementById(
    "clearPublicRepositorySearch"
  );

const repositoryList =
  document.getElementById(
    "publicRepositoryList"
  );

const repositoryMessage =
  document.getElementById(
    "publicRepositoryMessage"
  );

let repositoryCards = [];

function captureRepositoryCards() {
  if (!repositoryList) {
    return;
  }

  repositoryCards =
    Array.from(
      repositoryList.children
    );
}

function filterRepositories() {
  if (!repositoryList) {
    return;
  }

  const query =
    (
      searchInput?.value ||
      ""
    ).trim().toLowerCase();

  let visibleCount = 0;

  for (const card of repositoryCards) {
    const cardText =
      (
        card.textContent ||
        ""
      ).toLowerCase();

    const matches =
      !query ||
      cardText.includes(query);

    card.hidden =
      !matches;

    if (matches) {
      visibleCount += 1;
    }
  }

  if (
    repositoryMessage &&
    query
  ) {
    repositoryMessage.textContent =
      visibleCount === 0
        ? `No public repositories match "${query}".`
        : `${visibleCount} matching public ` +
          "repository/repositories found.";

    repositoryMessage.hidden =
      false;
  }
}

function handleSearchInput() {
  captureRepositoryCards();
  filterRepositories();
}

function clearSearch() {
  if (searchInput) {
    searchInput.value =
      "";
  }

  captureRepositoryCards();

  for (const card of repositoryCards) {
    card.hidden =
      false;
  }

  if (repositoryMessage) {
    repositoryMessage.textContent =
      "Public repositories loaded.";

    repositoryMessage.hidden =
      false;
  }
}

if (searchInput) {
  searchInput.addEventListener(
    "input",
    handleSearchInput
  );
}

if (clearButton) {
  clearButton.addEventListener(
    "click",
    clearSearch
  );
}

/*
 * The public-repositories.js file loads cards
 * asynchronously. Watch only for new cards.
 * We never move cards inside the observer, so
 * there is no infinite mutation loop.
 */
if (repositoryList) {
  const observer =
    new MutationObserver(
      function () {
        captureRepositoryCards();

        if (
          searchInput?.value?.trim()
        ) {
          filterRepositories();
        }
      }
    );

  observer.observe(
    repositoryList,
    {
      childList: true
    }
  );

  setTimeout(
    captureRepositoryCards,
    100
  );
}