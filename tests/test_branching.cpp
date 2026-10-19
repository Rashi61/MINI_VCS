#include "branchmanager.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    fs::path root = fs::temp_directory_path() / "minivcs_branch_test";
    fs::remove_all(root);
    fs::create_directories(root / ".minivcs" / "branches");
    std::ofstream(root / ".minivcs" / "HEAD") << "main\n";
    std::ofstream(root / ".minivcs" / "branches" / "main") << "c1\n";

    BranchManager bm(root);

    assert(bm.getCurrentBranch() == "main");
    assert(bm.getCurrentCommit() == "c1");

    assert(bm.createBranch("feature"));
    assert(!bm.createBranch("feature"));       // duplicate
    assert(!bm.createBranch("bad name"));      // invalid characters
    assert(!bm.createBranch("x.tmp"));         // reserved ending
    assert(bm.listBranches().size() == 2);
    assert(bm.getBranchCommit("feature") == "c1");

    assert(bm.switchBranch("feature"));
    assert(bm.getCurrentBranch() == "feature");
    assert(!bm.switchBranch("ghost"));         // doesn't exist

    assert(bm.updateBranchCommit("feature", "c2"));
    assert(bm.getCurrentCommit() == "c2");
    assert(bm.getBranchCommit("main") == "c1"); // main unaffected

    assert(!bm.deleteBranch("feature"));       // current branch
    assert(!bm.deleteBranch("main"));          // protected
    assert(bm.switchBranch("main"));
    assert(bm.deleteBranch("feature"));
    assert(bm.listBranches().size() == 1);

    std::cout << "All branch tests passed\n";
    return 0;
}
