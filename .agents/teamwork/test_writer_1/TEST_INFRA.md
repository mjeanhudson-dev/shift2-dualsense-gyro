# Local Git Forge E2E Test Suite Infrastructure (TEST_INFRA)

**Document Version:** 1.0.0  
**Author:** `test_writer_1` (Teamwork E2E Test Suite Track)  
**Parent Orchestrator:** `92832789-5f0b-43c9-ac41-db72954b753a`  
**Target Repository & Runtime:** `C:\Users\Zachariah\Documents\antigravity\local-forge`  
**Host Architecture:** Windows 11 AMD64, PowerShell 5.1+, Port 3000, Embedded SQLite3  

---

## 1. Executive Summary & Purpose

The Local Git Forge E2E Test Suite is an independent, opaque-box, requirement-driven test framework designed to validate all functional, non-functional, and operational requirements specified in `ORIGINAL_REQUEST.md` and `PROJECT.md`.

The suite is engineered according to the **4-Tier Systematic Testing Methodology**:
1. **Tier 1 - Feature Coverage (>=5 per requirement)**: Isolated happy-path verification of every primary feature (R1: Standalone Instance, R2: Git Remote & Web Browser, R3: Bounty Board Synchronization, R4: Service Lifecycle Management).
2. **Tier 2 - Boundary & Corner Cases (>=5 per requirement)**: Extreme values, conflict handling, race conditions, character encodings, and malformed inputs across all four requirements.
3. **Tier 3 - Cross-Feature Combinations (Pairwise)**: Dynamic subsystem interactions (Lifecycle ↔ Git HTTP, Git Push ↔ Bounty Sync, Active Load ↔ Status Monitoring, Cold Reboots ↔ SQLite Persistence).
4. **Tier 4 - Real-World Workload Scenarios (E2E Journeys)**: Full end-to-end operator workflows (Zero-to-Hero Bootstrap, 16-Bounty State Parity, Vibrant Darwin Mirroring, Fault Recovery & Clean Teardown).

---

## 2. Directory Structure & Co-Location

All test scripts, helpers, and verification assets are co-located within the Local Forge project root under `tests/`:

```
C:\Users\Zachariah\Documents\antigravity\local-forge\
├── tests\
│   ├── run_e2e_tests.ps1             # Master Test Suite Runner (Tier filtering, reporting)
│   ├── test_helpers.ps1              # Shared test framework, assertions, fixtures, API wrapper
│   ├── test_tier1_features.ps1       # Tier 1: 24 Feature Coverage Tests (6 per requirement)
│   ├── test_tier2_boundaries.ps1     # Tier 2: 24 Boundary & Corner Case Tests (6 per requirement)
│   ├── test_tier3_interactions.ps1   # Tier 3: 5 Cross-Feature Combination Tests
│   ├── test_tier4_workloads.ps1      # Tier 4: 4 Real-World E2E User Journeys
│   └── TEST_INFRA.md                 # Test architecture and runner documentation
├── manage_forge.ps1                  # System under test (Lifecycle daemon manager)
├── sync_forge.ps1                    # System under test (Bounty board sync engine)
├── custom\conf\app.ini               # System under test (Forge configuration)
└── data\gitea.db                     # System under test (SQLite database)
```

---

## 3. Test Invariants & Methodology

### 3.1 Opaque-Box Validation
Tests treat the Local Git Forge as an opaque black box. All tests interact exclusively via:
- **Loopback HTTP REST API**: `http://127.0.0.1:3000/api/v1`
- **Native Smart HTTP Git Client**: `git clone`, `git push`, `git commit` via Windows Git CLI
- **Web UI HTTP Endpoints**: HTML source tree, commit log, and visual diff endpoints
- **PowerShell CLI Lifecycle Interface**: `manage_forge.ps1 -Action start|stop|status|test|sync`
- **File System State Verification**: SQLite database file existence, SHA256 hashes, PID tracking

### 3.2 Authoritative Output Derivation
Every assertion in the test suite is strictly derived from authoritative specifications:
- **R1 (Standalone Instance)**: `ORIGINAL_REQUEST.md §R1`, `PROJECT.md §Interface Contracts`, and `survey_spec_miner_2\handoff.md §3.1`.
- **R2 (Git Remote & Web UI)**: `ORIGINAL_REQUEST.md §R2`, `PROJECT.md §F11-F13`, and Gitea OpenAPI Swagger schemas.
- **R3 (Bounty Board Sync)**: `BOUNTY_BOARD.md` (16 bounties, 15 completed, 1 open: `BOUNTY-VD-016`), `GLOBAL_KNOWLEDGE_INDEX.md` (10 domain taxonomy tags with hex colors), and `PROJECT_HANDOFF.md` (4 aggregated milestones).
- **R4 (Lifecycle Management)**: `ORIGINAL_REQUEST.md §R4`, `PROJECT.md §F7-F10`, and Windows process lifecycle contracts.

### 3.3 Zero-Facade Guarantee
Tests perform real network socket connections, invoke real external binaries, push real Git objects, and assert on HTTP response payloads. No mocked responses or artificial pass flags exist in the suite.

### 3.4 Progressive Testability
During milestone development (Milestones M1–M5), running `run_e2e_tests.ps1` produces clear, diagnostic reasons for pending components (e.g. `Gitea executable not found at C:\Users\Zachariah\Documents\antigravity\local-forge\bin\gitea.exe` or `Port 3000 not listening`). As each milestone completes, its corresponding tests immediately transition from FAIL to PASS without modifying test code.

---

## 4. Comprehensive 4-Tier Test Matrix (57 Tests Total)

### Tier 1: Feature Coverage (24 Tests)
| Test ID | Requirement | Name | Description |
|---|---|---|---|
| `T1.1_R1_HealthEndpoint` | R1 | API Version Endpoint | Asserts HTTP 200 on `/api/v1/version` and valid JSON version. |
| `T1.2_R1_WebLandingPage` | R1 | Web UI Landing Page | Asserts HTTP 200 on `/` and HTML document structure. |
| `T1.3_R1_SQLiteDatabaseFile` | R1 | SQLite Database File | Asserts `data/gitea.db` exists and is non-empty (>4 KB). |
| `T1.4_R1_PathContainment` | R1 | Strict Path Containment | Asserts 100% of paths in `app.ini` reside under `local-forge`. |
| `T1.5_R1_OfflineAndZeroDependency` | R1 | Offline Mode Configuration | Asserts `DB_TYPE=sqlite3`, `OFFLINE_MODE=true`, `DISABLE_SSH=true`. |
| `T1.6_R1_AdminTokenValidation` | R1 | Admin Token Verification | Asserts token from `forge_token.txt` authenticates user `admin`. |
| `T1.7_R2_CreateRepositoryViaAPI` | R2 | Create Repository via API | Asserts HTTP 201 on `POST /api/v1/user/repos`. |
| `T1.8_R2_GitHttpRemoteClone` | R2 | Git HTTP Clone | Clones repo over `http://localhost:3000/admin/<repo>.git`. |
| `T1.9_R2_GitCommitAndPush` | R2 | Git Commit and Push | Commits files and pushes to remote with HTTP authentication. |
| `T1.10_R2_WebUiSourceTree` | R2 | Web UI Source Tree | Asserts web browser renders committed files and markdown. |
| `T1.11_R2_WebUiCommitLogAndDiff` | R2 | Web UI Commits & Diffs | Asserts web browser renders commit history and line-by-line diff. |
| `T1.12_R2_BranchNavigation` | R2 | Branch Navigation | Asserts web UI lists branches and supports navigation. |
| `T1.13_R3_BountyMarkdownParsing` | R3 | Bounty Board Markdown Parsing | Asserts parser extracts all 16 bounties from `BOUNTY_BOARD.md`. |
| `T1.14_R3_MilestonesSync` | R3 | Milestones Synchronization | Asserts Milestones 1–4 are created in the forge repository. |
| `T1.15_R3_LabelTaxonomySync` | R3 | Domain Label Taxonomy Sync | Asserts 10 domain tags created with valid 6-character hex colors. |
| `T1.16_R3_IssueStateParity` | R3 | Issue State Parity | Asserts 15 closed issues and 1 open issue (`BOUNTY-VD-016`). |
| `T1.17_R3_IssueMetadataAnchoring` | R3 | Issue Metadata Anchoring | Asserts issues contain `<!-- antigravity:bounty_id=... -->`. |
| `T1.18_R3_SyncIdempotency` | R3 | Sync Idempotency | Asserts repeated sync runs produce 0 duplicate issues/labels. |
| `T1.19_R4_StatusRunning` | R4 | manage_forge.ps1 Status | Asserts `status` returns 0 and reports Running, PID, Uptime. |
| `T1.20_R4_ActionTest` | R4 | manage_forge.ps1 Test | Asserts `test` runs automated health validations with exit code 0. |
| `T1.21_R4_StopService` | R4 | manage_forge.ps1 Stop | Asserts `stop` terminates daemon and releases port 3000. |
| `T1.22_R4_StatusStopped` | R4 | manage_forge.ps1 Status Stopped | Asserts `status` on stopped service reports Stopped (code != 0). |
| `T1.23_R4_StartService` | R4 | manage_forge.ps1 Start | Asserts `start` launches detached process and creates PID file. |
| `T1.24_R4_ActionSync` | R4 | manage_forge.ps1 Sync | Asserts `sync` invokes synchronization engine with exit code 0. |

---

### Tier 2: Boundary & Corner Cases (24 Tests)
| Test ID | Requirement | Name | Edge Condition Tested |
|---|---|---|---|
| `T2.1_R1_PortCollisionDetection` | R1 | Port Collision Detection | Foreign TCP listener on port 3000 detected with socket conflict. |
| `T2.2_R1_InvalidTokenReturns401` | R1 | Invalid API Token | Request with corrupted token rejected with HTTP 401 Unauthorized. |
| `T2.3_R1_NonExistentApiRouteReturns404` | R1 | Non-Existent API Route | Invalid route returns HTTP 404 JSON, not server crash. |
| `T2.4_R1_MalformedJsonPayload` | R1 | Malformed JSON Payload | Invalid JSON returns client error (400/422), not HTTP 500. |
| `T2.5_R1_UnauthenticatedPrivateAccess` | R1 | Unauthenticated Private Access | Access to private repo resources without credentials returns 401/403. |
| `T2.6_R1_OfflineAssetIsolation` | R1 | Offline CDN Asset Isolation | Asserts zero external CDN requests (Google Fonts, Gravatar). |
| `T2.7_R2_DuplicateRepoConflict` | R2 | Duplicate Repository Conflict | `POST /user/repos` with existing name returns HTTP 409 Conflict. |
| `T2.8_R2_InvalidGitPushCredentials` | R2 | Invalid Git Push Auth | Pushing with bad password rejected with HTTP 401/403. |
| `T2.9_R2_SpecialCharactersInFilesAndCommits` | R2 | Special Characters in Files | Files with Unicode (`1€`, `°`, `$C^2$`) and spaces push cleanly. |
| `T2.10_R2_DeepDirectoryHierarchy` | R2 | Deep Directory Hierarchy | Deep nested paths (`/src/deep/nested/...`) render in web tree. |
| `T2.11_R2_NonExistentCommitSha404` | R2 | Non-Existent Commit SHA | Querying 40-zero commit SHA returns HTTP 404 in web UI. |
| `T2.12_R2_LargePayloadPush` | R2 | 2 MB Binary Payload Push | Multi-megabyte binary push verifies HTTP body buffer limits. |
| `T2.13_R3_EmptyTableSync` | R3 | Empty Table Sync | Syncing against empty markdown table completes without exception. |
| `T2.14_R3_SpecialCharactersInBountyTitles` | R3 | Special Characters in Bounty Titles | Titles with `1€`, `±45°`, and math expressions parse cleanly. |
| `T2.15_R3_DuplicateDomainTagsDeduplication` | R3 | Duplicate Tags Deduplication | Repeated tags (`#hid #hid`) deduplicated to single label. |
| `T2.16_R3_MissingMilestoneGracefulFallback` | R3 | Missing Milestone Fallback | Bounty with unmapped anchor falls back without throwing error. |
| `T2.17_R3_MalformedMarkdownRow` | R3 | Malformed Row Recovery | Broken markdown row skipped with warning; valid rows proceed. |
| `T2.18_R3_StateFlipReconciliation` | R3 | Two-Way State Flip Reconcile | Toggling status between OPEN and COMPLETED reconciles issue state. |
| `T2.19_R4_DoubleStartWhenRunning` | R4 | Double Start Idempotency | `start` when running returns code 0 without duplicate process. |
| `T2.20_R4_StopWhenAlreadyStopped` | R4 | Stop When Stopped | `stop` when stopped completes cleanly with exit code 0. |
| `T2.21_R4_StalePidFileRecovery` | R4 | Stale PID File Recovery | Dead PID in `gitea.pid` detected; `start` cleans and recovers. |
| `T2.22_R4_RapidRestartCycles` | R4 | Rapid Restart Cycles | 3 back-to-back restarts succeed without socket hang or leak. |
| `T2.23_R4_InvalidActionParameter` | R4 | Invalid Action Parameter | Unknown action parameter rejected with validation error. |
| `T2.24_R4_PortReleaseVerification` | R4 | Port Release Verification | Port 3000 verified released within 5 seconds of stop. |

---

### Tier 3: Cross-Feature Combinations (5 Tests)
| Test ID | Requirements | Name | Cross-Feature Interaction |
|---|---|---|---|
| `T3.1_LifecycleAndGitRemote` | R1+R2+R4 | Lifecycle ↔ Git HTTP | Push succeeds when running, fails when stopped, succeeds on restart. |
| `T3.2_GitPushAndBountySyncInterleaving` | R2+R3 | Git Commits ↔ Issue Sync | Commits referencing `[BOUNTY-VD-001]` interleave with synced issues. |
| `T3.3_DaemonStatusDuringActiveLoad` | R1+R4 | Active Load ↔ Status Query | `status` queries PID, memory, and HTTP 200 under high API traffic. |
| `T3.4_SyncReconciliationAfterApiDrift` | R3+R1 | API Drift ↔ Sync Reconcile | Manually drifted issue state via API is restored by sync engine. |
| `T3.5_FullServiceRestartPreservesData` | R1+R2+R3+R4 | Cold Boot ↔ Persistence | Full daemon shutdown and restart verifies 100% data preservation. |

---

### Tier 4: Real-World Workload Scenarios (4 Tests)
| Test ID | Requirements | Journey Name | Full User Journey Verified |
|---|---|---|---|
| `T4.1_ZeroToHeroBootstrapJourney` | R1+R4 | Zero-to-Hero Bootstrap | Clean directory -> Binary check -> Configuration check -> Migration check -> Token check -> Background daemon startup -> HTTP 200 health check. |
| `T4.2_FullMultiAgentBountySyncJourney` | R3+R1 | Multi-Agent Sync Journey | Provision `vibrant-darwin` repo -> Sync 16 bounties -> Verify 15 closed + 1 open (`BOUNTY-VD-016`) -> Verify 10 taxonomy labels -> Verify 4 milestones. |
| `T4.3_VibrantDarwinCodeMirrorJourney` | R2 | Vibrant Darwin Mirror Journey | Clone mirror -> Push real vibrant-darwin files (README, handoff, config, verify_repo.ps1) -> Verify web UI file tree, markdown render, commit log, and visual diffs. |
| `T4.4_CompleteEndToEndLifecycleAndRecoveryJourney` | R4+R1 | Operator Lifecycle & Recovery | `status` -> `sync` -> `test` -> Simulate process kill (`Stop-Process -Force`) -> Verify dead PID detection -> Self-healing `start` recovery -> Graceful `stop` teardown. |

---

## 5. Test Harness & Helper Library (`test_helpers.ps1`)

The shared test framework provides standardized functions:
- `Invoke-ForgeApi -Endpoint <path> [-Method GET|POST|PATCH|DELETE] [-Body <data>] [-Token <str>] [-SkipAuth]`: Robust REST API caller handling JSON serialization, authentication headers, and network exceptions without crashing.
- `New-TempGitWorkspace [-Prefix <name>]`: Creates an isolated temporary directory for Git clone/commit operations.
- `Remove-TempGitWorkspace -Path <path>`: Recursively cleans temporary Git workspace, stripping Windows readonly attributes.
- `Test-PortListening [-Port 3000]`: Non-blocking TCP loopback probe.
- `Assert-True`, `Assert-False`, `Assert-Equal`, `Assert-Match`, `Assert-FileExists`, `Assert-StatusCode`: Explicit assertion functions with detailed diagnostic failure reporting.
- `Execute-TestCase -TestId <id> -Tier <tier> -Requirement <req> -Name <name> -TestBlock { ... }`: Structured execution block recording duration in milliseconds and capturing stack traces on failure.

---

## 6. How to Run the Tests

### Master Test Suite Runner Command:
```powershell
powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1
```

### Options:
- **Run a specific Tier:**
  ```powershell
  # Run Tier 1 Feature Coverage
  powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1 -Tier 1

  # Run Tier 2 Boundaries
  powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1 -Tier 2

  # Run Tier 3 Interactions
  powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1 -Tier 3

  # Run Tier 4 Real-World Journeys
  powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1 -Tier 4
  ```

- **Export Detailed JSON Report:**
  ```powershell
  powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1 -Tier All -JsonReportPath "C:\Users\Zachariah\Documents\antigravity\local-forge\tests\test_results.json"
  ```

- **Halt Execution on First Failure:**
  ```powershell
  powershell -ExecutionPolicy Bypass -File C:\Users\Zachariah\Documents\antigravity\local-forge\tests\run_e2e_tests.ps1 -StopOnFail
  ```
