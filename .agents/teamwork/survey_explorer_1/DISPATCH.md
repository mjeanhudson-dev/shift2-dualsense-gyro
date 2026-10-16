# DISPATCH: Survey Explorer 1 (Environment, Host System, Binary Acquisition & Target Directory)

## Identity & Working Directory
- Archetype: teamwork_preview_explorer
- Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_1
- Parent Conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a

## Objective
Investigate the environment and host system for deploying a standalone local Git Forge on Windows localhost:
1. Examine target directory `C:\Users\Zachariah\Documents\antigravity\local-forge` (does it exist, what is currently inside?).
2. Check existing binaries or downloads: is a `gitea.exe` or `forgejo.exe` already present anywhere on disk (e.g. downloads, temp, or local-forge)?
3. If not already present, determine the exact download URL or installation mechanism for standalone Windows Gitea/Forgejo binary (e.g., official release binary from dl.gitea.com or codeberg.org/forgejo/forgejo/releases, or winget/curl). Check network access and what tools are available (curl.exe, Invoke-WebRequest, tar/expand/etc.).
4. Check port 3000 (and alternative fallback ports if any): is port 3000 free or occupied? What command verifies this?
5. Verify Windows version, architecture (amd64/x64), PowerShell version, Git version (`git --version`), and any security/execution policy restrictions.

## Mandatory Reading
- ORIGINAL_REQUEST.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (read verbatim).

## Scope Boundaries
- You are read-only / exploratory. DO NOT download large binaries or start persistent background services yet. Small probing commands (e.g. `Test-NetConnection`, `git --version`, `Get-ChildItem`) are allowed.
- Output your findings to `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_1\handoff.md`.

## Output Requirements
Write `handoff.md` with:
- Target directory status and path layout
- Binary availability (existing path vs download URL, sha256, version)
- System capabilities (OS, arch, Git CLI availability, curl availability)
- Port 3000 availability check
- Recommended deployment strategy for the standalone executable

## Completion Criteria
When finished, send a message to orchestrator (`92832789-5f0b-43c9-ac41-db72954b753a`) stating that `handoff.md` is ready.
