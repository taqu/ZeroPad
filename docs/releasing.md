# Automatic tag releases

The branch/PR build workflow produces local candidate artifacts. The separate `release.yml` publishes automatically on a valid `v*` tag push. **Pushing a matching version tag is publication authorization:** no extra approval or manual dispatch is required. This replaces the earlier Phase 5 policy of preparing local assets only. Finish manual acceptance before pushing a release tag; CI cannot establish game/hardware compatibility.

## Version source and supported tags

CMakeLists.txt is authoritative: `project(ZeroPad VERSION MAJOR.MINOR.PATCH ...)` plus `set(ZEROPAD_PRERELEASE "")`. For prereleases set the latter to `alpha.1`, `beta.1`, or `rc.1` in committed source. The workflow never rewrites versions or overrides prerelease metadata from a tag.

Supported tags are `vMAJOR.MINOR.PATCH` and `vMAJOR.MINOR.PATCH-(alpha|beta|rc).N`, with canonical decimal integers (no leading zeros except zero itself). Other SemVer suffixes and build metadata are deliberately unsupported. Numeric core components must be at most 65535 for Windows resources. `v1.0.0-test`, `v1`, `v1.0`, `vabc`, and mismatched versions fail validation. Unrelated tags do not trigger this workflow.

Prerelease product strings and ZIP names retain the suffix, e.g. `1.1.0-rc.1`. Windows file/manifest version remains numeric `1.1.0.0`, with the prerelease resource flag set. Release is marked prerelease on GitHub.

## Maintainer steps

1. Review source, changelog, licenses, known limitations and manual acceptance results. Update version/prerelease metadata and commit. Protect release tags so only trusted maintainers can push them.
2. Build/test locally using [building instructions](building.md). Ensure the tagged tree and submodules are clean.
3. Create the matching lightweight or annotated tag and push it, for example:

```powershell
git tag v1.0.0
git push origin v1.0.0
```

These commands publish once the workflow passes. They were not run while implementing this workflow.

The Windows 2022 runner checks out the exact tag with recursive submodules, configures MSVC x64, validates generated version metadata, builds Release, and runs every noninteractive CTest suite. GUI tests remain off because they require a reliable interactive desktop. Zero tests or any test failure stops the workflow. The package script repeats tests, rejects dirty or mismatched tagged source, checks post-link hashes, imports and resource versions, and verifies extracted contents. Only the versioned ZIP and `.zip.sha256` are uploaded as release assets; BUILD_INFO is inside the ZIP.

Publication uses the standard GITHUB_TOKEN with contents:write only in the tag release job. No PAT or stored signing secret is needed. Same-tag runs queue rather than cancel. GitHub CLI creates a draft with both verified assets, then publishes only after uploads succeed. If an upload/publication fails, inspect the draft; existing releases are never silently replaced. Resolve/delete an incomplete draft deliberately before rerunning. Do not move an already published tag. A remote tag/checkout comparison immediately before creation detects tags moved during the build.

The release notes disclose unsigned status, tested source commit and CI verification limits. No certificate, security bypass, drivers or additional runtime dependencies are introduced. A hosted run must still be observed to verify repository permissions and actual GitHub publication; local validation cannot simulate these services.

For a local candidate use `tools/package.ps1` without ReleaseTag. For strict tagged packaging use `-ReleaseTag v1.0.0`. Both use the linked version to name artifacts and audit the executable; the strict mode additionally requires matching configured metadata, HEAD/tag identity and a clean worktree. Standard ZIP entry ordering and timestamps are preserved.

## Implementation verification

Locally verified on Windows 11 x64 with MSVC 19.44:

- Fresh Release build and all seven noninteractive CTest suites passed, including stable/prerelease syntax, malformed-tag rejection, version mismatch and wrong-architecture tests.
- Stable local candidate ZIP creation, extraction, hash/version/import audit and checksum passed.
- An isolated source copy with `rc.1` metadata built successfully. Its executable reports product version `1.0.0-rc.1`, file version `1.0.0.0`, and IsPreRelease=true.
- A throwaway local Git repository/tag exercised strict prerelease packaging: all seven suites passed, the versioned prerelease ZIP/checksum were produced, and BUILD_INFO records the clean tagged fixture commit. Adding an untracked fixture file correctly blocked packaging.
- Workflow YAML and the publication PowerShell block parsed successfully. No hosted workflow or actual GitHub release was executed. The main repository was not committed/tagged/pushed by this work; temporary test tags exist only in the isolated build fixture.
- A local mock of the publication step verified the successful annotated-tag/prerelease path and confirmed that moved tags, checksum mismatches and upload failures never reach the publish command. This tests control flow, not GitHub authentication or service behavior.

The two interactive suites remain excluded from hosted release CI by design. Prior GUI/game/hardware acceptance findings remain applicable; automated publication does not certify them.

Implementation references: [GitHub tag triggers](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/trigger-a-workflow), [Git references API](https://docs.github.com/en/rest/git/refs#get-a-reference), [annotated tags API](https://docs.github.com/en/rest/git/tags#get-a-tag), and [GitHub CLI release creation](https://cli.github.com/manual/gh_release_create).
