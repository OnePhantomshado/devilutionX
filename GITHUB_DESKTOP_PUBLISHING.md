# Publish DevilutionX-Hellgate with GitHub Desktop

This folder is a clean Git repository prepared for publication as:

- Repository: `OnePhantomshado/DevilutionX-Hellgate`
- Branch: `main`
- Project: DevilutionX-Hellgate with the D1Hellforge save editor and modding platform
- Upstream: `diasurgical/DevilutionX`

## 1. Create the GitHub fork

GitHub Desktop's **Publish repository** command creates an independent repository.
To retain GitHub's visible fork relationship, create the fork on GitHub first:

1. Open <https://github.com/diasurgical/DevilutionX>.
2. Select **Fork**, then **Create a new fork**.
3. Choose the `OnePhantomshado` account.
4. Create the fork. If GitHub requires the original name, create it as
   `DevilutionX` and then open **Settings > General > Repository name** and rename
   it to `DevilutionX-Hellgate`.
5. Do not add another README, `.gitignore`, or license.

The finished repository URL should be:

`https://github.com/OnePhantomshado/DevilutionX-Hellgate`

## 2. Add this prepared folder to GitHub Desktop

1. Open GitHub Desktop and sign in to `OnePhantomshado`.
2. Select **File > Add local repository**.
3. Browse to this `DevilutionX-Hellgate` folder and select **Add repository**.
4. Confirm that **Current branch** is `main` and the latest commit is
   `Release DevilutionX-Hellgate with D1Hellforge v0.3.0`.
5. Select **Push origin**.

The folder is already configured so that:

- `origin` is the OnePhantomshado fork.
- `upstream` is the official DevilutionX repository.

If GitHub Desktop reports that `main` and the fork's existing branch have
unrelated changes, do not force-push. The fork may contain GitHub-created files.
Delete and recreate the fork without initializing extra files, then repeat these
steps.

## 3. Verify the public repository

After the push, check that GitHub displays:

- `README.md` with the DevilutionX-Hellgate and D1Hellforge overview.
- `tools/D1Hellforge/` source code and changelog.
- `docs/d1hellforge/` architecture, formats, compatibility, and testing notes.
- Commit `Release DevilutionX-Hellgate with D1Hellforge v0.3.0` on `main`.

Do not upload game MPQs, save files, personal `D1Hellforge.ini` files, build
directories, or development work folders.
