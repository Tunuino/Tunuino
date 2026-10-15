# Tunuino Java Console

The desktop Console is being migrated incrementally, preserving upstream functionality and required licensing notices.

## Branding

The `branding` module provides blue-and-white Java2D artwork, reusable Swing window branding and a standalone preview (`./gradlew :branding:preview`). It does not yet provide the complete ECU Console.

The `trigger-ui` module now contains the first migrated logo integration points, `BasicLogoHelper` and `LogoHelper`, wired to the Tunuino artwork. The upstream trigger UI functionality and its other dependencies are **not yet migrated**. Existing hardware-specific logos are not imported; all variants currently use the Tunuino logo.

## Available modules

- `logging-api`: upstream logging implementation
- `logging`: upstream file-output utilities adapted to Tunuino namespace
- `branding`: Tunuino application identity and Swing artwork
- `trigger-ui`: initial logo integration classes only

Run `./gradlew build` to validate currently registered modules.
