# Melee Bounding Boxes

> **AI disclaimer**
>
> This project — the bounding-box modification, the stripped build tooling, and
> the changes layered on top of Training Mode - Community Edition — was created
> with the assistance of AI (an automated coding agent). It has **not** been
> reviewed or tested to the standard of the upstream project and may contain
> bugs or incorrect assumptions. Use at your own risk.
>
> The upstream [Training Mode - Community Edition](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition)
> project is human-authored; this disclaimer applies only to the modifications
> made here.

Melee Bounding Boxes is a minimal, near-vanilla Super Smash Bros. Melee build
that draws a screen-space bounding box around every fighter (one colour per
port) for image-classification / dataset capture. It is built on top of the
Training Mode - Community Edition toolchain (m-ex DLL loader and build scripts).

## What it changes

Compared to a vanilla NTSC 1.02 (`GALE01`) ISO, the only differences are:

- `Start.dol` is patched with the m-ex / gecko code loader.
- `codes.gct` contains a minimal hook set: the m-ex helper functions, a
  `StartMelee` hook, and the DLL loader.
- `TM/eventMenu.dat` contains a minimal `tmFunction` stub plus the bounding box
  module (`src/bbox.c`).

Everything else (menus, gameplay, movies, banner, etc.) is untouched vanilla.

## Bounding boxes

Each fighter gets an axis-aligned, screen-space rectangle drawn on top of the
world and HUD. The rectangle is the union of the fighter's hurtbox capsules,
projected through the match camera to screen space. Colours are per port:

- P1 magenta
- P2 yellow
- P3 orange
- P4 red

## Building

Requires devkitPro (`gamecube-dev`), `xdelta3`, and the bundled tools in `bin/`.

```
./build.sh path/to/vanilla-melee.iso
```

This produces `MeleeBoundingBoxes.iso`. Run `./clean.sh` to remove build output.

## Credits

Built on [Training Mode - Community Edition](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition)
by Alexander Harrison, which in turn builds on UnclePunch's training modpack and
Ploaj's m-ex. Those upstream projects are human-authored; see the disclaimer
above.
