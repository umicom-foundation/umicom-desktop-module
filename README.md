# Umicom Desktop Module

`umicom-desktop-module` is the thin application repository for **Umicom Desk**.

It composes Framework-owned application discovery, launch planning, process
supervision, taskbar state, semantic layouts, context linking and the GTK4 Desk
adapter. It does not duplicate those capabilities.

## Repository role

```text
umicom-framework
    reusable application and desktop platform

umicom-desktop-module
    thin product profile and executable entry point

umicom-applications
    runnable multi-application superproject
```

## Build inside `umicom-applications`

The parent repository configures Framework once and then adds this module from:

```text
applications/desktop
```

The resulting executables are:

```text
umicom-desk
umicom-desk-console
```

When the graphical Desk starts, it presents installed applications as a
checkbox list. Several products can be selected and opened together. The
selection and launch report are Framework models; this module only supplies
the suite composition and supervised process adapter.

Before execution, the module can request a Framework guided launch plan. This
read-only plan tells the interface which selected applications will start,
which running applications will be brought forward, which workspace is the
recommended starting point, and why a catalogue item is unavailable. The
module does not copy the joining or validation rules.

## Architectural rules

- The Master Controller owns application runtime mutations.
- Slave Controllers own bounded services.
- The taskbar renders validated runtime records, not arbitrary directories.
- Application processes start through the Framework launcher and process
  supervisor.
- Launch previews never start a process and must be refreshed after the
  selection changes.
- Layouts remain semantic and toolkit-neutral.
- User and session layouts will be persisted through the Data Server.
- Linux kernel, boot, drivers and recovery remain outside this repository.

## Ownership

Project lead and author: Sammy Hegab
Organisation: Umicom Foundation
Licence: MIT
