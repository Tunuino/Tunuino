#!/usr/bin/env python3
"""Read-only USB serial signature probe for Tunuino Console."""
import argparse

def probe(port, baudrate=115200, timeout=1.0, serial_factory=None):
    """Send the TunerStudio S query and read a bounded ASCII identity."""
    if serial_factory is None:
        try:
            from serial import Serial
        except ImportError as exc:
            raise RuntimeError("Install pyserial to use USB serial probing") from exc
        serial_factory = Serial
    if not 0 < timeout <= 10:
        raise ValueError("Timeout must be between 0 and 10 seconds")
    with serial_factory(port=port, baudrate=baudrate, timeout=timeout,
                        write_timeout=timeout) as link:
        link.reset_input_buffer()
        if link.write(b"S") != 1:
            raise IOError("USB serial query was not fully sent")
        raw = link.read(96)
    if not raw or any(byte < 32 or byte > 126 for byte in raw):
        raise ValueError("No valid printable ECU signature received")
    return raw.decode("ascii")

def main():
    parser = argparse.ArgumentParser(description="Read ECU identity over USB CDC serial")
    parser.add_argument("port", help="e.g. COM4 or /dev/ttyACM0")
    parser.add_argument("--baudrate", type=int, default=115200)
    args = parser.parse_args()
    try:
        identity = probe(args.port, args.baudrate)
    except (OSError, ValueError, RuntimeError) as exc:
        parser.exit(1, f"USB signature probe failed: {exc}\n")
    print(f"ECU signature: {identity}")
    print("Tunuino profile" if identity.startswith("Tunuino") else
          "External ECU profile: do not use Tunuino INI without matching signature")

if __name__ == "__main__":
    main()
