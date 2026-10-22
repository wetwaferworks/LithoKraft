#!/usr/bin/env python3
# Game-origin server mirroring itch's player hosting: COOP same-origin +
# COEP require-corp + CORP cross-origin (so a cross-origin, isolated
# embedder may frame it and the frame itself can be cross-origin isolated).
import functools, http.server, mimetypes, os, sys
mimetypes.add_type("application/wasm", ".wasm"); mimetypes.add_type("text/javascript", ".js"); mimetypes.add_type("application/json", ".json")
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()
    def log_message(self, fmt, *args): pass
d = os.path.abspath(sys.argv[1]); port = int(sys.argv[2])
with http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(H, directory=d)) as s:
    print(f"itch-style game origin {d} at :{port}", flush=True); s.serve_forever()
