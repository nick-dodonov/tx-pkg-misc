# Kit

Configurations, settings, and tools to set up build, debug, or development assistance, shared between repositories via git subtree. Git submodules can also be used for this purpose, but the currently selected approach is to set up dependencies as Bazel modules without submodules.

## Adding Kit to a New Repository

To add the `/uni` subtree folder to a new repository from `git@github.com:nick-dodonov/tx-kit-uni.git`:

```bash
# Add the remote repository
git remote add tx-kit-uni git@github.com:nick-dodonov/tx-kit-uni.git

# Add the subtree
git subtree add --prefix=uni tx-kit-uni main --squash

# Update the subtree later
git subtree pull --prefix=uni tx-kit-uni main --squash

# Pushing changes back to upstream
git subtree push --prefix=uni tx-kit-uni main
##
