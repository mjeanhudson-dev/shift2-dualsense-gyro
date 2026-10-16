# DISPATCH: Survey Explorer 3 (Verification Resources, Bounty Board, Handoff & Knowledge Formats)

## Identity & Working Directory
- Archetype: teamwork_preview_explorer
- Working Directory: c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3
- Parent Conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a

## Objective
Analyze the verification resources and multi-agent coordination records specified in ORIGINAL_REQUEST.md:
1. Examine `C:\Users\Zachariah\Documents\antigravity\BOUNTY_BOARD.md`:
   - What structure, tables, columns, tags, milestones, and claim statuses does it contain?
   - How are tasks/bounties defined?
   - What indicates Open vs Closed?
2. Examine `C:\Users\Zachariah\Documents\antigravity\GLOBAL_KNOWLEDGE_INDEX.md`:
   - How is knowledge indexed? What tags, categories, or linkages exist?
3. Examine `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md`:
   - What handoff data, completion logs, or milestones are recorded?
4. Examine `C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1`:
   - What does the existing verification script test, and how can we integrate/reuse or ensure compatibility?
5. Design the parsing logic for the two-way sync script:
   - Input format -> Markdown tables / headers / regex extraction
   - Output mapping -> Forge Milestones, Issue Titles, Issue Bodies, Issue Labels (e.g. `hardware-hid`, `physics-dampening`), Assignees / Claim statuses, Open vs Closed state.
   - Idempotency: how the sync script avoids duplicate issues when run repeatedly.

## Mandatory Reading
- ORIGINAL_REQUEST.md: `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md` (read verbatim).

## Scope Boundaries
- You are read-only / exploratory. Do not write code or modify files outside your working directory.
- Output your findings to `c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3\handoff.md`.

## Output Requirements
Write `handoff.md` with:
- Detailed schema and parsing rules for BOUNTY_BOARD.md, GLOBAL_KNOWLEDGE_INDEX.md, and PROJECT_HANDOFF.md.
- Mapping table from markdown artifacts to Gitea/Forgejo Issues, Milestones, and Labels.
- State reconciliation logic (Open vs Closed, claim status).
- Requirements and edge cases for the sync script implementation.

## Completion Criteria
When finished, send a message to orchestrator (`92832789-5f0b-43c9-ac41-db72954b753a`) stating that `handoff.md` is ready.


## 2026-10-16T15:53:20Z
You are survey_explorer_3.
Your working directory is c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3.
Read your dispatch assignment at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3\DISPATCH.md.
MANDATORY: You MUST read ORIGINAL_REQUEST.md at c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\ORIGINAL_REQUEST.md before beginning.
Analyze the verification resources and multi-agent coordination records:
- C:\Users\Zachariah\Documents\antigravity\BOUNTY_BOARD.md
- C:\Users\Zachariah\Documents\antigravity\GLOBAL_KNOWLEDGE_INDEX.md
- C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\PROJECT_HANDOFF.md
- C:\Users\Zachariah\Documents\antigravity\vibrant-darwin\verify_repo.ps1
Extract schema, tables, tags, claim statuses, and design the mapping and reconciliation logic for the two-way sync script.
Write your findings to c:\Users\Zachariah\Documents\antigravity\vibrant-darwin\.agents\teamwork\survey_explorer_3\handoff.md.
When done, notify the orchestrator (conversation ID: 92832789-5f0b-43c9-ac41-db72954b753a) via send_message with a brief summary and the path to handoff.md.
