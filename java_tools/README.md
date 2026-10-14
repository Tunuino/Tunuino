# Tunuino Java tools

Gradle modules:

- `version`: shared Tunuino Java version information.
- `bin2header`: converts binary firmware resources into C/C++ headers. Source adapted from upstream, with the Tunuino Java package name.

Build both using `./gradlew build`. Run the converter using `./gradlew :bin2header:run --args='input.bin output.h "const unsigned char data[]"'`.

The full desktop Console and other upstream tools are not yet integrated.
