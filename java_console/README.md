# Tunuino Java Console

The desktop Console is being migrated incrementally, preserving upstream functionality and required licensing notices.

## Branding

The `branding` module defines the product title and dedicated Tunuino image resource paths. The actual PNG artwork and Swing desktop UI integration are **not yet present**; these constants alone do not replace the upstream icons. Upstream uses `trigger-ui/BasicLogoHelper.java`, `trigger-ui/LogoHelper.java` and `core_ui` image resources. Replace those integration points when their modules are migrated.

## Available modules

- `logging-api`: upstream logging implementation
- `logging`: upstream file-output utilities adapted to Tunuino namespace
- `branding`: Tunuino application naming and resource contract

Run `./gradlew build` to validate currently registered modules.
