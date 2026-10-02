#!/usr/bin/env python3
"""
Legacy VC71 score fields describe mnemonic similarity, NOT raw byte accuracy.


SSE-powered live progress dashboard server.

Serves static files from artifacts/progress/ and pushes live updates
via Server-Sent Events whenever report.json changes.

Usage:
    python3 tools/report/progress_server.py [--port 8080] [--directory artifacts/progress]
"""

import os
import sys
import time
import json
import argparse
import logging
import subprocess
import threading
import tempfile
import uuid
from http.server import HTTPServer, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn

_tools_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

from report.atomic_write import write_json_atomic

# Equivalence lock: one raw-XBE differential run per function at a time.
_equivalence_locks = {}
_equivalence_locks_mu = threading.Lock()

# Raw-XBE structural audit lock: one populate run per unit at a time.
_raw_audit_locks = {}
_raw_audit_locks_mu = threading.Lock()

# SSE client tracking, for visibility into how many browsers are connected
_sse_clients = 0
_sse_clients_mu = threading.Lock()

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)




def _recompute_unit_match(unit: dict) -> None:
    """Recompute unit match from scored, ported functions."""
    scores = []
    weighted_sum = 0.0
    weighted_bytes = 0
    for func in unit.get('functions', []):
        mp = func.get('match_percent')
        if not func.get('ported') or mp is None:
            continue
        size = func.get('size') or 0
        scores.append(mp)
        weighted_sum += mp * size
        weighted_bytes += size
    summary = unit.setdefault('summary', {})
    if scores:
        summary['match_avg'] = round(sum(scores) / len(scores), 1)
        summary['match_weighted'] = round(weighted_sum / max(weighted_bytes, 1), 1)
    else:
        summary['match_avg'] = None
        summary['match_weighted'] = None


def _recompute_summary_match(report: dict) -> None:
    """Recompute overall match from scored, ported functions."""
    total_sum = 0.0
    weighted_sum = 0.0
    weighted_bytes = 0
    count = 0
    for unit in report.get('units', []):
        for func in unit.get('functions', []):
            mp = func.get('match_percent')
            if not func.get('ported') or mp is None:
                continue
            size = func.get('size') or 0
            total_sum += mp
            weighted_sum += mp * size
            weighted_bytes += size
            count += 1
    if count > 0:
        report.setdefault('summary', {})['match'] = {
            'average': round(total_sum / count, 1),
            'weighted': round(weighted_sum / max(weighted_bytes, 1), 1),
            'scored_count': count,
        }


class SSEHandler(SimpleHTTPRequestHandler):
    """HTTP handler that also serves an SSE endpoint at /events."""

    def do_GET(self):
        if self.path == '/events':
            self.handle_sse()
        else:
            try:
                super().do_GET()
            except (BrokenPipeError, ConnectionResetError, OSError):
                # Clients can disconnect mid-response (e.g. browser refresh/abort).
                # Treat this as normal and avoid noisy socketserver tracebacks.
                pass

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def do_POST(self):
        if self.path == '/api/equivalence':
            self.handle_equivalence()
        elif self.path == '/api/raw-audit':
            self.handle_raw_audit()
        else:
            self.send_error(404, 'Not found')

    def _json_response(self, status, body):
        data = json.dumps(body).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(data)

    def handle_equivalence(self):
        """Re-run a displayed divergence against the raw pristine-XBE oracle."""
        try:
            length = int(self.headers.get('Content-Length', 0))
            body = json.loads(self.rfile.read(length) if length else b'{}')
        except (ValueError, json.JSONDecodeError) as e:
            self._json_response(400, {'error': f'Bad request: {e}'})
            return

        function_name = body.get('function')
        address = body.get('address')
        if not isinstance(function_name, str) or not isinstance(address, str):
            self._json_response(400, {'error': 'Missing "function" or "address" field'})
            return

        report_path = os.path.join(self.directory, 'report.json')
        try:
            with open(report_path) as f:
                report = json.load(f)
        except Exception as e:
            logging.error('Cannot read report.json: %s', e)
            self._json_response(500, {'error': 'report_unreadable'})
            return

        function = None
        for unit in report.get('units', []):
            for candidate in unit.get('functions', []):
                if (candidate.get('name') == function_name and
                        str(candidate.get('address', '')).lower() == address.lower()):
                    function = candidate
                    break
            if function is not None:
                break
        if function is None:
            self._json_response(404, {'error': 'function_not_found'})
            return

        if not (function.get('ported') and function.get('equiv_status') == 'fail' and
                function.get('equiv_reason') == 'divergence' and
                function.get('equiv_confidence') in ('high', 'moderate')):
            self._json_response(409, {'error': 'not_divergent'})
            return

        logging.info('Equivalence request for %s at %s from %s', function_name,
                     address, self.client_address[0])
        lock_key = address.lower()
        with _equivalence_locks_mu:
            lock = _equivalence_locks.setdefault(lock_key, threading.Lock())

        if lock.locked():
            logging.info('Function %s is already running equivalence; waiting', function_name)
        with lock:
            result = self._run_equivalence(function_name, address)

        if result is None:
            self._json_response(409, {'error': 'no_raw_xbe_oracle'})
            return

        if not self._refresh_dashboard():
            self._json_response(500, {'error': 'dashboard_refresh_failed', 'result': result})
            return
        self._json_response(200, {'ok': True, 'function': function_name,
                                  'address': address, 'result': result})

    def handle_raw_audit(self):
        """Run the raw-XBE structural audit for every ported function in a unit.

        This is what produces the dashboard's aligned-byte numbers.  The
        generator discards an audit whose source hash no longer matches, so
        any edit to the TU needs a fresh run.
        """
        try:
            length = int(self.headers.get('Content-Length', 0))
            body = json.loads(self.rfile.read(length) if length else b'{}')
        except (ValueError, json.JSONDecodeError) as e:
            self._json_response(400, {'error': f'Bad request: {e}'})
            return

        unit_name = body.get('unit')
        if not isinstance(unit_name, str) or not unit_name:
            self._json_response(400, {'error': 'Missing "unit" field'})
            return

        report_path = os.path.join(self.directory, 'report.json')
        try:
            with open(report_path) as f:
                report = json.load(f)
        except Exception as e:
            logging.error('Cannot read report.json: %s', e)
            self._json_response(500, {'error': 'report_unreadable'})
            return

        unit = next((u for u in report.get('units', [])
                     if u.get('name') == unit_name), None)
        if unit is None:
            self._json_response(404, {'error': 'unit_not_found'})
            return
        source_path_rel = unit.get('source_path')
        if unit.get('synthetic') or not source_path_rel:
            self._json_response(409, {'error': 'no_source'})
            return

        logging.info('Raw-XBE audit request for unit %s from %s', unit_name,
                     self.client_address[0])
        with _raw_audit_locks_mu:
            lock = _raw_audit_locks.setdefault(unit_name, threading.Lock())
        if lock.locked():
            logging.info('Unit %s is already auditing; waiting for it to finish', unit_name)
        with lock:
            result = self._run_raw_audit(unit_name, source_path_rel)

        if result is None:
            self._json_response(500, {'error': 'audit_failed', 'unit': unit_name})
            return
        if not self._refresh_dashboard():
            self._json_response(500, {'error': 'dashboard_refresh_failed', 'result': result})
            return
        self._json_response(200, {'ok': True, 'unit': unit_name, 'result': result})

    def _run_raw_audit(self, unit_name, source_path_rel):
        """Freshly compile byte_regression.measure for one TU with complete input provenance.

        A subprocess, not an import: populate owns a process pool and the
        module-level ROOT, and a crash there must not take the server down.
        """
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, '../..'))
        script = os.path.join(project_root, 'tools', 'verify', 'byte_regression.py')
        parent = os.path.join(project_root, 'artifacts', 'byte_measurements')
        os.makedirs(parent, exist_ok=True)
        output = tempfile.mkdtemp(prefix='dashboard-', dir=parent)
        plan_path = os.path.join(output, 'plan.json')
        with open(plan_path, 'w') as stream:
            json.dump({'sources': [source_path_rel]}, stream)
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=project_root, text=True).strip()
        cmd = [sys.executable, script, 'measure', '--commit', commit, '--run-id', uuid.uuid4().hex,
               '--plan', plan_path, '--output', os.path.join(output, 'snapshot.json'),
               '--allow-dirty', '--publish']
        logging.info('Running raw-XBE audit for %s ...', source_path_rel)
        t_start = time.time()
        try:
            r = subprocess.run(cmd, cwd=project_root, capture_output=True,
                               text=True, timeout=1800)
        except subprocess.TimeoutExpired:
            logging.error('Raw-XBE audit timed out for unit %s', unit_name)
            return None
        elapsed = time.time() - t_start
        # Failed measurements are errors; no stale record is treated as a new result.
        if r.returncode != 0:
            logging.error('Raw-XBE audit failed for unit %s (rc=%d, %.1fs): %s',
                          unit_name, r.returncode, elapsed, r.stderr[-2000:])
            return None
        tail = r.stdout.strip().splitlines()
        totals = None
        if tail:
            try:
                totals = json.loads(tail[-1])
            except json.JSONDecodeError:
                pass
        logging.info('Raw-XBE audit finished for %s in %.1fs (rc=%d): %s',
                     source_path_rel, elapsed, r.returncode, totals)
        return {'returncode': r.returncode, 'elapsed': round(elapsed, 1),
                'totals': totals}

    def _run_equivalence(self, function_name, address):
        """Run the standard single-target batch verifier against the raw XBE."""
        from pathlib import Path

        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, '../..'))
        equivalence_dir = os.path.join(project_root, 'tools', 'equivalence')
        if equivalence_dir not in sys.path:
            sys.path.insert(0, equivalence_dir)
        try:
            import batch_verify as _equiv
        except ImportError as e:
            logging.error('Cannot import batch_verify: %s', e)
            return None

        candidates = _equiv.load_candidates(discover=True, oracle='xbe')
        candidate = next((item for item in candidates
                          if item['name'] == function_name and
                          item['addr'].lower() == address.lower()), None)
        if candidate is None:
            logging.warning('No raw-XBE equivalence candidate for %s at %s',
                            function_name, address)
            return None

        output_dir = Path(project_root) / 'artifacts' / 'batch_verify'
        output_dir.mkdir(parents=True, exist_ok=True)
        fingerprint = _equiv.candidate_fingerprint(_equiv.input_fingerprint(),
                                                    candidate, 'xbe')
        logging.info('Running raw-XBE equivalence for %s ...', function_name)
        return _equiv.run_verify(function_name, output_dir, seeds=50, timeout=60,
                                 float_tolerance=32, update_leaf_cache=False,
                                 input_fingerprint=fingerprint, oracle='xbe')

    def _refresh_dashboard(self):
        """Regenerate the report so a fresh verdict or audit reaches SSE clients."""
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(script_dir, '../..'))
        report_dir = os.path.join(project_root, 'tools', 'report')
        if report_dir not in sys.path:
            sys.path.insert(0, report_dir)
        try:
            import generate_decomp_report as _report
            report_path = os.path.join(self.directory, 'report.json')
            html_path = os.path.join(self.directory, 'index.html')
            history_path = os.path.join(self.directory, 'history.json')
            report = _report.generate_report(report_path)
            _report.generate_html(report, html_path, history_path)
            os.utime(report_path, None)
            return True
        except Exception as e:
            logging.error('Dashboard refresh failed: %s', e)
            return False

    def log_message(self, format, *args):
        logging.info("%s - %s", self.client_address[0], format % args)

    def handle_sse(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Cache-Control', 'no-cache, no-transform')
        self.send_header('Connection', 'keep-alive')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('X-Accel-Buffering', 'no')
        self.end_headers()

        report_path = os.path.join(self.directory, 'report.json')
        # Also watch history.json
        history_path = os.path.join(self.directory, 'history.json')
        watched = {
            report_path: 0,
            history_path: 0,
        }

        global _sse_clients
        with _sse_clients_mu:
            _sse_clients += 1
            active = _sse_clients
        logging.info('SSE client connected from %s (%d active)', self.client_address[0], active)

        def send_field(name, data):
            msg = f'event: {name}\ndata: {data}\n\n'
            try:
                self.wfile.write(msg.encode('utf-8'))
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                return False
            return True

        try:
            # Send initial state
            for path in (report_path, history_path):
                if os.path.exists(path):
                    with open(path) as f:
                        content = f.read()
                    name = 'report' if 'report' in path else 'history'
                    watched[path] = os.path.getmtime(path)
                    if not send_field(name, content):
                        return

            # Poll for changes
            while True:
                time.sleep(1)
                for path in list(watched):
                    if os.path.exists(path):
                        mtime = os.path.getmtime(path)
                        if mtime > watched[path]:
                            watched[path] = mtime
                            with open(path) as f:
                                content = f.read()
                            name = 'report' if 'report' in path else 'history'
                            if not send_field(name, content):
                                return
                            logging.info('Pushed update for %s', os.path.basename(path))
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            with _sse_clients_mu:
                _sse_clients -= 1
                active = _sse_clients
            logging.info('SSE client disconnected from %s (%d active)', self.client_address[0], active)


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


def _background_regen(serve_dir: str, project_root: str, interval_secs: int = 300) -> None:
    """Periodically regenerate CI status + main report so the dashboard stays fresh."""
    venv_py = os.path.join(project_root, '.venv', 'bin', 'python3')
    py = venv_py if os.path.exists(venv_py) else sys.executable
    ci_script = os.path.join(project_root, 'tools', 'report', 'generate_ci_status.py')
    report_script = os.path.join(project_root, 'tools', 'report', 'generate_decomp_report.py')
    report_json = os.path.join(serve_dir, 'report.json')
    report_html = os.path.join(serve_dir, 'index.html')

    while True:
        time.sleep(interval_secs)
        logging.info('Background dashboard refresh starting...')
        t_start = time.time()
        try:
            r = subprocess.run(
                [py, ci_script, '--output-dir', serve_dir],
                cwd=project_root, capture_output=True, timeout=30,
            )
            if r.returncode == 0:
                r2 = subprocess.run(
                    [py, report_script, '--output', report_json, '--html', report_html],
                    cwd=project_root, capture_output=True, timeout=60,
                )
                if r2.returncode == 0:
                    logging.info('Dashboard auto-refreshed in %.1fs', time.time() - t_start)
                else:
                    logging.warning('Report regen failed (rc=%d, %.1fs): %s',
                                     r2.returncode, time.time() - t_start,
                                     r2.stderr.decode(errors='replace')[:200])
            else:
                logging.warning('CI regen failed (rc=%d, %.1fs): %s',
                                 r.returncode, time.time() - t_start,
                                 r.stderr.decode(errors='replace')[:200])
        except Exception as exc:
            logging.warning('Background regen error (%.1fs): %s', time.time() - t_start, exc)


def _background_raw_audit(project_root: str, interval_secs: int, workers: int) -> None:
    """Periodically re-run the raw-XBE structural audit for the whole project.

    Any source edit invalidates a function's audit, so without this the audited
    set shrinks and the aligned-byte history line tracks coverage instead of
    accuracy.  The next dashboard refresh picks the new records up.
    """
    venv_py = os.path.join(project_root, '.venv', 'bin', 'python3')
    py = venv_py if os.path.exists(venv_py) else sys.executable
    script = os.path.join(project_root, 'tools', 'verify', 'raw_xbe_structural.py')

    while True:
        time.sleep(interval_secs)
        logging.info('Background raw-XBE audit starting (%d workers)...', workers)
        t_start = time.time()
        try:
            r = subprocess.run(
                [py, script, 'populate', '--workers', str(workers)],
                cwd=project_root, capture_output=True, text=True,
                timeout=max(interval_secs, 1800),
            )
            # rc 2 = some TUs failed to compile; the rest are still recorded.
            if r.returncode in (0, 2):
                logging.info('Background raw-XBE audit finished in %.1fs (rc=%d)',
                             time.time() - t_start, r.returncode)
            else:
                logging.warning('Background raw-XBE audit failed (rc=%d, %.1fs): %s',
                                r.returncode, time.time() - t_start, r.stderr[-200:])
        except Exception as exc:
            logging.warning('Background raw-XBE audit error (%.1fs): %s',
                            time.time() - t_start, exc)


def main():
    parser = argparse.ArgumentParser(
        description='SSE-powered live progress dashboard server'
    )
    parser.add_argument(
        '--port', type=int, default=8080,
        help='Port to serve on (default: 8080)'
    )
    parser.add_argument(
        '--directory', default='artifacts/progress',
        help='Directory to serve (default: artifacts/progress)'
    )
    parser.add_argument(
        # Loopback by default.  /api/raw-audit is an unauthenticated endpoint that
        # compiles source and rewrites tracked score files, so binding it to
        # every interface hands that to anyone on the network.  Pass an explicit
        # --host to expose it (and put access control in front of it first).
        '--host', default='127.0.0.1',
        help='Host to bind to (default: 127.0.0.1; /api/raw-audit is unauthenticated)'
    )
    parser.add_argument(
        '--raw-audit-interval', type=int, default=3600,
        help='Seconds between whole-project raw-XBE audits that keep aligned-byte '
             'coverage current (default: 3600; 0 disables)'
    )
    parser.add_argument(
        '--raw-audit-workers', type=int, default=4,
        help='Parallel TU workers for the background raw-XBE audit (default: 4)'
    )
    args = parser.parse_args()

    # Change to project root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, '../..'))
    os.chdir(project_root)

    serve_dir = os.path.abspath(args.directory)

    if not os.path.exists(serve_dir):
        logging.error('Directory does not exist: %s', serve_dir)
        sys.exit(1)

    class Handler(SSEHandler):
        def __init__(self, *a, **kw):
            super().__init__(*a, directory=serve_dir, **kw)

    server = ThreadedHTTPServer((args.host, args.port), Handler)
    logging.info('Serving %s', serve_dir)
    logging.info('  Dashboard:  http://localhost:%d', args.port)
    logging.info('  SSE events: http://localhost:%d/events', args.port)
    logging.info('  Press Ctrl+C to stop')
    logging.info('  Dashboard auto-refreshes every 5 min')

    regen_thread = threading.Thread(
        target=_background_regen,
        args=(serve_dir, project_root, 300),
        daemon=True,
    )
    regen_thread.start()

    if args.raw_audit_interval > 0:
        logging.info('  Raw-XBE audit refreshes every %d min', args.raw_audit_interval // 60)
        threading.Thread(
            target=_background_raw_audit,
            args=(project_root, args.raw_audit_interval, args.raw_audit_workers),
            daemon=True,
        ).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info('Shutting down...')
        server.shutdown()


if __name__ == '__main__':
    main()
