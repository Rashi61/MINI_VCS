"""Small security helpers: login rate limiting and HTTP security headers."""
import threading
import time
from collections import defaultdict, deque


class RateLimiter:
    """In-memory sliding-window limiter.

    Suitable for a single server process (the supplied deployment runs one
    worker). For several workers, replace it with a shared store such as Redis.
    """

    def __init__(self, max_attempts: int, window_seconds: int):
        self.max_attempts = max_attempts
        self.window_seconds = window_seconds
        self._attempts = defaultdict(deque)
        self._lock = threading.Lock()

    def _prune(self, bucket, now):
        while bucket and now - bucket[0] > self.window_seconds:
            bucket.popleft()

    def is_blocked(self, key: str) -> bool:
        now = time.monotonic()

        with self._lock:
            bucket = self._attempts[key]
            self._prune(bucket, now)
            return len(bucket) >= self.max_attempts

    def register_failure(self, key: str) -> None:
        now = time.monotonic()

        with self._lock:
            bucket = self._attempts[key]
            self._prune(bucket, now)
            bucket.append(now)

    def reset(self, key: str) -> None:
        with self._lock:
            self._attempts.pop(key, None)

    def retry_after(self, key: str) -> int:
        now = time.monotonic()

        with self._lock:
            bucket = self._attempts.get(key)

            if not bucket:
                return 0

            return max(
                1,
                int(self.window_seconds - (now - bucket[0])),
            )


SECURITY_HEADERS = {
    "X-Content-Type-Options": "nosniff",
    "X-Frame-Options": "DENY",
    "Referrer-Policy": "no-referrer",
    "Permissions-Policy": "geolocation=(), microphone=(), camera=()",
    "Content-Security-Policy": (
        "default-src 'self'; "
        "script-src 'self'; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "connect-src 'self'; "
        "frame-ancestors 'none'; "
        "base-uri 'self'; "
        "form-action 'self'"
    ),
}


def apply_security_headers(response, secure_transport: bool):
    for name, value in SECURITY_HEADERS.items():
        response.headers.setdefault(name, value)

    if secure_transport:
        response.headers.setdefault(
            "Strict-Transport-Security",
            "max-age=31536000; includeSubDomains",
        )

    return response
