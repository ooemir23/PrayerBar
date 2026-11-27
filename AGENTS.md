# Repository guidance

Before changing location detection, city search, timezone handling, or related privacy disclosures, read `docs/LOCATION_DETECTION_HANDOFF.md`. Preserve the manual-location override and keep built-in city search usable offline.

## Microsoft Store & GitHub parity (mandatory)

- Start every session by syncing with GitHub (`git pull --ff-only origin main`) so every device and AI assistant works from the exact same baseline.
- Every Microsoft Store submission (`.msix`) MUST match GitHub `origin/main` commit-for-commit: verify a clean working tree (`git status --porcelain` empty) and `HEAD == origin/main` before packaging and uploading to Partner Center.
- Version strings in `src/core/commercial_config.h` (`kAppVersionW`) and `packaging/store.config.json` (`Version`) MUST match each other, be strictly higher than the previous Store submission, and be committed and pushed to `origin/main` before building or uploading the Store `.msix`.
- Never upload a local-only, unpushed, or dirty build to Microsoft Partner Center. If a change is needed before submission, commit and push it to `origin/main` first, then rebuild (or use the GitHub Actions `Build & Package Store MSIX` artifact for that commit).
- Keep `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, and `.github/copilot-instructions.md` synchronized whenever project rules change.
