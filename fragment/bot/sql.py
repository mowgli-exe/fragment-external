import discord
from discord.ext import commands
from discord.ui import View, Button, Modal, TextInput
import aiohttp
import sqlite3
import json
import csv
import asyncio
import os
import shutil
import subprocess
from datetime import datetime, timezone
import re
from pathlib import Path

intents = discord.Intents.default()
intents.message_content = True
intents.members = True
bot = commands.Bot(command_prefix="$", intents=intents)

TARGET_ASSET_ID = 86383953065992
TARGET_ROLE_ID = 1449851448044294155
DB_FILE = "data.db"
ACCENT = discord.Color(0x76A0FF)
MOD_LOG_CHANNEL_ID = 1452775842605305929
MOD_LOG_WEBHOOK = "https://discord.com/api/webhooks/1453862312732524635/QD8-DyMhOhADWK0bum_cwQ-3Aj3trK5ENoKb5PCDA_wLMZywtNu5n-rmYQbwO4rOL2xE"
LICENSE_CSV = Path("license_keys.csv")
TRIAL_LICENSE_CSV = Path("trial_keys.csv")
TRIAL_INVITE_REQUIREMENT = 3
TICKET_CATEGORY_ID = 1453876512456577025
TICKET_STAFF_ROLE_ID = 1449851285762478222
TICKET_COUNTER_FILE = Path("ticket_counters.json")
RESELLER_VALIDATE_URL = os.getenv("RESELLER_VALIDATE_URL", "https://fragment.byethost10.com/reseller_validate.php")
RESELLER_API_KEY = os.getenv("RESELLER_API_KEY", "mR5kQ2")
LICENSE_KEY_PATTERN = re.compile(r"^[A-Za-z0-9-]{6,64}$")
RESELLER_REGISTRY_FILE = Path(__file__).resolve().parent / "resellers.json"
invite_cache: dict[int, dict[str, int]] = {}


def accent_embed(
    title: str,
    description: str,
    color: discord.Color | None = None,
    url: str | None = None,
) -> discord.Embed:
    return discord.Embed(title=title, description=description, color=color or ACCENT, url=url)


def init_db():
    with sqlite3.connect(DB_FILE) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS purchases (
                discord_id TEXT PRIMARY KEY,
                roblox_username TEXT NOT NULL,
                roblox_user_id INTEGER UNIQUE NOT NULL,
                license_key TEXT NOT NULL,
                purchase_time TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS tickets (
                discord_id TEXT NOT NULL,
                channel_id INTEGER NOT NULL,
                payment_method TEXT NOT NULL,
                status TEXT NOT NULL,
                created_at TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS trial_claims (
                discord_id TEXT PRIMARY KEY,
                license_key TEXT NOT NULL,
                claimed_at TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS manual_purchases (
                discord_id TEXT NOT NULL,
                license_key TEXT PRIMARY KEY,
                payment_method TEXT NOT NULL,
                duration TEXT NOT NULL,
                purchase_time TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS invite_credits (
                invited_id TEXT PRIMARY KEY,
                inviter_id TEXT NOT NULL,
                guild_id INTEGER NOT NULL,
                credited_at TEXT NOT NULL
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS reseller_claims (
                discord_id TEXT PRIMARY KEY,
                license_key TEXT UNIQUE NOT NULL,
                key_type TEXT NOT NULL,
                reseller_name TEXT NOT NULL DEFAULT 'Unknown',
                claimed_at TEXT NOT NULL
            )
            """
        )

        columns = {row[1] for row in conn.execute("PRAGMA table_info(reseller_claims)").fetchall()}
        if "reseller_name" not in columns:
            conn.execute("ALTER TABLE reseller_claims ADD COLUMN reseller_name TEXT NOT NULL DEFAULT 'Unknown'")

        conn.commit()


def fetch_one(query, params=()):
    with sqlite3.connect(DB_FILE) as conn:
        return conn.execute(query, params).fetchone()


def execute(query, params=()):
    with sqlite3.connect(DB_FILE) as conn:
        conn.execute(query, params)
        conn.commit()


def fetch_purchase_by_discord(discord_id):
    return fetch_one("SELECT * FROM purchases WHERE discord_id = ?", (str(discord_id),))


def fetch_purchase_by_roblox_id(roblox_user_id):
    return fetch_one("SELECT * FROM purchases WHERE roblox_user_id = ?", (roblox_user_id,))


def fetch_open_ticket(discord_id):
    return fetch_one("SELECT channel_id, payment_method FROM tickets WHERE discord_id = ? AND status = 'open'", (str(discord_id),))


def fetch_ticket_by_channel(channel_id):
    return fetch_one("SELECT discord_id, payment_method FROM tickets WHERE channel_id = ? AND status = 'open'", (channel_id,))


def insert_ticket(discord_id, channel_id, payment_method):
    execute(
        "INSERT INTO tickets (discord_id, channel_id, payment_method, status, created_at) VALUES (?, ?, ?, 'open', ?)",
        (str(discord_id), channel_id, payment_method, datetime.now(timezone.utc).isoformat()),
    )


def close_ticket_channel(channel_id):
    execute("UPDATE tickets SET status = 'closed' WHERE channel_id = ?", (channel_id,))


def load_ticket_counters():
    if TICKET_COUNTER_FILE.exists():
        try:
            return json.loads(TICKET_COUNTER_FILE.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return {"paypal": 0, "crypto": 0}
    return {"paypal": 0, "crypto": 0}


def save_ticket_counters(counters: dict):
    TICKET_COUNTER_FILE.write_text(json.dumps(counters), encoding="utf-8")


def insert_purchase(discord_id, roblox_username, roblox_user_id, license_key, timestamp):
    execute(
        "INSERT INTO purchases (discord_id, roblox_username, roblox_user_id, license_key, purchase_time) VALUES (?, ?, ?, ?, ?)",
        (str(discord_id), roblox_username, roblox_user_id, license_key, timestamp),
    )


async def send_mod_log(embed: discord.Embed):
    channel = bot.get_channel(MOD_LOG_CHANNEL_ID)
    if channel:
        try:
            await channel.send(embed=embed)
            return
        except discord.DiscordException:
            pass
    if not MOD_LOG_WEBHOOK:
        return
    payload = {"embeds": [embed.to_dict()]}
    async with aiohttp.ClientSession() as session:
        await session.post(MOD_LOG_WEBHOOK, json=payload)


async def get_user_id(username: str) -> int | None:
    url = "https://users.roblox.com/v1/usernames/users"
    async with aiohttp.ClientSession() as session:
        async with session.post(url, json={"usernames": [username], "excludeBannedUsers": False}) as resp:
            data = await resp.json()
            users = data.get("data", [])
            return users[0]["id"] if users else None


async def user_owns_asset(user_id: int, asset_id: int) -> bool:
    url = f"https://inventory.roblox.com/v1/users/{user_id}/items/Asset/{asset_id}"
    async with aiohttp.ClientSession() as session:
        async with session.get(url) as resp:
            data = await resp.json()
            return bool(data.get("data"))


async def reserve_license_key() -> str | None:
    if LICENSE_CSV.exists():
        rows = []
        with LICENSE_CSV.open(newline="", encoding="utf-8") as f:
            rows = list(csv.reader(f))
        for idx, row in enumerate(rows):
            if not row:
                continue
            key = row[0].strip()
            if key:
                remaining = [r for i, r in enumerate(rows) if i != idx and any(cell.strip() for cell in r)]
                with LICENSE_CSV.open("w", newline="", encoding="utf-8") as f:
                    writer = csv.writer(f)
                    writer.writerows(remaining)
                return key
    return None


async def reserve_trial_key() -> str | None:
    if TRIAL_LICENSE_CSV.exists():
        rows = []
        with TRIAL_LICENSE_CSV.open(newline="", encoding="utf-8") as f:
            rows = list(csv.reader(f))
        for idx, row in enumerate(rows):
            if not row:
                continue
            key = row[0].strip()
            if key:
                remaining = [r for i, r in enumerate(rows) if i != idx and any(cell.strip() for cell in r)]
                with TRIAL_LICENSE_CSV.open("w", newline="", encoding="utf-8") as f:
                    writer = csv.writer(f)
                    writer.writerows(remaining)
                return key
    return None


def normalize_license_key(raw_key: str) -> str:
    return re.sub(r"\s+", "", raw_key.strip())


def reseller_key_token(raw_key: str) -> str:
    return normalize_license_key(raw_key).casefold()


def load_reseller_key_map() -> tuple[dict[str, str], str | None]:
    if not RESELLER_REGISTRY_FILE.exists():
        return {}, f"Reseller registry not found: `{RESELLER_REGISTRY_FILE.name}`"

    try:
        payload = json.loads(RESELLER_REGISTRY_FILE.read_text(encoding="utf-8"))
    except OSError:
        return {}, "Could not read reseller registry file."
    except json.JSONDecodeError:
        return {}, "Reseller registry JSON is invalid."

    key_map: dict[str, str] = {}

    def register_keys(name: str, keys: list):
        clean_name = str(name).strip()
        if not clean_name:
            return
        for key in keys:
            if not isinstance(key, str):
                continue
            normalized = normalize_license_key(key)
            if not normalized:
                continue
            key_map[reseller_key_token(normalized)] = clean_name

    if isinstance(payload, dict) and isinstance(payload.get("resellers"), list):
        for entry in payload["resellers"]:
            if not isinstance(entry, dict):
                continue
            if entry.get("active", True) is False:
                continue
            keys = entry.get("keys", [])
            if isinstance(keys, list):
                register_keys(str(entry.get("name", "")), keys)
    elif isinstance(payload, dict):
        for reseller_name, value in payload.items():
            if reseller_name == "resellers":
                continue
            if isinstance(value, list):
                register_keys(str(reseller_name), value)
            elif isinstance(value, dict) and isinstance(value.get("keys"), list):
                if value.get("active", True) is False:
                    continue
                register_keys(str(reseller_name), value["keys"])
    else:
        return {}, "Reseller registry root must be a JSON object."

    return key_map, None


def get_reseller_for_key(license_key: str) -> tuple[str | None, str | None]:
    key_map, load_error = load_reseller_key_map()
    if load_error:
        return None, load_error

    reseller_name = key_map.get(reseller_key_token(license_key))
    if not reseller_name:
        return None, "Key is not assigned to a registered reseller."

    return reseller_name, None


def _gf_mul(a: int, b: int) -> int:
    result = 0
    while b:
        if b & 1:
            result ^= a
        a = ((a << 1) ^ 0x11B) & 0xFF if a & 0x80 else (a << 1) & 0xFF
        b >>= 1
    return result


def _gf_pow(a: int, n: int) -> int:
    result = 1
    base = a
    exp = n
    while exp:
        if exp & 1:
            result = _gf_mul(result, base)
        base = _gf_mul(base, base)
        exp >>= 1
    return result


def _gf_inv(a: int) -> int:
    return 0 if a == 0 else _gf_pow(a, 254)


def _rotl8(value: int, shift: int) -> int:
    return ((value << shift) | (value >> (8 - shift))) & 0xFF


def _build_aes_sbox() -> list[int]:
    sbox: list[int] = []
    for x in range(256):
        inv = _gf_inv(x)
        sbox_val = inv ^ _rotl8(inv, 1) ^ _rotl8(inv, 2) ^ _rotl8(inv, 3) ^ _rotl8(inv, 4) ^ 0x63
        sbox.append(sbox_val & 0xFF)
    return sbox


_AES_SBOX = _build_aes_sbox()
_AES_INV_SBOX = [0] * 256
for _idx, _val in enumerate(_AES_SBOX):
    _AES_INV_SBOX[_val] = _idx
_AES_RCON = [0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36, 0x6C, 0xD8, 0xAB, 0x4D, 0x9A]


def _bytes2matrix(data: bytes) -> list[list[int]]:
    return [list(data[i : i + 4]) for i in range(0, len(data), 4)]


def _matrix2bytes(matrix: list[list[int]]) -> bytes:
    return bytes(value for row in matrix for value in row)


def _add_round_key(state: list[list[int]], round_key: list[list[int]]):
    for i in range(4):
        for j in range(4):
            state[i][j] ^= round_key[i][j]


def _inv_sub_bytes(state: list[list[int]]):
    for i in range(4):
        for j in range(4):
            state[i][j] = _AES_INV_SBOX[state[i][j]]


def _inv_shift_rows(state: list[list[int]]):
    state[0][1], state[1][1], state[2][1], state[3][1] = state[3][1], state[0][1], state[1][1], state[2][1]
    state[0][2], state[1][2], state[2][2], state[3][2] = state[2][2], state[3][2], state[0][2], state[1][2]
    state[0][3], state[1][3], state[2][3], state[3][3] = state[1][3], state[2][3], state[3][3], state[0][3]


def _xtime(value: int) -> int:
    return ((value << 1) ^ 0x1B) & 0xFF if value & 0x80 else (value << 1) & 0xFF


def _mix_single_column(column: list[int]):
    t = column[0] ^ column[1] ^ column[2] ^ column[3]
    u = column[0]
    column[0] ^= t ^ _xtime(column[0] ^ column[1])
    column[1] ^= t ^ _xtime(column[1] ^ column[2])
    column[2] ^= t ^ _xtime(column[2] ^ column[3])
    column[3] ^= t ^ _xtime(column[3] ^ u)


def _mix_columns(state: list[list[int]]):
    for i in range(4):
        _mix_single_column(state[i])


def _inv_mix_columns(state: list[list[int]]):
    for i in range(4):
        u = _xtime(_xtime(state[i][0] ^ state[i][2]))
        v = _xtime(_xtime(state[i][1] ^ state[i][3]))
        state[i][0] ^= u
        state[i][1] ^= v
        state[i][2] ^= u
        state[i][3] ^= v
    _mix_columns(state)


def _expand_round_keys(master_key: bytes) -> tuple[list[list[list[int]]], int]:
    rounds_by_size = {16: 10, 24: 12, 32: 14}
    if len(master_key) not in rounds_by_size:
        raise ValueError("Unsupported AES key size")

    n_rounds = rounds_by_size[len(master_key)]
    iteration_size = len(master_key) // 4
    key_columns = _bytes2matrix(master_key)
    columns = key_columns[:]
    rcon_idx = 1

    while len(columns) < (n_rounds + 1) * 4:
        word = list(columns[-1])

        if len(columns) % iteration_size == 0:
            word.append(word.pop(0))
            word = [_AES_SBOX[b] for b in word]
            word[0] ^= _AES_RCON[rcon_idx]
            rcon_idx += 1
        elif len(master_key) == 32 and len(columns) % iteration_size == 4:
            word = [_AES_SBOX[b] for b in word]

        word = [x ^ y for x, y in zip(word, columns[-iteration_size])]
        columns.append(word)

    round_keys = [columns[4 * i : 4 * (i + 1)] for i in range(len(columns) // 4)]
    return round_keys, n_rounds


def _aes_cbc_decrypt_single_block(cipher_block: bytes, key: bytes, iv: bytes) -> bytes:
    round_keys, n_rounds = _expand_round_keys(key)
    state = _bytes2matrix(cipher_block)

    _add_round_key(state, round_keys[-1])
    _inv_shift_rows(state)
    _inv_sub_bytes(state)

    for round_idx in range(n_rounds - 1, 0, -1):
        _add_round_key(state, round_keys[round_idx])
        _inv_mix_columns(state)
        _inv_shift_rows(state)
        _inv_sub_bytes(state)

    _add_round_key(state, round_keys[0])
    plain_block = _matrix2bytes(state)
    return bytes(a ^ b for a, b in zip(plain_block, iv))


def is_js_cookie_challenge(body: str) -> bool:
    lowered = body.lower()
    return "__test=" in lowered and ("slowaes.decrypt" in lowered or "/aes.js" in lowered)


def parse_js_cookie_triplet(body: str) -> tuple[str, str, str] | None:
    specific = re.search(
        r'var\s+a\s*=\s*toNumbers\(\s*[\'"]([0-9a-fA-F]+)[\'"]\s*\)\s*,\s*'
        r'b\s*=\s*toNumbers\(\s*[\'"]([0-9a-fA-F]+)[\'"]\s*\)\s*,\s*'
        r'c\s*=\s*toNumbers\(\s*[\'"]([0-9a-fA-F]+)[\'"]\s*\)',
        body,
        re.IGNORECASE,
    )
    if specific:
        return specific.group(1), specific.group(2), specific.group(3)

    matches = re.findall(r'toNumbers\(\s*[\'"]([0-9a-fA-F]+)[\'"]\s*\)', body)
    if len(matches) < 3:
        return None
    return matches[0], matches[1], matches[2]


def remove_pkcs7_padding(raw: bytes) -> bytes:
    if not raw:
        return raw
    pad = raw[-1]
    if 1 <= pad <= 16 and raw.endswith(bytes([pad]) * pad):
        return raw[:-pad]
    return raw


def solve_js_cookie(body: str) -> str | None:
    triplet = parse_js_cookie_triplet(body)
    if triplet is None:
        return None

    key_hex, iv_hex, cipher_hex = triplet

    try:
        key = bytes.fromhex(key_hex)
        iv = bytes.fromhex(iv_hex)
        cipher_text = bytes.fromhex(cipher_hex)
    except ValueError:
        return None

    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    except Exception:
        pass
    else:
        try:
            decryptor = Cipher(algorithms.AES(key), modes.CBC(iv)).decryptor()
            plain = decryptor.update(cipher_text) + decryptor.finalize()
            return plain.hex()
        except Exception:
            pass

    try:
        node_path = shutil.which("node")
        if not node_path:
            raise RuntimeError("node-not-found")

        node_script = (
            "const crypto=require('node:crypto');"
            "const [k,i,c]=process.argv.slice(1);"
            "try{"
            "const key=Buffer.from(k,'hex');"
            "const iv=Buffer.from(i,'hex');"
            "const ct=Buffer.from(c,'hex');"
            "const d=crypto.createDecipheriv(`aes-${key.length*8}-cbc`,key,iv);"
            "d.setAutoPadding(false);"
            "const out=Buffer.concat([d.update(ct),d.final()]);"
            "process.stdout.write(out.toString('hex'));"
            "}catch(e){process.exit(1);}"
        )
        proc = subprocess.run(
            [node_path, "-e", node_script, key_hex, iv_hex, cipher_hex],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
        if proc.returncode != 0:
            return None
        out = proc.stdout.strip().lower()
        if not re.fullmatch(r"[0-9a-f]+", out):
            raise RuntimeError("node-invalid-output")
        return out
    except Exception:
        pass

    try:
        plain = _aes_cbc_decrypt_single_block(cipher_text, key, iv)
        return plain.hex()
    except Exception:
        return None


async def fetch_validation_response(
    session: aiohttp.ClientSession,
    params: dict[str, str],
    headers: dict[str, str] | None = None,
) -> tuple[int, str]:
    async with session.get(RESELLER_VALIDATE_URL, params=params, headers=headers, allow_redirects=True) as resp:
        return resp.status, await resp.text()


async def validate_reseller_key(license_key: str) -> tuple[bool, str, dict | None]:
    params = {"api_key": RESELLER_API_KEY, "key": license_key}

    try:
        timeout = aiohttp.ClientTimeout(total=10)
        async with aiohttp.ClientSession(timeout=timeout) as session:
            status_code, body = await fetch_validation_response(session, params)

            if is_js_cookie_challenge(body):
                cookie_value = solve_js_cookie(body)
                if not cookie_value:
                    return (
                        False,
                        "Validation endpoint returned a JavaScript cookie challenge that the bot could not solve. (api_key/key params are already being sent by the bot)",
                        None,
                    )

                challenge_params = dict(params)
                challenge_params["i"] = "1"
                challenge_headers = {"Cookie": f"__test={cookie_value}"}
                status_code, body = await fetch_validation_response(session, challenge_params, challenge_headers)
    except asyncio.TimeoutError:
        return False, f"Validation service timed out ({RESELLER_VALIDATE_URL}).", None
    except aiohttp.ClientError:
        return False, f"Could not reach validation service ({RESELLER_VALIDATE_URL}).", None

    try:
        data = json.loads(body)
    except json.JSONDecodeError:
        if is_js_cookie_challenge(body):
            return (
                False,
                "Validation endpoint is behind a JavaScript anti-bot page. Host reseller_validate.php on a direct API-safe endpoint.",
                None,
            )
        return False, "Validation service returned invalid JSON.", None

    if not isinstance(data, dict):
        return False, "Validation service returned an invalid response.", None

    if status_code != 200:
        message = str(data.get("message") or f"Validation failed (HTTP {status_code}).")
        return False, message, None

    if data.get("status") != "success" or not data.get("valid", False):
        return False, str(data.get("message") or "Key is not valid."), None

    return True, "", data


async def cache_guild_invites(guild: discord.Guild):
    try:
        invites = await guild.invites()
    except discord.Forbidden:
        return
    except discord.HTTPException:
        return

    invite_cache[guild.id] = {invite.code: (invite.uses or 0) for invite in invites}


async def detect_used_invite(guild: discord.Guild):
    """Return (invite, invites_after) where invite is the one whose uses increased."""
    try:
        invites_after = await guild.invites()
    except (discord.Forbidden, discord.HTTPException):
        return None, None

    before = invite_cache.get(guild.id, {})
    used = None
    for inv in invites_after:
        prev_uses = before.get(inv.code, 0)
        curr_uses = inv.uses or 0
        if curr_uses > prev_uses:
            used = inv
            break
    invite_cache[guild.id] = {inv.code: (inv.uses or 0) for inv in invites_after}
    return used, invites_after


def insert_manual_purchase(discord_id, license_key, payment_method, duration):
    execute(
        "INSERT OR REPLACE INTO manual_purchases (discord_id, license_key, payment_method, duration, purchase_time) VALUES (?, ?, ?, ?, ?)",
        (str(discord_id), license_key, payment_method, duration, datetime.now(timezone.utc).isoformat()),
    )


def fetch_manual_purchase_by_discord(discord_id):
    return fetch_one(
        "SELECT license_key, payment_method, duration, purchase_time FROM manual_purchases WHERE discord_id = ? ORDER BY purchase_time DESC LIMIT 1",
        (str(discord_id),),
    )


def fetch_manual_purchase_by_key(license_key):
    return fetch_one(
        "SELECT discord_id, payment_method, duration, purchase_time FROM manual_purchases WHERE license_key = ?",
        (license_key,),
    )


def fetch_trial_claim(discord_id):
    return fetch_one("SELECT license_key, claimed_at FROM trial_claims WHERE discord_id = ?", (str(discord_id),))


def insert_trial_claim(discord_id, license_key):
    execute(
        "INSERT OR REPLACE INTO trial_claims (discord_id, license_key, claimed_at) VALUES (?, ?, ?)",
        (str(discord_id), license_key, datetime.now(timezone.utc).isoformat()),
    )


def fetch_reseller_claim_by_discord(discord_id):
    return fetch_one(
        "SELECT license_key, key_type, claimed_at, reseller_name FROM reseller_claims WHERE discord_id = ?",
        (str(discord_id),),
    )


def fetch_reseller_claim_by_key(license_key):
    return fetch_one(
        "SELECT discord_id, key_type, claimed_at, reseller_name FROM reseller_claims WHERE license_key = ?",
        (license_key,),
    )


def insert_reseller_claim(discord_id, license_key, key_type, reseller_name):
    execute(
        "INSERT INTO reseller_claims (discord_id, license_key, key_type, reseller_name, claimed_at) VALUES (?, ?, ?, ?, ?)",
        (str(discord_id), license_key, key_type, reseller_name, datetime.now(timezone.utc).isoformat()),
    )


def insert_invite_credit(invited_id: int, inviter_id: int, guild_id: int):
    execute(
        "INSERT OR IGNORE INTO invite_credits (invited_id, inviter_id, guild_id, credited_at) VALUES (?, ?, ?, ?)",
        (str(invited_id), str(inviter_id), guild_id, datetime.now(timezone.utc).isoformat()),
    )


def invite_credit_exists(invited_id: int) -> bool:
    row = fetch_one("SELECT 1 FROM invite_credits WHERE invited_id = ?", (str(invited_id),))
    return row is not None


def count_invite_credits_for_inviter(inviter_id: int, guild_id: int) -> int:
    row = fetch_one(
        "SELECT COUNT(*) FROM invite_credits WHERE inviter_id = ? AND guild_id = ?",
        (str(inviter_id), guild_id),
    )
    return int(row[0]) if row and row[0] is not None else 0


def reset_invites_and_trial(inviter_id: int, guild_id: int) -> tuple[int, int]:
    """Delete invite credits and trial claim for the inviter; returns (invite_rows, trial_rows)."""
    with sqlite3.connect(DB_FILE) as conn:
        cur_invites = conn.execute(
            "DELETE FROM invite_credits WHERE inviter_id = ? AND guild_id = ?",
            (str(inviter_id), guild_id),
        )
        cur_trial = conn.execute(
            "DELETE FROM trial_claims WHERE discord_id = ?",
            (str(inviter_id),),
        )
        conn.commit()
        return cur_invites.rowcount, cur_trial.rowcount


async def grant_customer_role(user_id: int):
    for guild in bot.guilds:
        role = guild.get_role(TARGET_ROLE_ID)
        member = guild.get_member(user_id)
        if not member:
            try:
                member = await guild.fetch_member(user_id)
            except discord.HTTPException:
                member = None
        if member and role:
            try:
                await member.add_roles(role, reason="Verified Roblox shirt purchase")
                return True
            except discord.Forbidden:
                continue
    return False


class LicenseKeyView(View):
    def __init__(self, user_id, roblox_user_id, roblox_username):
        super().__init__(timeout=None)
        self.user_id = str(user_id)
        self.roblox_user_id = roblox_user_id
        self.roblox_username = roblox_username

        button = Button(label="Verify My Purchase", style=discord.ButtonStyle.success, custom_id="verify_or_get_key")
        button.callback = self.verify_or_get
        self.add_item(button)

    async def verify_or_get(self, interaction: discord.Interaction):
        await interaction.response.defer()
        can_ephemeral = interaction.guild_id is not None
        purchase = fetch_purchase_by_discord(self.user_id)

        if purchase:
            license_key = purchase[3]
            await interaction.followup.send(
                embed=accent_embed("Your License Key", f"You've already verified your purchase.\nHere is your license key:\n```{license_key}```"),
                ephemeral=can_ephemeral,
            )
            return

        if fetch_purchase_by_roblox_id(self.roblox_user_id):
            await interaction.followup.send(
                embed=accent_embed(
                    "Roblox Account Already Claimed",
                    "This Roblox account has already been used to claim a license key with another Discord account.",
                    discord.Color.red(),
                ),
                ephemeral=can_ephemeral,
            )
            return

        if not await user_owns_asset(self.roblox_user_id, TARGET_ASSET_ID):
            await interaction.followup.send(
                embed=accent_embed(
                    "Purchase Not Found",
                    "We couldn't verify your purchase. Make sure you've bought the shirt and try again.",
                    discord.Color.red(),
                ),
                ephemeral=can_ephemeral,
            )
            return

        license_key = await reserve_license_key()
        if not license_key:
            await interaction.followup.send(
                embed=accent_embed(
                    "License Unavailable",
                    "No license keys are available right now. Please contact support.",
                    discord.Color.red(),
                ),
                ephemeral=can_ephemeral,
            )
            return

        timestamp = datetime.utcnow().isoformat()
        insert_purchase(self.user_id, self.roblox_username, self.roblox_user_id, license_key, timestamp)

        await grant_customer_role(int(self.user_id))

        await send_mod_log(
            accent_embed(
                "Purchase Verified",
                (
                    f"{interaction.user.mention} bought with Robux.\n"
                    f"Roblox: `{self.roblox_username}` (ID {self.roblox_user_id})\n"
                    f"License: `{license_key}`"
                ),
                discord.Color.green(),
            )
        )

        await interaction.followup.send(
            embed=accent_embed("Purchase Verified", "You've successfully purchased the product.", discord.Color.green()),
        )
        await interaction.followup.send(
            embed=accent_embed(
                "License Key",
                (
                    f"Here is your license key:\n```{license_key}```\n"
                    "Please store it safely.\n\n"
                    "Next steps:\n"
                    "• Open the FAQ channel: https://discord.com/channels/1449851219223908374/1452775210896851056\n"
                    "• Download the build: https://discord.com/channels/1449851219223908374/1453873719339520095"
                ),
            ),
        )


class RobloxUsernameModal(Modal, title="Enter your Roblox username"):
    username = TextInput(label="What's your Roblox Username?", placeholder="e.g., Builderman", required=True)

    async def on_submit(self, interaction: discord.Interaction):
        uname = self.username.value.strip()
        user_id = await get_user_id(uname)

        if not user_id:
            await interaction.response.send_message(
                embed=accent_embed("Invalid Username", f"The username `{uname}` is not valid on Roblox."),
                ephemeral=True,
            )
            return

        if fetch_purchase_by_roblox_id(user_id):
            await interaction.response.send_message(
                embed=accent_embed(
                    "Roblox Account Already Claimed",
                    "This Roblox account has already been used to claim a license key with another Discord account.",
                    discord.Color.red(),
                ),
                ephemeral=interaction.guild_id is not None,
            )
            return

        await interaction.response.send_message(
            embed=accent_embed(
                "Username Accepted",
                f"Thanks! Username `{uname}` received.\nI've sent you a message - please check your DMs to continue.",
            ),
            ephemeral=True,
        )

        try:
            await interaction.user.send(
                embed=accent_embed(
                    "Your Key is Almost Ready!",
                    (
                        "To unlock access, follow these steps:\n"
                        "1) Open the product page: https://www.roblox.com/catalog/86383953065992/fragment-license\n"
                        "2) Purchase the shirt with Robux.\n"
                        "3) Use the button in the next message to finish verification."
                    ),
                )
            )

            await interaction.user.send(
                embed=accent_embed(
                    "Final Step: Claim Access",
                    "Click the button below to verify your purchase or view your key.",
                ),
                view=LicenseKeyView(interaction.user.id, user_id, uname),
            )

        except discord.Forbidden:
            await interaction.followup.send("I couldn't DM you. Please enable DMs and try again.", ephemeral=True)


class PurchaseView(View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Purchase with Robux", style=discord.ButtonStyle.secondary, custom_id="purchase_robux")
    async def purchase_button(self, interaction: discord.Interaction, button: Button):
        await interaction.response.send_modal(RobloxUsernameModal())


class PaymentSelect(discord.ui.Select):
    def __init__(self):
        options = [
            discord.SelectOption(label="PayPal", value="paypal", description="Open a PayPal ticket"),
            discord.SelectOption(label="Crypto", value="crypto", description="Open a crypto ticket"),
        ]
        super().__init__(placeholder="Choose payment method", min_values=1, max_values=1, options=options, custom_id="ticket_payment_select")

    async def callback(self, interaction: discord.Interaction):
        existing = fetch_open_ticket(interaction.user.id)
        if existing:
            channel = interaction.client.get_channel(existing[0])
            msg = f"You already have an open ticket: {channel.mention}" if channel else "You already have an open ticket."
            await interaction.response.send_message(msg, ephemeral=True)
            return

        guild = interaction.guild
        if not guild:
            await interaction.response.send_message("Tickets must be created in the server.", ephemeral=True)
            return

        category = guild.get_channel(TICKET_CATEGORY_ID)
        if not category or not isinstance(category, discord.CategoryChannel):
            await interaction.response.send_message("Ticket category not found. Please contact staff.", ephemeral=True)
            return

        payment = self.values[0]
        prefix = "paypal" if payment == "paypal" else "crypto"
        counters = load_ticket_counters()
        next_num = counters.get(prefix, 0) + 1
        counters[prefix] = next_num
        save_ticket_counters(counters)
        channel_name = f"{prefix}-{next_num:04d}"

        overwrites = {
            guild.default_role: discord.PermissionOverwrite(view_channel=False),
            interaction.user: discord.PermissionOverwrite(view_channel=True, send_messages=True, read_message_history=True),
        }
        staff_role = guild.get_role(TICKET_STAFF_ROLE_ID)
        if staff_role:
            overwrites[staff_role] = discord.PermissionOverwrite(view_channel=True, send_messages=True, read_message_history=True)

        channel = await guild.create_text_channel(
            channel_name,
            category=category,
            overwrites=overwrites,
            reason=f"{payment} ticket for {interaction.user}",
        )

        insert_ticket(interaction.user.id, channel.id, payment)

        embed = accent_embed(
            f"{payment.capitalize()} Ticket",
            (
                f"Welcome {interaction.user.mention}!\n"
                "Please provide the required details below.\n\n"
                "**Payment Email/Address:** zhkznez@gmail.com / Ask for crypto address\n"
                "**Amount to send:** $11.5"
            ),
        )
        await channel.send(content=interaction.user.mention, embed=embed)
        await interaction.response.send_message(f"Ticket created: {channel.mention}", ephemeral=True)


class TicketView(View):
    def __init__(self):
        super().__init__(timeout=None)
        self.add_item(PaymentSelect())


class TrialKeyView(View):
    def __init__(self):
        super().__init__(timeout=None)
        button = Button(label="Get Key", style=discord.ButtonStyle.success, custom_id="trial_get_key")
        button.callback = self.handle_get_key
        self.add_item(button)

    async def handle_get_key(self, interaction: discord.Interaction):
        if not interaction.guild:
            await interaction.response.send_message("This must be used in the server.", ephemeral=True)
            return

        existing = fetch_trial_claim(interaction.user.id)
        if existing:
            license_key = existing[0]
            await interaction.response.send_message(
                embed=accent_embed("Trial Already Claimed", f"You already claimed a trial key.\nYour key: `{license_key}`"),
                ephemeral=True,
            )
            return

        invite_credits = count_invite_credits_for_inviter(interaction.user.id, interaction.guild.id)
        if invite_credits < TRIAL_INVITE_REQUIREMENT:
            await interaction.response.send_message(
                embed=accent_embed(
                    "Not Enough Invites",
                    f"You need {TRIAL_INVITE_REQUIREMENT} unique invites to claim a trial key.\nCurrent invites: `{invite_credits}`.",
                    discord.Color.red(),
                ),
                ephemeral=True,
            )
            return

        license_key = await reserve_trial_key()
        if not license_key:
            await interaction.response.send_message(
                embed=accent_embed("No Trial Keys", "No trial keys are available right now. Please contact staff.", discord.Color.red()),
                ephemeral=True,
            )
            return

        insert_trial_claim(interaction.user.id, license_key)

        dm_ok = True
        try:
            dm_embed = accent_embed(
                "Your 2-Day Trial Key",
                (
                    f"Here is your 2-day whitelist key:\n```{license_key}```\n"
                    "This key lasts 2 days from activation.\n\n"
                    "Download: https://cdn.discordapp.com/attachments/1453873719339520095/1455775899076132945/5b077c09380f4fe6.rar?ex=6955f475&is=6954a2f5&hm=2e88c8fe7507083459d48993c63b4e9ce7aa2c93dba488d3f71ba1be608ebd1b&\n"
                    "You must be on version: version-5b077c09380f4fe6. "
                    "If there are any bugs please report in chat, ping aori in chat, or directly DM aori <@1195144155790327898>."
                ),
            )
            await interaction.user.send(embed=dm_embed)
        except discord.Forbidden:
            dm_ok = False

        lines = ["You've been whitelisted!"]
        if dm_ok:
            lines.append("Check your DMs for your trial key.")
        else:
            lines.append("I couldn't DM you. Please enable DMs and contact staff if you need help.")
            lines.append(f"Your trial key: `{license_key}`")
        lines.append(
            "Download: https://cdn.discordapp.com/attachments/1453873719339520095/1455775899076132945/5b077c09380f4fe6.rar?ex=6955f475&is=6954a2f5&hm=2e88c8fe7507083459d48993c63b4e9ce7aa2c93dba488d3f71ba1be608ebd1b&"
        )
        lines.append(
            "You must be on version: version-5b077c09380f4fe6. If there are any bugs please report it in chat, ping aori in chat, or directly DM aori <@1195144155790327898>."
        )

        await interaction.response.send_message(
            embed=accent_embed("Trial Granted", "\n".join(lines), discord.Color.green()),
            ephemeral=True,
        )


class ResellerClaimModal(Modal, title="Claim Reseller Key"):
    key_input = TextInput(label="Enter your key", placeholder="XXXX-XXXX-XXXX", required=True, max_length=64)

    async def on_submit(self, interaction: discord.Interaction):
        if not interaction.guild:
            await interaction.response.send_message("This must be used in the server.", ephemeral=True)
            return

        license_key = normalize_license_key(str(self.key_input.value))
        if not LICENSE_KEY_PATTERN.fullmatch(license_key):
            await interaction.response.send_message(
                embed=accent_embed(
                    "Invalid Key Format",
                    "Please enter a valid key format (letters, numbers, and dashes only).",
                    discord.Color.red(),
                ),
                ephemeral=True,
            )
            return

        existing_claim = fetch_reseller_claim_by_discord(interaction.user.id)
        if existing_claim:
            await interaction.response.send_message(
                embed=accent_embed(
                    "Already Claimed",
                    (
                        "You already claimed a reseller key.\n"
                        f"Your key: `{existing_claim[0]}`"
                    ),
                    discord.Color.red(),
                ),
                ephemeral=True,
            )
            return

        key_claim = fetch_reseller_claim_by_key(license_key)
        if key_claim:
            if str(key_claim[0]) == str(interaction.user.id):
                await interaction.response.send_message(
                    embed=accent_embed(
                        "Already Claimed",
                        f"You already claimed this key: `{license_key}`",
                    ),
                    ephemeral=True,
                )
            else:
                await interaction.response.send_message(
                    embed=accent_embed(
                        "Key Already Used",
                        "That key has already been claimed by another user. Contact the reseller.",
                        discord.Color.red(),
                    ),
                    ephemeral=True,
                )
            return

        manual_purchase = fetch_manual_purchase_by_key(license_key)
        if manual_purchase:
            await interaction.response.send_message(
                embed=accent_embed(
                    "Key Already Used",
                    "That key is already assigned. Contact support if this is unexpected.",
                    discord.Color.red(),
                ),
                ephemeral=True,
            )
            return

        reseller_name, reseller_error = get_reseller_for_key(license_key)
        if not reseller_name:
            await interaction.response.send_message(
                embed=accent_embed("Invalid Key", str(reseller_error), discord.Color.red()),
                ephemeral=True,
            )
            return

        valid, message, key_info = await validate_reseller_key(license_key)
        if not valid:
            await interaction.response.send_message(
                embed=accent_embed("Invalid Key", message, discord.Color.red()),
                ephemeral=True,
            )
            return

        key_type = str((key_info or {}).get("type") or "lifetime")
        try:
            insert_reseller_claim(interaction.user.id, license_key, key_type, reseller_name)
        except sqlite3.IntegrityError:
            await interaction.response.send_message(
                embed=accent_embed(
                    "Key Already Used",
                    "That key was claimed a moment ago. Contact support if needed.",
                    discord.Color.red(),
                ),
                ephemeral=True,
            )
            return

        role_granted = await grant_customer_role(interaction.user.id)

        response_lines = ["You've been whitelisted through reseller."]
        if role_granted:
            response_lines.append("Your customer role has been granted.")
        else:
            response_lines.append("I couldn't grant your role automatically. Please contact staff.")

        response_lines.append(f"Reseller: `{reseller_name}`")
        response_lines.append("FAQ: https://discord.com/channels/1449851219223908374/1452775210896851056")
        response_lines.append("Download: https://discord.com/channels/1449851219223908374/1453873719339520095")

        await interaction.response.send_message(
            embed=accent_embed("Reseller Claim Complete", "\n".join(response_lines), discord.Color.green()),
            ephemeral=True,
        )

        await send_mod_log(
            accent_embed(
                "Reseller Whitelist",
                (
                    f"{interaction.user.mention} whitelisted through reseller.\n"
                    f"Reseller: `{reseller_name}`\n"
                    f"License: `{license_key}`\n"
                    f"Type: `{key_type}`"
                ),
                discord.Color.green(),
            )
        )


class ResellerClaimView(View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Claim Key", style=discord.ButtonStyle.success, custom_id="reseller_claim_key")
    async def claim_button(self, interaction: discord.Interaction, button: Button):
        await interaction.response.send_modal(ResellerClaimModal())


@bot.hybrid_command(name="buyer_info", aliases=["buyerinfo", "buyer-info", "buyer"])
@commands.has_permissions(administrator=True)
async def buyer_info(ctx, member: discord.Member):
    row = fetch_purchase_by_discord(str(member.id))
    manual_row = fetch_manual_purchase_by_discord(str(member.id))
    reseller_row = fetch_reseller_claim_by_discord(str(member.id))
    if not row and not manual_row and not reseller_row:
        if ctx.interaction and not ctx.interaction.response.is_done():
            await ctx.interaction.response.send_message(f"No purchase data found for {member.mention}.", ephemeral=True)
        else:
            await ctx.send(f"No purchase data found for {member.mention}.")
        return

    if row:
        _, roblox_username, _, license_key, purchase_time = row
        dt = datetime.fromisoformat(purchase_time)
        formatted_time = dt.strftime("%d %B %Y at %I:%M %p")
        roblox_url = f"https://www.roblox.com/users/profile?username={roblox_username}"

        embed = accent_embed(
            "Buyer Info",
            (
                f"**User:** {member.mention}\n"
                f"**Roblox Username:** [{roblox_username}]({roblox_url})\n"
                f"**License Key:** `{license_key}`\n"
                f"**Purchased At:** `{formatted_time}`"
            ),
        )
    elif manual_row:
        license_key, payment_method, duration, purchase_time = manual_row
        dt = datetime.fromisoformat(purchase_time)
        formatted_time = dt.strftime("%d %B %Y at %I:%M %p")
        embed = accent_embed(
            "Buyer Info",
            (
                f"**User:** {member.mention}\n"
                f"**License Key:** `{license_key}`\n"
                f"**Duration:** `{duration}`\n"
                f"**Payment Method:** `{payment_method.capitalize()}`\n"
                f"**Purchased At:** `{formatted_time}`"
            ),
        )
    else:
        license_key, key_type, claimed_at, reseller_name = reseller_row
        dt = datetime.fromisoformat(claimed_at)
        formatted_time = dt.strftime("%d %B %Y at %I:%M %p")
        embed = accent_embed(
            "Buyer Info",
            (
                f"**User:** {member.mention}\n"
                f"**License Key:** `{license_key}`\n"
                f"**Duration:** `{key_type}`\n"
                "**Payment Method:** `Reseller`\n"
                f"**Reseller:** `{reseller_name}`\n"
                f"**Whitelisted At:** `{formatted_time}`"
            ),
        )

    if ctx.interaction:
        if not ctx.interaction.response.is_done():
            await ctx.interaction.response.send_message(embed=embed, ephemeral=True)
        else:
            await ctx.interaction.followup.send(embed=embed, ephemeral=True)
        return

    await ctx.reply(embed=embed, mention_author=False)


@bot.command(name="send_robux")
@commands.has_any_role(1449851285762478222)
async def send_purchase(ctx):
    embed = accent_embed("Get fragment now!", "Click below to purchase automatically with Robux.")
    await ctx.send(embed=embed, view=PurchaseView())


@bot.command(name="send_ticket")
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def send_ticket(ctx):
    embed = accent_embed("Open a Ticket", "Select PayPal or Crypto to start a ticket.")
    await ctx.send(embed=embed, view=TicketView())


@bot.command(name="c")
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def close_ticket(ctx):
    channel = ctx.channel
    if not isinstance(channel, discord.TextChannel) or channel.category_id != TICKET_CATEGORY_ID:
        await ctx.send("This command can only be used inside a ticket channel.")
        return
    close_ticket_channel(channel.id)
    await ctx.send("Closing ticket...", delete_after=3)
    await channel.delete(reason=f"Ticket closed by staff {ctx.author}")


@bot.command(name="trialkey")
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def send_trial_key(ctx):
    embed = accent_embed(
        "Claim a Trial Key",
        f"Invite {TRIAL_INVITE_REQUIREMENT} people to the server to unlock a 2-day whitelist key. Click below to check and claim.",
    )
    await ctx.send(embed=embed, view=TrialKeyView())


@bot.hybrid_command(name="send_reseller_panel", aliases=["resellerpanel"])
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def send_reseller_panel(ctx):
    embed = accent_embed(
        "Claim Reseller Key",
        "Bought through a reseller? Click the button below and enter your key to get whitelisted.",
    )
    if ctx.interaction and not ctx.interaction.response.is_done():
        await ctx.interaction.response.send_message(embed=embed, view=ResellerClaimView())
    else:
        await ctx.send(embed=embed, view=ResellerClaimView())


@bot.command(name="reset_invites")
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def reset_invites(ctx, member: discord.Member):
    guild = ctx.guild
    if not guild:
        await ctx.send("This command must be used inside a server.")
        return

    invites_removed, trial_removed = reset_invites_and_trial(member.id, guild.id)
    await ctx.send(
        f"Reset invite credits for {member.mention}. "
        f"Removed {invites_removed} entr{'y' if invites_removed == 1 else 'ies'}; "
        f"trial cleared: {'yes' if trial_removed else 'no'}."
    )


@commands.hybrid_command(name="p")
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def purchased(ctx, license_key: str | None = None):
    channel = ctx.channel if isinstance(ctx.channel, discord.TextChannel) else None
    if not channel or channel.category_id != TICKET_CATEGORY_ID:
        msg = "Use this command inside a ticket channel."
        if ctx.interaction and not ctx.interaction.response.is_done():
            await ctx.interaction.response.send_message(msg, ephemeral=True)
        else:
            await ctx.reply(msg)
        return

    ticket_row = fetch_ticket_by_channel(channel.id)
    if not ticket_row:
        msg = "No open ticket record for this channel."
        if ctx.interaction and not ctx.interaction.response.is_done():
            await ctx.interaction.response.send_message(msg, ephemeral=True)
        else:
            await ctx.reply(msg)
        return

    user_id, payment_method = ticket_row
    member = channel.guild.get_member(int(user_id)) if channel.guild else None
    if not member and channel.guild:
        try:
            member = await channel.guild.fetch_member(int(user_id))
        except discord.HTTPException:
            member = None

    key = license_key or await reserve_license_key()
    if not key:
        text = "No license keys available."
        if ctx.interaction and not ctx.interaction.response.is_done():
            await ctx.interaction.response.send_message(text, ephemeral=True)
        else:
            await ctx.reply(text)
        return

    dm_ok = False
    if member:
        dm_embed = accent_embed(
            "License Key",
            (
                f"Here is your license key:\n```{key}```\n"
                "Please store it safely.\n\n"
                "Next steps:\n"
                "• Open the FAQ channel: https://discord.com/channels/1449851219223908374/1452775210896851056\n"
                "• Download the build: https://discord.com/channels/1449851219223908374/1453873719339520095"
            ),
        )
        try:
            await member.send(embed=dm_embed)
            dm_ok = True
        except discord.Forbidden:
            dm_ok = False

    role_ok = False
    if member:
        role_ok = await grant_customer_role(member.id)

    insert_manual_purchase(user_id, key, payment_method, "Lifetime")

    desc = f"{member.mention if member else user_id} paid via {payment_method.upper()}.\nLicense: `{key}`\n"
    log_embed = accent_embed("Purchase Verified", desc, discord.Color.green())
    mod_channel = ctx.guild.get_channel(MOD_LOG_CHANNEL_ID) if ctx.guild else None
    if mod_channel:
        await mod_channel.send(embed=log_embed)
    elif MOD_LOG_WEBHOOK:
        payload = {"embeds": [log_embed.to_dict()]}
        async with aiohttp.ClientSession() as session:
            await session.post(MOD_LOG_WEBHOOK, json=payload)

    status_lines = [
        f"DM sent: {'Yes' if dm_ok else 'No'}",
        f"Customer role granted: {'Yes' if role_ok else 'No'}",
    ]
    ticket_embed = accent_embed(
        "Manual Purchase Processed",
        "\n".join(status_lines),
        discord.Color.green(),
    )
    if ctx.interaction and not ctx.interaction.response.is_done():
        await ctx.interaction.response.send_message(embed=ticket_embed, ephemeral=True)
    else:
        await ctx.reply(embed=ticket_embed, mention_author=False, delete_after=10)


# prefix alias to avoid CommandNotFound when typing /purchased as a message
@bot.command(name="p")
@commands.has_any_role(TICKET_STAFF_ROLE_ID)
async def purchased_prefix(ctx, license_key: str | None = None):
    await purchased.callback(ctx, license_key)


@bot.event
async def on_ready():
    if not getattr(bot, "_persistent_views_ready", False):
        bot.add_view(ResellerClaimView())
        bot._persistent_views_ready = True

    print(f"Reseller validation URL: {RESELLER_VALIDATE_URL}")
    reseller_map, reseller_error = load_reseller_key_map()
    if reseller_error:
        print(f"Reseller registry warning: {reseller_error}")
    else:
        print(f"Reseller registry loaded: {len(reseller_map)} keys across {len(set(reseller_map.values()))} reseller(s)")

    for guild in bot.guilds:
        await cache_guild_invites(guild)
    try:
        await bot.tree.sync()
    except Exception as exc:
        print(f"Slash sync failed: {exc}")
    print(f"Logged in as {bot.user} ({bot.user.id})")


@bot.event
async def on_member_join(member: discord.Member):
    guild = member.guild
    if not guild:
        return

    if guild.id not in invite_cache:
        await cache_guild_invites(guild)

    used_invite, _ = await detect_used_invite(guild)
    if not used_invite or not used_invite.inviter:
        return

    if invite_credit_exists(member.id):
        return

    insert_invite_credit(member.id, used_invite.inviter.id, guild.id)


init_db()
bot.run("MTQ1Mzg1MzU4NzgyMTI5NzcyNA.GHk2ov._nGNXczRHiiheqhvU2Oj9Wydf106dj9RfGwYUE")
