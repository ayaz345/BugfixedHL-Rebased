#!/usr/bin/env python3
# -------------------------------------------------------------
# VersionServer.py
# This script emulates GitHub API to list specific releases.
# Used to test update checker and auto-updater.
# -------------------------------------------------------------

import http.server
import socketserver
import json

IP = "127.0.0.1"
PORT = 8000
LOCAL_URL = f'http://{IP}:{PORT}/'

VERSIONS = [
    # From newest to oldest
    '1.6.1',
    '1.1.0',
    '1.0.1',
    '1.0.0',
]

ASSET_NAMES = [
    'client-windows',
    'server-windows',
    'client-linux',
    'server-linux'
]


class HTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path.startswith('/releases'):
            super().do_GET()
        elif self.path.startswith('/repos/tmp64/BugfixedHL-Rebased/releases'):
            self.handle_releases()
        else:
            self.send_error(http.HTTPStatus.NOT_FOUND, 'File not found')

    def handle_releases(self):
        response = []
        for release_id, version in enumerate(VERSIONS, start=1):
            release = {
                'id': release_id,
                'tag_name': f'v{version}',
                'name': f'v{version}: Test release',
                'body': f'A description for {version} release.',
                'draft': False,
                'assets': [],
            }

            for asset_id, asset_name in enumerate(ASSET_NAMES, start=1):
                name = f'BugfixedHL-{version}-{asset_name}.zip'
                asset = {
                    'id': asset_id,
                    'browser_download_url': f'{LOCAL_URL}releases/{name}',
                    'name': name,
                }

                release['assets'].append(asset)

            response.append(release)

        json_response = json.dumps(response)
        self.send_response(http.HTTPStatus.OK)
        self.send_header("Content-type", 'application/json')
        self.send_header("Content-Length", str(len(json_response)))
        self.end_headers()
        self.wfile.write(json_response.encode('UTF-8'))
        self.wfile.flush()


handler = HTTPRequestHandler

with socketserver.TCPServer((IP, PORT), handler) as httpd:
    print("serving at port", PORT)
    httpd.serve_forever()
