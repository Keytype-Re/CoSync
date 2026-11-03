"""Linux integration tests for login, chat, file synchronization and version history."""

import os
import ctypes
import re
import select
import shutil
import socket
import subprocess
import tempfile
import time
import unittest
from contextlib import ExitStack
from pathlib import Path


class Packet(ctypes.Structure):
    _fields_ = [("flag", ctypes.c_int), ("username", ctypes.c_char * 50),
                ("message", ctypes.c_char * 4096), ("file_data", ctypes.c_char * 4096)]


def receive(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise EOFError("connection closed before the expected reply")
        data.extend(chunk)
    return bytes(data)


class CollaborationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build_dir = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.build_dir.cleanup)
        root = Path(cls.build_dir.name)
        source = Path(__file__).resolve().parent.parent
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            cls.port = probe.getsockname()[1]
        port = cls.port
        # Only the test copies use a loopback address and a temporary port.
        for shared in ("protocol.h", "protocol.c"):
            shutil.copy2(source / shared, root / shared)
        for side in ("server", "client"):
            build = root / side
            build.mkdir()
            for path in (source / side).glob("*.c"):
                shutil.copy2(path, build)
            shutil.copy2(source / side / "Makefile", build)
            shutil.copytree(source / side / "includes", build / "includes")
            header = build / "includes" / "common.h"
            code, count = re.subn(r"(?m)^#define PORT \d+$", f"#define PORT {port}",
                                  header.read_text(encoding="utf-8"))
            if count != 1:
                raise ValueError("could not set test port")
            header.write_text(code, encoding="utf-8")
            if side == "server":
                main = build / "main.c"
                code = main.read_text(encoding="utf-8")
                if code.count("= INADDR_ANY;") != 1:
                    raise ValueError("could not set test address")
                code = code.replace("= INADDR_ANY;", "= htonl(INADDR_LOOPBACK);")
                main.write_text(code, encoding="utf-8")
            subprocess.run(["make", "-B", "-C", str(build)], check=True)

    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        build = Path(self.build_dir.name)
        self.client = build / "client" / "client_app"
        self.state = self.root / "server-state"
        self.state.mkdir()
        self.users = self.state / "user.txt"
        self.users.write_text("returning\n")
        self.server = subprocess.Popen([build / "server" / "server_app"],
                                       cwd=self.state, stdout=subprocess.DEVNULL)
        self.stack.callback(self.server.wait)
        self.stack.callback(self.server.kill)
        deadline = time.monotonic() + 5
        while True:
            try:
                self.first = self.connect()
                break
            except ConnectionRefusedError:
                if self.server.poll() is not None or time.monotonic() >= deadline:
                    raise
                time.sleep(0.02)

    def connect(self):
        sock = socket.create_connection(("127.0.0.1", self.port), timeout=3)
        self.stack.callback(sock.close)
        return sock

    def test_login_limit(self):
        def complete_registration(sock):
            self.assertEqual(receive(sock, 10), b"REGISTERED")
            sock.sendall(b"INIT_REQUEST")
            for flag in (1, 2):
                packet = Packet.from_buffer_copy(receive(sock, ctypes.sizeof(Packet)))
                self.assertEqual(packet.flag, flag)

        peers = []
        for i in range(10):
            sock = self.first if i == 0 else self.connect()
            sock.sendall(bytes(Packet(0, f"user{i}".encode())))
            complete_registration(sock)
            peers.append(sock)

        denied = self.connect()
        denied.sendall(bytes(Packet(0, b"overflow")))
        self.assertEqual(receive(denied, 1), b"F")
        self.assertEqual(denied.recv(1), b"")

        for returning in (False, True):
            client_state = self.root / f"client-{returning}"
            client_state.mkdir()
            if returning:
                (client_state / "user.txt").write_text("returning\n")
            result = subprocess.run([self.client], cwd=client_state,
                                    env={**os.environ, "COSYNC_SERVER_ADDR": "127.0.0.1"},
                                    input="overflow\n", capture_output=True, text=True,
                                    timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Server is full", result.stderr)
            self.assertNotIn("Login Complete", result.stdout)
            self.assertEqual((client_state / "user.txt").exists(), returning)
        self.assertNotIn("overflow", self.users.read_text().splitlines())

        peers[0].sendall(bytes(Packet(1, b"user0", b"still connected")))
        for sock in peers[1:]:
            packet = Packet.from_buffer_copy(receive(sock, ctypes.sizeof(Packet)))
            self.assertEqual((packet.flag, packet.message), (1, b"still connected"))

        peers[-1].close()
        deadline = time.monotonic() + 3
        while True:
            sock = self.connect()
            sock.sendall(bytes(Packet(0, b"overflow")))
            if sock.recv(1, socket.MSG_PEEK) != b"F":
                break
            sock.close()
            self.assertLess(time.monotonic(), deadline, "released slot was never reused")
        complete_registration(sock)
        self.assertIn("overflow", self.users.read_text().splitlines())

    def test_files_versions_and_shutdown(self):
        # Fragment a signup handshake while another peer is already sending.
        a = self.first
        a.sendall(bytes(Packet(1, b"returning")))
        self.assertEqual(self.packet(a).flag, 1)
        self.assertEqual(self.packet(a).flag, 2)
        b = self.connect()
        b.sendall(bytes(Packet(0, b"peer")))
        self.assertEqual(receive(b, 10), b"REGISTERED")
        b.sendall(b"INIT_")
        a.sendall(bytes(Packet(1, b"forged", b"during login")))
        log = self.state / "chat_log.txt"
        self.wait_until(lambda: log.exists() and "during login" in log.read_text())
        b.sendall(b"REQUEST")
        self.assertIn(b"returning, during login", self.packet(b).message)
        self.assertEqual(self.packet(b).flag, 2)

        data = b"a" * 4095
        a.sendall(bytes(Packet(2, b"forged", b"", data)))
        update = self.packet(b)
        self.assertEqual((update.username, update.file_data), (b"returning", data))
        versions = self.state / "version_logs"
        (versions / "3_shared_file.txt").write_bytes(b"old")
        a.sendall(bytes(Packet(3, b"", b"/commit snapshot", b"saved")))
        self.assertIn(b"version 4", self.packet(a).message)
        self.assertEqual((versions / "3_shared_file.txt").read_bytes(), b"old")
        self.assertEqual((versions / "4_shared_file.txt").read_bytes(), b"saved")
        a.sendall(bytes(Packet(3, b"", b"/rebase 4")))
        for peer in (a, b):
            self.assertEqual(self.packet(peer).file_data, b"saved")
        # A listing larger than both the old 100-entry cap and one wire packet.
        for n in range(5, 126):
            (versions / f"{n}_shared_file.txt").touch()
        listed = set()
        a.sendall(bytes(Packet(3, b"", b"/log")))
        a.sendall(bytes(Packet(1, b"", b"listing finished")))
        self.assertEqual(self.packet(b).message, b"listing finished")
        a.sendall(bytes(Packet(3, b"", b"/bad")))
        while True:
            page = self.packet(a)
            if page.flag == 1:
                break
            self.assertEqual(page.flag, 3)
            listed.update(int(n) for n in re.findall(rb"Version: (\d+)_shared_file.txt", page.file_data))
        self.assertEqual(listed, set(range(3, 126)))
        # Full, unterminated wire field: reject without replacing the server file.
        b.sendall(bytes(Packet(2, b"peer", b"", b"x" * 4096)))
        self.assertEqual(b.recv(1), b"")
        self.assertEqual((self.state / "shared_file.txt").read_bytes(), b"saved")
        pending = self.connect()
        pending.sendall(b"x")
        self.server.terminate()
        self.assertEqual(self.server.wait(timeout=5), 0)

    def test_signup_waits_for_input_and_reports_disconnect(self):
        self.server.terminate()
        self.assertEqual(self.server.wait(timeout=5), 0)
        listener = self.stack.enter_context(socket.socket())
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", self.port))
        listener.listen()
        listener.settimeout(3)
        proc = subprocess.Popen([self.client], cwd=self.root, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                env={**os.environ, "COSYNC_SERVER_ADDR": "127.0.0.1"})
        self.stack.callback(proc.wait)
        self.stack.callback(proc.kill)
        output = b""
        while b"Enter your username:" not in output:
            self.assertTrue(select.select([proc.stdout], [], [], 3)[0], "no username prompt")
            chunk = os.read(proc.stdout.fileno(), 4096)
            self.assertTrue(chunk, "client exited before prompting")
            output += chunk
        self.assertFalse(select.select([listener], [], [], 0)[0],
                         "the server handshake started while waiting for user input")
        proc.stdin.write(b"demo\n")
        proc.stdin.flush()
        conn, _ = listener.accept()
        with conn:
            self.assertEqual(self.packet(conn).username, b"demo")
        _, error = proc.communicate(timeout=5)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn(b"before replying to login", error)

    def test_real_client_file_events(self):
        peer = self.first
        peer.sendall(bytes(Packet(1, b"returning")))
        self.packet(peer)
        self.packet(peer)
        work = self.root / "client-state"
        work.mkdir()
        proc = subprocess.Popen([self.client], cwd=work, stdin=subprocess.PIPE,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                env={**os.environ, "COSYNC_SERVER_ADDR": "127.0.0.1"})
        self.stack.callback(proc.wait)
        self.stack.callback(proc.kill)
        self.stack.callback(proc.stdin.close)
        def line(data):
            proc.stdin.write(data + b"\n")
            proc.stdin.flush()
        line(b"cli")
        line(b"ready")
        self.assertEqual(self.packet(peer).message, b"ready")
        watch = work / "watch"
        (watch / "other.txt").write_bytes(b"private")
        replacement = work / "replacement"
        replacement.write_bytes(b"edited")
        replacement.replace(watch / "shared_file.txt")
        self.assertEqual(self.packet(peer).file_data, b"edited")
        for invalid in (b"x" * 4096, b"x" * 4097, b"a\0b"):
            (work / "large").write_bytes(invalid)
            line(b"/load large")
            line(b"after load")
            self.assertEqual(self.packet(peer).message, b"after load")
            self.assertEqual((watch / "shared_file.txt").read_bytes(), b"edited")
        # A symlink moved onto the watched name must not upload its target.
        secret = work / "local-only"
        secret.write_bytes(b"private")
        replacement.symlink_to(secret)
        replacement.replace(watch / "shared_file.txt")
        line(b"still here")
        self.assertEqual(self.packet(peer).message, b"still here")
        replacement.write_bytes(b"immediate")
        replacement.replace(watch / "shared_file.txt")
        line(b"/commit")
        snapshot = self.state / "version_logs" / "1_shared_file.txt"
        self.wait_until(snapshot.exists)
        self.assertEqual(snapshot.read_bytes(), b"immediate")
        self.server.terminate()
        self.assertEqual(self.server.wait(timeout=5), 0)
        proc.wait(timeout=5)  # stdin stays open; disconnect must wake the input thread.

    @staticmethod
    def packet(sock):
        return Packet.from_buffer_copy(receive(sock, ctypes.sizeof(Packet)))

    def wait_until(self, predicate):
        deadline = time.monotonic() + 3
        while not predicate():
            self.assertLess(time.monotonic(), deadline)
            time.sleep(0.01)


if __name__ == "__main__":
    unittest.main()
