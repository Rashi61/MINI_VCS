#include "MergeManager.h"

#include <cassert>
#include <filesystem>

int main() {
    const std::filesystem::path root =
        std::filesystem::current_path();

    MergeManager mergeManager(root);

    const MergeResult result =
        mergeManager.mergeBranch("feature");

    assert(!result.success);
    assert(!result.hasConflicts);

    return 0;
}
