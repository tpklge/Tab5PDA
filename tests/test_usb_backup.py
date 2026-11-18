"""Test PC framing with a backup produced by the real C++ codec."""
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import usb_backup

FIXTURE = Path(sys.argv.pop(1)).read_bytes()


class Port:
    def __init__(self, data, corrupt=False):
        self.data = data
        self.corrupt = corrupt
        self.lines = []
        self.position = 0
        self.upload = bytearray()
        self.size = 0
        self.crc = 0
        self.commands = []

    def write(self, data):
        if data == b"PDA GET\n":
            self.commands.append(data)
            crc = (zlib.crc32(self.data) ^ int(self.corrupt)) & 0xffffffff
            self.lines.append(f"PDA DATA {len(self.data)} {crc:08x}\n".encode())
            self.lines.append(b"\nPDA END\n")
        elif data.startswith(b"PDA PUT ") and not self.size:
            self.commands.append(data)
            parts = data.split()
            self.size, self.crc = int(parts[2]), int(parts[3], 16)
            self.lines.append(b"PDA READY\n")
        else:
            self.upload.extend(data)
            if len(self.upload) == self.size:
                valid = zlib.crc32(self.upload) & 0xffffffff == self.crc
                self.lines.append(b"PDA STAGED\n" if valid else b"PDA ERROR invalid\n")
        return len(data)

    def readline(self):
        return self.lines.pop(0) if self.lines else b"PDA ERROR simulated\n"

    def read(self, count):
        count = min(count, 7)  # short reads must be accumulated.
        data = self.data[self.position:self.position + count]
        self.position += len(data)
        return data


class BackupTests(unittest.TestCase):
    def test_cpp_fixture_and_receive(self):
        usb_backup.validate(FIXTURE)
        port = Port(FIXTURE)
        self.assertEqual(usb_backup.receive(port), FIXTURE)
        self.assertEqual(port.commands, [b"PDA GET\n"])

    def test_upload_does_not_request_restore(self):
        port = Port(FIXTURE)
        usb_backup.send(port, FIXTURE)
        self.assertEqual(port.upload, FIXTURE)
        self.assertEqual(len(port.commands), 1)
        self.assertTrue(port.commands[0].startswith(b"PDA PUT "))

    def test_corrupted_receive(self):
        with self.assertRaises(ValueError):
            usb_backup.receive(Port(FIXTURE, corrupt=True))

    def test_invalid_upload_sends_nothing(self):
        port = Port(FIXTURE)
        with self.assertRaises(ValueError):
            usb_backup.send(port, FIXTURE[:-1])
        self.assertEqual(port.commands, [])

    def test_large_backup(self):
        # A valid framed payload above the old 32 KiB ceiling.
        rows = FIXTURE.splitlines(keepends=True)[:-1]
        rows.insert(-1, b'{"type":"extra","text":"' + b'x' * 40000 + b'"}\n')
        payload = b''.join(rows)
        data = payload + ('{"crc32":"%08x","type":"commit"}\n' % (zlib.crc32(payload) & 0xffffffff)).encode()
        usb_backup.validate(data)
        self.assertEqual(usb_backup.receive(Port(data)), data)
        port = Port(data)
        usb_backup.send(port, data)
        self.assertEqual(port.upload, data)

    def test_device_error(self):
        with self.assertRaises(RuntimeError):
            usb_backup.response(Port(FIXTURE), "PDA READY")


if __name__ == "__main__":
    unittest.main()
