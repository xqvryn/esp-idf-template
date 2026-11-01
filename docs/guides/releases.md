# CI and releases

## What CI runs

Every push and pull request runs, without secrets:

- formatting and the architecture boundary check
- host tests
- a strict documentation build
- ESP32-S3 firmware builds for the root project, each application profile, the Unity test app, and every example

The networking-only example fails CI if the node application libraries appear in its build graph.

Documentation is built with `mkdocs build --strict`. GitHub Pages deployment is separate and off until you set `ENABLE_PAGES=true`. The Pages job runs only for a push to `main`. See [Repository setup](../repository-setup.md).

## How a version is chosen

There is no committed firmware version. [semantic-release](https://github.com/semantic-release/semantic-release) reads commit messages:

| Commit | Effect |
| --- | --- |
| `fix:` | patch |
| `feat:` | minor |
| `feat!:` or a `BREAKING CHANGE` footer | major |
| `docs:`, `refactor:`, `chore:`, tests | no version bump |

`main` is the stable channel. `develop` is the `dev` prerelease channel. Tags look like `v3.0.0` and `v3.0.0-dev.1`.

The version job plans that number and passes it as `PROJECT_VER`. The release job runs only when all of these are true:

- the event is a push to `main` or `develop`
- the repository variable `ENABLE_RELEASES` is `true`
- the plan says this push should publish

semantic-release then creates the git tag and the GitHub Release. A push of the tag does not start another release.

## How develop reaches main

1. Feature work lands on `develop` through a pull request. The merge can squash, and the PR title must be a Conventional Commit.
2. Promote `develop` to `main` with a **merge commit**, not a squash. Release analysis needs the original commits.
3. Merge `main` back into `develop`. A develop release is refused until `main` is an ancestor of `develop`.

The last stable tag is `v2.0.0` on `main`. The pending notes for the next stable tag are in the [changelog](../changelog.md). This documentation change does not publish that release.

Full rules, including the firmware archive checks, are in [Release details](../releases.md).
