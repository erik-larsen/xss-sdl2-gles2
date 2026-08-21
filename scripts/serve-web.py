#!/usr/bin/env python3
"""Static server for the assembled web/ gallery with caching disabled.

  python3 scripts/serve-web.py [port] [directory]

Plain `python3 -m http.server` lets browsers heuristically cache
hack .js/.wasm (no Cache-Control header), which serves stale builds
during development. This sends no-store on everything.
"""
import functools, http.server, sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        self.send_header("Expires", "0")
        super().end_headers()


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8901
    directory = sys.argv[2] if len(sys.argv) > 2 else "web"
    handler = functools.partial(Handler, directory=directory)
    print(f"serving {directory}/ on http://localhost:{port} (no-store)")
    http.server.ThreadingHTTPServer(("", port), handler).serve_forever()
