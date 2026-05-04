# Milestone 2 implementation gaps

This note compares the **current tree** to **[Milestone 2](milestones.md#milestone-2--multi-account-single-session-catalogue-account-context--dictionary-model)** in `milestones.md`. It is a working checklist, not a commitment to implement every item as written.

## Reference

- Roadmap: [`milestones.md`](milestones.md) (Milestone 2 section).
- Bootstrap and login behaviour: [`gemini-bootstrap.md`](gemini-bootstrap.md).
- Related product docs: [`tcl-shell.md`](tcl-shell.md).

## Aligned with Milestone 2 (summary)

These areas largely match the milestone intent:

- **`ACCOUNTS` catalogue** (host JSON), account → host directory, per-account tree with **`MD`** and **`VOC`**.
- **VOC records** as attribute-per-line text (see `PickVoc::VocResolver` line parsing and committed `gemini/` fixtures).
- **`PickCore::UserSession`**, **`LoginService::runCatalogLogin`** from **`main`** after **`BootMonitor`**; Tcl after **`Shell::attachUserSession`**.
- **`LOGTO`**, **`WHO`**, **`LOGOFF`**; **`LOGOFF`** returns to host **`runCatalogLogin`** (no nested Tcl login loop).
- **Session fields** **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`** in Tcl, PROC, and BASIC (read-only where specified).
- **Out of scope by design:** distinct **`USERS`**-driven user identities (vs account), concurrency/locking.

## Gap 1 — Top-level Tcl vs VOC command resolution (addressed)

**Milestone text:** Tcl command resolution **through VOC**—Pick-style lookup, **not** a parallel shortcut resolver.

**Resolution:** Interactive Tcl now applies **`PickVoc::VocResolver::resolveVerbName`** to the first token in **`Shell::handleTclCommand`** (before operand expansion and builtin dispatch), with ASCII-uppercase canonicalization for the builtin map. PROC **`TCL ...`** lines use the same path (PROC may still pre-resolve once; a second hop is a no-op for typical **`V`** → builtin targets).

**Remaining stretch:** executing verbs that exist only in **`VOC`** without a C++ builtin; full Pick **`D`** data-dictionary semantics; **`X`**-item execution of embedded multi-line TCL from VOC.

## Gap 2 — Dictionary pointer types **D**, **A**, **X** (and full **V** / **Q** semantics) (addressed — minimal subset)

**Milestone text:** Full pointer semantics where applicable: **V, Q, D, A, X**.

**Resolution:** **`PickVoc::VocResolver`** now parses **`D`**, **`A`**, and **`X`**. **`D`** and **`A`** participate in program/PROC file resolution like **`F`**. **`X`** is treated like **`V`** for verb alias strings (attribute 2 only). **`resolveVerbName`** follows **`Q`** / **`V`** / **`X`** chains with cycle detection and a 64-hop cap; **`listProgramFiles`** includes **`D`**/**`A`** file pointers.

**Remaining stretch (documented in [`tcl-shell.md`](tcl-shell.md)):** full Pick **`D`**-item data dictionary behaviour (I-descriptors, correlatives, etc.); **`X`** as execute body; arbitrary deeper **`Q`** semantics beyond the current file/verb walks.

## Gap 3 — Session model: **MD** binding and default file pointers

**Milestone text:** Per-session state includes **current account** (Pick root), **current VOC** (and **MD** binding), **default file pointers**, Tcl environment, and BASIC runtime state.

**Current behaviour:** Session setup applies **catalogue root**, **Pick root** (filesystem root), VM reset, and identity fields (`ShellSession::applyUserSession`). Program/PROC/EDIT paths use **VOC** (plus defaults such as **BP** / **PROC** in the resolver). There is no separate **MD** dictionary layer or explicit **default file pointer** state distinct from that resolution path.

**Implication:** If Milestone 2 is read strictly, **MD**-driven bindings and **Pick-style default pointers** (beyond what falls out of VOC + resolver defaults) remain to be modelled in session and Tcl/BASIC I/O.

## Not gaps for delivered Milestone 2 (per roadmap)

- **`USERS`** catalogue as the authority for logon (deferred; **`USERS.json`** may exist for future/reporting).
- **`@TTY`**, **`@PRIVILEGES`**, richer **MD-derived** session fields — called **stretch** in `milestones.md`.

## Related source (for implementers)

| Topic | Primary location |
|--------|-------------------|
| Tcl dispatch | `src/userland/tcl/Shell.cpp` — `handleTclCommand` (VOC `resolveVerbName` + dispatch), `executeProcTclCommand`, `dispatch` |
| VOC parse / lookup | `src/core/voc/VocResolver.cpp`, `VocResolver.h` |
| Session / login | `src/userland/tcl/ShellSession.cpp`, `src/core/login/LoginService.cpp`, `UserSession.h` |
| Host entry / login loop | `src/userland/cli/Main.cpp`, `src/Main.cpp` |
