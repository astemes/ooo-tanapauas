#!/usr/bin/env python3
"""Send Wi-Fi settings to the box over USB, instead of typing them on screen.

Run it yourself in a terminal while the box is plugged in:
    python3 firmware/tools/set_wifi.py
It asks for the network name and password (the password isn't echoed) and
prints the box's IP address when it connects. Needs pyserial (PlatformIO's
Python has it; otherwise `pip3 install pyserial`).
"""
import getpass
import glob
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: run `pip3 install pyserial`, or use PlatformIO's python.")


def find_port(wait_s: float = 15) -> str:
    """The box's port can briefly vanish while it restarts, so wait for it."""
    deadline = time.time() + wait_s
    while time.time() < deadline:
        ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
        if ports:
            return ports[0]
        time.sleep(0.5)
    sys.exit("No box found. Plug in the GIGA's USB-C cable and try again.")


def scan(box) -> list:
    """Ask the box which networks it can see."""
    box.reset_input_buffer()
    box.write(b"SCAN\n")
    nets, deadline = [], time.time() + 15
    while time.time() < deadline:
        line = box.readline().decode(errors="replace").rstrip("\r\n")
        if line == "SCAN END":
            break
        if line.startswith("NET "):
            parts = line.split(" ", 4)
            if len(parts) == 5:
                nets.append((int(parts[1]), parts[2], parts[3], parts[4]))
    return sorted(nets, reverse=True)


def main() -> None:
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    with serial.Serial(port, 115200, timeout=1) as box:
        time.sleep(1.0)
        print("Looking for networks...")
        nets = []
        for _ in range(3):  # the first scan after opening the port can come back empty
            nets = scan(box)
            if nets:
                break
            time.sleep(1)
        for i, (rssi, ch, sec, name) in enumerate(nets, 1):
            print(f"  {i}. {name}   ({rssi} dBm, {sec})")
        if not nets:
            print("  The box can't see any networks right now.")

        answer = input("Pick a number, or type the network name: ").strip()
        names = [n[3] for n in nets]
        if answer.isdigit() and 1 <= int(answer) <= len(names):
            ssid = names[int(answer) - 1]
        else:
            ssid = answer
            close = [n for n in names if n.lower() == ssid.lower()]
            if ssid not in names and close:
                ssid = close[0]
            elif ssid not in names:
                print("That name isn't in the list; trying anyway (hidden network?).")
        print(f"Network: {ssid}")
        password = getpass.getpass("Wi-Fi password: ")
        if not ssid or "\t" in ssid + password or len(ssid) > 32 or len(password) > 64:
            sys.exit("The network name must be 1-32 characters and the password at most 64, without tabs.")

        box.reset_input_buffer()
        box.write(f"WIFI\t{ssid}\t{password}\n".encode())
        print("Connecting (up to 30 s)...")
        deadline = time.time() + 40
        while time.time() < deadline:
            line = box.readline().decode(errors="replace").strip()
            if line.startswith("WIFI OK"):
                ip = line.split()[-1]
                print(f"Connected. Open http://{ip}/ on your phone, or scan the code on the box.")
                return
            if line.startswith("WIFI FAIL"):
                print("The box couldn't connect: " + line[len("WIFI FAIL "):])
                nets = scan(box)
                print("\nNetworks the box can see (signal, channel, security, name):")
                for rssi, ch, sec, name in nets:
                    mark = "  <- yours" if name == ssid else ("  <- similar name?" if name.strip().lower() == ssid.strip().lower() else "")
                    print(f"  {rssi:>4} dBm  ch {ch:>3}  {sec:<5} {name!r}{mark}")
                if not nets:
                    print("  (none)")
                elif ssid not in [n[3] for n in nets]:
                    print("\nYour network isn't in the list. It may be 5 GHz only, or the name differs (check capitals and spaces).")
                sys.exit(1)
    sys.exit("No answer from the box. Is the new firmware flashed?")


if __name__ == "__main__":
    main()
