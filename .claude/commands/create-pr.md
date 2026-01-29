---

description: "Create a pull request from current branch or staged changes"
allowed-tools:

- Bash(./.claude/branch-name.sh)
- Bash(just check-all:\*)
- Bash(git \*)
- Bash(gh \*)

---

# Create Pull Request

This command creates a pull request either from the current branch or a set of staged for commit files.

## Branch Name

First you must check if the current branch is `main` or not. If the branch is not `main`, we are OK, and we can skip to the next section.

If, however, the current branch is `main` and the user has some changes (staged or unstaged), you must first create a new branch for this pull request.

To do so, you must first invoke the script provided in this directory named `branch-name.sh` and capture it's STDOUT. The script interacts with the user by means of STDIN and STDERR, and the branch name is the only data printed to STDOUT. Once you capture the branch name, create it with `git checkout -b <branch-name>`.

## Committing The Staged Files

There may be several situations that we have to deal with:

1. There are staged files only (the diff of those is described with `git diff --cached`)
1. There are staged and unstaged files or untracked files
1. There are unstaged or untracked files only

### 1. Staged Files Only

If there are staged files only, and we are already on the branch, we must first run the command `make test` from project's root, and verify that everything passes. Then run `make format` to reformat all C files.

If something doesn't pass, we must read and interpret its output, and ask the user whether they want us to fix the current issue, and if yes proceed accordingly, assuming that all non-destructive changes are auto-approved. We can also use permissions defined in the `settings.local.json` file.

Once all of the the checks are passing, we can `git add` our changes, and can skip to the section further below called "Committing Changes and Creating the PR".

### 2. There are Staged and Unstaged or Untracked Files

If there are unstaged or untracked changes in the workspace in addition to staged, you must ask the user if these files are to be stashed, all of them added to staged, each file is added to staged or not interactively, or discarded.

Present the choices as a menu that user picks from.

If the user responds with:

- Stashed, you must perform a series of git commands to stash the unstaged/untracked files for later recall:
  1. `git commit -m 'WIP: staged changes' --no-verify` — commit the staged changes temporarily
  1. `git stash push --include-untracked` — stash the remaining unstaged changes
  1. `git reset --soft HEAD^` — undo the temporary commit.
  1. `git add .` — to stage the previously staged for commit files

Now the situation is identical to "1. Staged Files Only", so you follow those instructions above.

After the PR is created and branch is pushed (See "Committing Changes and Creating the PR" section) ask the user if they want to restore the unstaged changes, and if they confirm, run `git stash apply`.

### 3. There are Unstaged and/or Untracked files only

In this case, you ask the user if they want to `git add` all of the files, or just some of the files, and unless they say "all of the files", you stop and wait for them to perform this action on their own, before you continue with the PR. Let the user know to use another terminal to choose which files to stage and which not.

After that you will be in the situation described by the section "2. There are Staged and Unstaged or Untracked Files", and you continue accordingly.

## Committing Changes and Creating the PR

Once we are on the branch, and either there are only changes staged for commit or NO changes at all (so we'll be creating the PR from the branch).

### Rebasing from `main`

At that point, ask the user if the want to rebase from `main` before creating their PR as they may have to resolve conflicts.

If the user says yes, run the command `git fetch`, then `git rebase origin/main`.

### Resolving Conflicts

If there are conflicts, you inform the user and ask them if they need help resolving conflicts and if you know how to do that, you help the user to do so. Otherwise you wait until the user resolves conflicts in another terminal and returns when the files are staged for commit and conflicts are resolved.

### Committing Currently Staged Changes

At this point, you should either a clean branch with all changes committed, or some changes staged for commit.

If there are changes staged for commit, you must perform `git diff --cached`, and summarize those changes in the markdown format in a temporary file we'll call <tempfile> (preferably under /tmp folder), and create a short title for the commit.

Then we commit with `git commit -m "<title>" -F "<tempfile>"`, and push the changes `git push -u origin`.

### Creating the PR

This is the final step. You are to use command `gh` (which stands for `github`).

In order to create a PR we must NOT be on the `main` branch, we must have all changes pushed to the origin and we must have no locally untracked or modified files.

The next step is describing the PR.

You are going to perform the diff of the current branch with the remote `main` branch using `git diff origin/main...HEAD`, and analyze it. You will create a markdown file that we'll refer to <pr-description> preferably in the `/tmp` folder, that describes the changes of this PR precisely, professionally, without any emojis. It should have the top header title that we'll refer to <pr-title>, (with a single '#' in markdown) that will also be the name of the PR. It should have the sections such as Summary (a short abstract-type description, no more than a few paragraphs) Description (if necessary, a more detailed description), Motivation, Testing, Backwards Compatibility, Scalability & Performance Impact, and Code Quality Analysis.

Once completed analysis and summarizing of this change, you will invoke the command `gh pr create -a @me -B main -F "<pr-description>" -t "<pr-title>"`.

If the command responds with an error such as Github Token authorization is insufficient, you are to save the PR description in the file at the root of the project called `PR.md` and report this to the user. Also print the URL required to create the PR on the web by clicking on the URL.

If you were able to create the PR with the `gh pr create` command, then open the web page to it anyway. Get the PR ID with `gh pr list | grep "<pr-title>" | awk '{print $1}'`
