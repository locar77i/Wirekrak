# Core Contracts — Executable Examples

This directory contains **executable examples that define the contractual behavior**
of Wirekrak Core.

These examples are not tutorials and are not intended to be “easy”.
They exist to make **responsibility, enforcement, and failure** explicit and observable.

If you are looking for a stable, convenience-oriented API, see the Lite examples.
If you are integrating Wirekrak Core directly, this directory defines what you can
and cannot rely on.

---

## What “Contracts” Mean Here

In this context, a *contract* is an explicit promise made by Wirekrak Core to the
integrator.

Contracts define:
- What Wirekrak Core **enforces**
- What Wirekrak Core **refuses to infer**
- What Wirekrak Core **exposes on failure**
- Where responsibility **shifts to the user**

Anything not demonstrated or enforced here is **not part of the Core contract**.

---

## How to Read These Examples

Each example in this directory is intentionally scoped to demonstrate **one idea**.

You are expected to:
- Run the example
- Observe the output
- Reason about what the system does *and does not do*

These examples do not:
- Smooth behavior
- Hide failures
- Automatically recover
- Guess intent

If something feels uncomfortable, that is intentional.

---

## Relationship to Lite

Some examples in this directory may resemble Lite examples in overall shape.
This similarity is deliberate.

The contrast highlights how responsibility changes between layers:

- Lite prioritizes usability and a stable façade
- Core prioritizes correctness, enforcement, and explicit responsibility

Do not assume Lite behavior from Core examples, or Core guarantees from Lite examples.

---

## Example Progression

The examples are organized to be read in order:

### `00_visibility`
Demonstrates what Wirekrak Core observes and exposes, without inference or recovery.

### `01_responsibility`
Demonstrates what Wirekrak Core requires the integrator to manage explicitly.

### `02_enforcement`
Demonstrates invariants and behaviors that Wirekrak Core enforces unconditionally.

### `03_failure`
Demonstrates how failures are surfaced and what is *not* hidden or repaired.

Each directory contains a short README describing the specific contract being shown.

---

## What These Examples Do Not Do

These examples intentionally do **not**:

- Provide onboarding or quickstart guidance
- Abstract exchange behavior
- Guarantee delivery completeness in user callbacks
- Implement retry or reconnection strategies
- Optimize for developer convenience

Those concerns belong either to Lite or to application-level code.

---

## Design Principle

Wirekrak Core follows one rule above all others:

> **Correctness is enforced internally.  
> Contracts are defined explicitly and narrowly.**

These examples exist to make that rule executable.

---
