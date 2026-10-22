#!/usr/bin/env python3
"""Static file server for local LithoKraft WASM build testing.

Serves a directory with the cross-origin isolation headers browsers require
before they will expose SharedArrayBuffer (which Emscripten pthreads need):

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

Also serves .wasm with the correct application/wasm MIME type so browsers
accept streaming compilation. Dependency-free (http.server only).

Usage:
    python3 serve.py <directory> [port]     # port defaults to 8080

Then open http://localhost:<port>/ in a browser.
"""
import functools
import http.server
import mimetypes
import os
import sys

mimetypes.add_type("application/wasm", ".wasm")
mimetypes.add_type("text/javascript", ".js")
mimetypes.add_type("application/json", ".json")


class IsolatedHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        # Local testing: never let the browser cache a stale build.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):  # keep the console readable
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    directory = os.path.abspath(sys.argv[1])
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    if not os.path.isdir(directory):
        print(f"error: not a directory: {directory}")
        return 2
    handler = functools.partial(IsolatedHandler, directory=directory)
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as httpd:
        print(f"Serving {directory} at http://localhost:{port}/ "
              f"(cross-origin isolated; Ctrl+C to stop)")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
