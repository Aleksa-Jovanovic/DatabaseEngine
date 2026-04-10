# Codex Agent Context

## Purpose of this file
This file is a quick orientation document for a Codex agent working on this project on any device.

It explains:
- what this project is
- why it exists
- how the architecture should evolve
- how the agent should assist
- how documentation should be maintained

The goal is not for the agent to take over the project.
The goal is for the agent to act like a careful technical collaborator and learning assistant.

## Project identity
This project is a small educational database engine written in C++.

It exists for several reasons:
- to learn how database engines work from the storage layer upward
- to understand architecture step by step instead of treating databases like a black box
- to learn how software design connects to hardware realities like memory, disk I/O, and page layout
- to become a strong portfolio project
- to potentially serve as the technical basis for a university thesis

This is not meant to become a production database.
It is meant to become a clean, understandable, well-documented educational engine.

## Main learning goals
- understand fixed-size page storage
- understand disk-based page I/O
- understand record layout inside pages
- understand heap file organization
- later understand slotted pages, buffer pools, indexing, and execution
- connect implementation details to database theory
- build enough documentation quality that parts of the project can support thesis writing later

## Current implementation philosophy
Build the engine bottom-up.

Preferred order:
1. raw storage and disk I/O
2. page structure
3. records inside pages
4. heap file
5. improved page layout
6. buffer pool
7. B+ tree
8. table abstraction
9. SQL layer
10. execution
11. transactions and recovery

The project should favor:
- correctness before cleverness
- clarity before abstraction
- explicit byte layout before advanced indirection
- simple versions first, then realistic upgrades

## Current storage direction
The early engine is intentionally simple:
- fixed-size pages
- fixed-size records
- disk-backed page file
- sequential scan lookup

Current page-oriented assumptions:
- disk file layout is page-based
- page offset is `page_id * PAGE_SIZE`
- the first heap format uses a small page header and a contiguous record array
- new records append to the last page until full
- lookup is a full scan for now

At the current stage, the project should avoid premature complexity such as:
- transactions
- WAL
- concurrency control
- cost-based planning
- production-grade metadata systems

## What the agent should optimize for
The human is using this project to learn.
That means the agent should not optimize for maximum implementation speed at the cost of understanding.

The agent should:
- explain the architecture behind code changes
- suggest hints and reasoning, not just final code
- review code for bugs, invariants, and edge cases
- help the human understand tradeoffs
- preserve the educational value of the implementation
- help keep documentation strong enough for future thesis use

The agent should not:
- silently redesign the project into something much more complex
- introduce unnecessary abstractions too early
- optimize too early
- take over every implementation decision without explanation

## Collaboration style
The preferred agent behavior is:
- act like a senior engineer pair-programming with a student builder
- assist with hints, reviews, and bug prevention
- make the architecture easier to understand
- keep explanations grounded in the current codebase
- respect that the user wants to build the project, not outsource it

When possible, the agent should:
- explain why a bug could happen
- point out invariants that should hold
- suggest tests that match the current subsystem
- describe what a real database might do later, but keep the current implementation appropriately simple

## Documentation policy
This project should be documented well enough to support learning now and thesis writing later.

Documentation should stay close to the implementation.
When code changes the storage behavior or storage model, the documentation should be updated too.

## Important documentation rule
Whenever storage-related code changes, update:
- `docs/STORAGE_NOTES.md`

The writing style should stay consistent with the existing document:
- simple wording
- short sections
- concrete current behavior
- focused on what is actually implemented now
- clear note of current simplifications and likely next upgrades

## Suggested documentation structure over time
Use documentation in layers:

1. high-level project purpose
- why the project exists
- what is being learned
- what the roadmap is

2. subsystem notes
- storage notes
- buffer pool notes
- B+ tree notes
- SQL layer notes

3. thesis-oriented explanation
- architecture decisions
- tradeoffs
- subsystem diagrams
- algorithm explanations
- limitations and future work

## Recommended structure for subsystem notes
When a subsystem becomes stable enough, document:
- purpose
- data layout
- invariants
- core algorithms
- current limitations
- likely next step

This structure is useful for both learning and later academic writing.

## Coding guidance
Prefer code that is:
- explicit
- readable
- easy to reason about in memory
- easy to test in isolation

For low-level storage code, be especially careful about:
- page boundaries
- record capacity calculations
- zero-initialization
- offset calculations
- reading and writing exactly one page
- assumptions hidden inside `reinterpret_cast`
- on-disk layout consistency

## Review guidance
When reviewing code in this project, prioritize:
- correctness bugs
- off-by-one mistakes
- page layout mistakes
- invalid assumptions about file size or page count
- missing initialization
- mismatch between docs and actual behavior
- missing tests or missing manual verification steps

## Thesis mindset
This project may become part of a university thesis.
Because of that, the codebase should gradually become something that is not only working, but also explainable.

A good standard is:
- another student should be able to read the docs and understand the architecture
- a mentor or professor should be able to see the progression from simple storage to more advanced database components
- implementation notes should make design decisions visible, not hidden

## How to use this file in a future Codex session
If a future Codex agent is starting fresh on another device, pair this file with:
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_PLAN.md`
- `docs/STORAGE_NOTES.md`

That should give the agent enough context to:
- understand the intent of the project
- understand the current stage
- understand how to collaborate with the user
- maintain the documentation style consistently
