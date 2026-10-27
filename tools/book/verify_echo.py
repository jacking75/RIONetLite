"""Exercise the built EchoServer over loopback; keep product execution separate from JS labs."""
from pathlib import Path
import argparse
import json
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def receive_exact(client, count):
    output = b""
    while len(output) < count:
        chunk = client.recv(count - len(output))
        if not chunk:
            raise RuntimeError("Echo closed before the complete response")
        output += chunk
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    args = parser.parse_args()
    output = ROOT / ".artifacts/book/echo" / args.configuration
    logs = ROOT / "logs/book_echo" / args.configuration
    output.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    stop_file = output / "stop.request"
    stop_file.unlink(missing_ok=True)
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    executable = ROOT / f"bin/x64/{args.configuration}/EchoServer.exe"
    receipt = {"configuration": args.configuration, "port": port, "checks": [], "forced": False}
    with (logs / "stdout.log").open("wb") as stdout, (logs / "stderr.log").open("wb") as stderr:
        process = subprocess.Popen([str(executable), "--port", str(port), "--workers", "2",
                                    "--max-sessions", "8", "--duration", "20",
                                    "--stop-file", str(stop_file)], stdout=stdout, stderr=stderr,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic() + 5
            while True:
                if process.poll() is not None:
                    raise RuntimeError("EchoServer exited before listening")
                try:
                    client = socket.create_connection(("127.0.0.1", port), timeout=1)
                    break
                except OSError:
                    if time.monotonic() > deadline:
                        raise RuntimeError("Echo listen deadline exceeded")
                    time.sleep(.025)
            with client:
                client.settimeout(3)
                packet = bytes.fromhex("00 00 00 07 72 69 6F")
                for fragment in (packet[:2], packet[2:5], packet[5:]):
                    client.sendall(fragment)
                    time.sleep(.01)
                assert receive_exact(client, 7) == packet
                receipt["checks"].append("2+3+2 fragmented request")
                combined = packet + bytes.fromhex("00 00 00 05 21")
                client.sendall(combined)
                assert receive_exact(client, len(combined)) == combined
                receipt["checks"].append("two coalesced requests")
                client.sendall(bytes.fromhex("00 00 00 03"))
                try:
                    assert client.recv(1) == b""
                except ConnectionResetError:
                    pass
                receipt["checks"].append("invalid total length closes connection")
        finally:
            stop_file.write_text("stop", encoding="ascii")
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                receipt["forced"] = True
                process.kill()
                process.wait()
            receipt["exitCode"] = process.returncode
            (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    if receipt["forced"] or process.returncode != 0:
        raise RuntimeError(f"Echo shutdown failed: {receipt}")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
