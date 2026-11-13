import hashlib
import secrets
import hmac
import sqlite3
from datetime import datetime, timedelta, timezone

from server.app.database import get_connection


SALT_SIZE = 16
HASH_SIZE = 32
ITERATIONS = 310000
SESSION_DURATION = 24


def hash_password(password):
    salt = secrets.token_bytes(SALT_SIZE)

    password_hash = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode("utf-8"),
        salt,
        ITERATIONS,
        dklen=HASH_SIZE
    )

    return password_hash.hex(), salt.hex()


def verify_password(password, stored_hash, stored_salt):
    salt = bytes.fromhex(stored_salt)

    new_hash = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode("utf-8"),
        salt,
        ITERATIONS,
        dklen=HASH_SIZE
    )

    return hmac.compare_digest(new_hash.hex(), stored_hash)
def register_user(username, email, password):
    if not username or not email or not password:
        raise ValueError("All fields are required.")

    if len(password) < 8:
        raise ValueError("Password must contain at least 8 characters.")

    password_hash, password_salt = hash_password(password)

    conn = get_connection()

    try:
        cursor = conn.execute(
            """INSERT INTO users
            (username, email, password_hash, password_salt)
            VALUES (?, ?, ?, ?)""",
            (username, email, password_hash, password_salt)
        )

        conn.commit()
        return cursor.lastrowid

    except sqlite3.IntegrityError:
        raise ValueError("Username or email already exists.")

    finally:
        conn.close()
def login_user(username, password):
    conn = get_connection()

    try:
        user = conn.execute(
            "SELECT * FROM users WHERE username = ?",
            (username,)
        ).fetchone()

        if user is None:
            raise ValueError("Invalid username or password.")

        if not verify_password(
            password,
            user["password_hash"],
            user["password_salt"]
        ):
            raise ValueError("Invalid username or password.")

        token = secrets.token_urlsafe(32)
        token_hash = hashlib.sha256(token.encode()).hexdigest()

        expires_at = (
            datetime.now(timezone.utc) + timedelta(hours=SESSION_DURATION)
        ).isoformat()

        conn.execute(
            """INSERT INTO sessions
            (user_id, token_hash, expires_at)
            VALUES (?, ?, ?)""",
            (user["id"], token_hash, expires_at)
        )

        conn.commit()

        return {
            "user_id": user["id"],
            "username": user["username"],
            "token": token
        }

    finally:
        conn.close()
def verify_session(token):
    if not token:
        return None

    token_hash = hashlib.sha256(token.encode()).hexdigest()

    conn = get_connection()

    try:
        session = conn.execute(
            """SELECT user_id, expires_at, revoked_at
            FROM sessions
            WHERE token_hash = ?""",
            (token_hash,)
        ).fetchone()

        if session is None or session["revoked_at"] is not None:
            return None

        expiry = datetime.fromisoformat(session["expires_at"])

        if expiry <= datetime.now(timezone.utc):
            return None

        return session["user_id"]

    finally:
        conn.close()
def logout_user(token):
    if not token:
        return False

    token_hash = hashlib.sha256(token.encode()).hexdigest()

    conn = get_connection()

    try:
        cursor = conn.execute(
            """UPDATE sessions
            SET revoked_at = ?
            WHERE token_hash = ? AND revoked_at IS NULL""",
            (datetime.now(timezone.utc).isoformat(), token_hash)
        )

        conn.commit()

        return cursor.rowcount > 0

    finally:
        conn.close()