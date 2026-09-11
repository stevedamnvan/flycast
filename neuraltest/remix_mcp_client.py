#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Minimal client for the RTX Remix Toolkit MCP server (standard library only).

The installed Toolkit (lightspeed.trex.mcp.core) serves MCP over SSE on
127.0.0.1:8000 while the Toolkit GUI runs, mounting its REST API as tools.
This client speaks the SSE transport: it opens the event stream, reads the
`endpoint` event, posts JSON-RPC requests to that endpoint and matches
responses arriving on the stream. It performs no installation and writes
nothing on disk; every tool call is an explicit request by the caller.
Discovery (`is_available`) is a bounded probe, never an assumption.
"""
import argparse
import http.client
import itertools
import json
import queue
import socket
import threading
import urllib.parse


class RemixMcp:
    def __init__(self, host='127.0.0.1', port=8000, timeout=60.0):
        self.host, self.port, self.timeout = host, port, timeout
        self._ids = itertools.count(1)
        self._events = queue.Queue()
        self._endpoint = None
        self._stream = None
        self._thread = None
        self.server = None

    @staticmethod
    def is_available(host='127.0.0.1', port=8000):
        try:
            with socket.create_connection((host, port), timeout=1.0):
                return True
        except OSError:
            return False

    def __enter__(self):
        self._stream = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        self._stream.request('GET', '/sse', headers={'Accept': 'text/event-stream'})
        response = self._stream.getresponse()
        if response.status != 200:
            raise RuntimeError(f'SSE endpoint returned {response.status}')
        self._thread = threading.Thread(target=self._pump, args=(response,), daemon=True)
        self._thread.start()
        kind, data = self._events.get(timeout=self.timeout)
        if kind != 'endpoint':
            raise RuntimeError('first SSE event was not the endpoint')
        self._endpoint = data
        self.server = self.request('initialize', dict(
            protocolVersion='2024-11-05', capabilities={},
            clientInfo=dict(name='flycast-remix-pilot', version='0')))
        self._notify('notifications/initialized', {})
        return self

    def __exit__(self, *exc):
        try:
            self._stream.close()
        finally:
            self._stream = None

    def _pump(self, response):
        event, data = None, []
        while True:
            try:
                line = response.readline()
            except Exception:  # stream closed by __exit__ or the server
                line = b''
            if not line:
                self._events.put(('closed', None))
                return
            line = line.decode('utf-8').rstrip('\r\n')
            if line.startswith('event:'):
                event = line[6:].strip()
            elif line.startswith('data:'):
                data.append(line[5:].strip())
            elif line == '':
                if data:
                    self._events.put((event or 'message', '\n'.join(data)))
                event, data = None, []

    def _post(self, payload):
        url = urllib.parse.urlsplit(self._endpoint)
        conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        conn.request('POST', url.path + ('?' + url.query if url.query else ''),
                     body=json.dumps(payload), headers={'Content-Type': 'application/json'})
        response = conn.getresponse()
        response.read()
        conn.close()
        if response.status not in (200, 202):
            raise RuntimeError(f'MCP message rejected with {response.status}')

    def _notify(self, method, params):
        self._post(dict(jsonrpc='2.0', method=method, params=params))

    def request(self, method, params):
        request_id = next(self._ids)
        self._post(dict(jsonrpc='2.0', id=request_id, method=method, params=params))
        while True:
            kind, data = self._events.get(timeout=self.timeout)
            if kind == 'closed':
                raise RuntimeError('SSE stream closed')
            if kind != 'message':
                continue
            message = json.loads(data)
            if message.get('id') != request_id:
                continue
            if 'error' in message:
                raise RuntimeError(f"MCP error: {message['error']}")
            return message['result']

    def tools(self):
        return self.request('tools/list', {})['tools']

    def call(self, tool_name, /, **arguments):
        result = self.request('tools/call', dict(name=tool_name, arguments=arguments))
        texts = [c.get('text', '') for c in result.get('content', []) if c.get('type') == 'text']
        if result.get('isError'):
            raise RuntimeError(f'{tool_name} failed: ' + ' '.join(texts))
        joined = '\n'.join(texts)
        try:
            return json.loads(joined) if joined else None
        except json.JSONDecodeError:
            return joined


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', type=int, default=8000)
    sub = p.add_subparsers(dest='command', required=True)
    sub.add_parser('probe', help='report whether the MCP server is listening')
    sub.add_parser('tools', help='list tools')
    call = sub.add_parser('call', help='call one tool with JSON arguments')
    call.add_argument('name')
    call.add_argument('arguments', nargs='?', default='{}')
    a = p.parse_args()
    if a.command == 'probe':
        print(json.dumps(dict(available=RemixMcp.is_available(port=a.port), port=a.port)))
        return
    with RemixMcp(port=a.port) as mcp:
        if a.command == 'tools':
            for tool in mcp.tools():
                print(tool['name'], '-', (tool.get('description') or '').split('\n')[0][:90])
        else:
            print(json.dumps(mcp.call(a.name, **json.loads(a.arguments)), indent=1))


if __name__ == '__main__':
    main()
