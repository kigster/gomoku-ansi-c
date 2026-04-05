---
name: Non-interactive execution preference
description: User grants blanket permission to run/edit anything without confirmation — only ask about product direction
type: feedback
---

Run all commands, edits, file writes, psql queries, and test executions non-interactively. Do not pause for confirmation on tool use.

**Why:** User is an experienced engineer working on their own project and finds confirmation prompts disruptive to flow.

**How to apply:** Only ask before proceeding when the question is about product direction or design decisions — never for file edits, shell commands, or database operations in this project.
