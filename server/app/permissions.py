from server.app.database import get_connection
ROLE_PERMISSIONS = {
    "owner": [
        "view", "clone", "pull", "push",
        "create_branch", "delete_branch",
        "manage_members", "change_settings",
        "delete_repository"
    ],

    "maintainer": [
        "view", "clone", "pull", "push",
        "create_branch", "delete_branch",
        "manage_members", "change_settings"
    ],

    "contributor": [
        "view", "clone", "pull", "push",
        "create_branch"
    ],

    "viewer": [
        "view", "clone", "pull"
    ]
}


def has_permission(role, action):
    if role not in ROLE_PERMISSIONS:
        return False

    return action in ROLE_PERMISSIONS[role]
def get_repository_role(user_id, repository_id):
    conn = get_connection()

    try:
        repo = conn.execute(
            "SELECT owner_id FROM repositories WHERE id = ?",
            (repository_id,)
        ).fetchone()

        if repo is None:
            return None

        if repo["owner_id"] == user_id:
            return "owner"

        member = conn.execute(
            """SELECT role FROM repository_members
            WHERE repository_id = ? AND user_id = ?""",
            (repository_id, user_id)
        ).fetchone()

        if member is None:
            return None

        return member["role"]

    finally:
        conn.close()


def check_repository_permission(user_id, repository_id, action):
    role = get_repository_role(user_id, repository_id)

    if role is None:
        return False

    return has_permission(role, action)