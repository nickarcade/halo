"""Atomic publication helpers for dashboard artifacts read by live clients."""

import json
import os
import tempfile


def write_text_atomic(path: str, content: str) -> None:
    """Replace *path* only after its complete replacement is durable on disk."""
    directory = os.path.dirname(os.path.abspath(path))
    os.makedirs(directory, exist_ok=True)
    prefix = '.' + os.path.basename(path) + '.'
    fd, temp_path = tempfile.mkstemp(prefix=prefix, suffix='.tmp', dir=directory)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as output:
            output.write(content)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temp_path, path)
    except Exception:
        try:
            os.unlink(temp_path)
        except FileNotFoundError:
            pass
        raise


def write_json_atomic(path: str, document: object, indent=None) -> None:
    """Serialize and atomically publish a JSON document."""
    write_text_atomic(path, json.dumps(document, indent=indent))
