#include "branchmanager.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const fs::path root =
        fs::temp_directory_path() / "minivcs_branch_test";

    fs::remove_all(root);
    fs::create_directories(root / ".minivcs" / "branches");

    {
        std::ofstream head(root / ".minivcs" / "HEAD");
        head << "main\n";
    }

    {
        std::ofstream mainBranch(
            root / ".minivcs" / "branches" / "main"
        );
        mainBranch << "\n";
    }

    BranchManager branchManager(root);

    assert(branchManager.getCurrentBranch() == "main");
    assert(branchManager.getCurrentCommit().empty());

    assert(branchManager.createBranch("feature"));
    assert(!branchManager.createBranch("feature"));
    assert(!branchManager.createBranch("bad name"));
    assert(!branchManager.createBranch("x.tmp"));

    const auto branches = branchManager.listBranches();
    assert(branches.size() == 2);
    assert(branchManager.getBranchCommit("feature").empty());

    assert(branchManager.switchBranch("feature"));
    assert(branchManager.getCurrentBranch() == "feature");
    assert(!branchManager.switchBranch("ghost"));

    assert(branchManager.getBranchCommit("main").empty());

    assert(!branchManager.deleteBranch("feature"));
    assert(!branchManager.deleteBranch("main"));

    assert(branchManager.switchBranch("main"));
    assert(branchManager.deleteBranch("feature"));
    assert(branchManager.listBranches().size() == 1);

    std::cout << "All branch tests passed\n";

    fs::remove_all(root);
    return 0;
}
