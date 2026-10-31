import discord
from discord.ext import commands
from discord.ui import View, Button, Modal, TextInput
import aiohttp
import asyncio
import json
import os
import re
from datetime import datetime
import random
import string

intents = discord.Intents.default()
intents.message_content = True
bot = commands.Bot(command_prefix="/", intents=intents)

TARGET_ASSET_ID = 78409400320183
TARGET_ROLE_ID = 1368364369221521470
DATA_FILE = "data.json"
RESELLER_CLAIMS_FILE = "reseller_claims.json"
RESELLER_VALIDATE_URL = os.getenv("RESELLER_VALIDATE_URL", "http://127.0.0.1:8000/reseller_validate.php")
RESELLER_API_KEY = os.getenv("RESELLER_API_KEY", "mR5kQ2")
RESELLER_LOG_CHANNEL_ID = int(os.getenv("RESELLER_LOG_CHANNEL_ID", "0") or "0")
LICENSE_KEY_PATTERN = re.compile(r"^[A-Za-z0-9-]{6,64}$")
ACCENT = discord.Color(0x76A0FF)


def accent_embed(
    title: str,
    description: str,
    color: discord.Color | None = None,
    url: str | None = None,
) -> discord.Embed:
    return discord.Embed(title=title, description=description, color=color or ACCENT, url=url)


def load_data():
    if not os.path.exists(DATA_FILE) or os.path.getsize(DATA_FILE) == 0:
        return {}
    try:
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    except json.JSONDecodeError:
        return {}


def save_data(data):
    with open(DATA_FILE, "w") as f:
        json.dump(data, f, indent=2)


def load_reseller_claims():
    if not os.path.exists(RESELLER_CLAIMS_FILE) or os.path.getsize(RESELLER_CLAIMS_FILE) == 0:
        return {}
    try:
        with open(RESELLER_CLAIMS_FILE, "r") as f:
            return json.load(f)
    except json.JSONDecodeError:
        return {}


def save_reseller_claims(data):
    with open(RESELLER_CLAIMS_FILE, "w") as f:
        json.dump(data, f, indent=2)


def find_reseller_claim_by_key(license_key: str):
    claims = load_reseller_claims()
    for discord_id, claim in claims.items():
        if claim.get("license_key") == license_key:
            return discord_id, claim
    return None


def normalize_license_key(raw_key: str) -> str:
    return re.sub(r"\s+", "", raw_key.strip())


async def validate_reseller_key(license_key: str) -> tuple[bool, str, dict | None]:
    params = {"api_key": RESELLER_API_KEY, "key": license_key}
    status_code = None

    try:
        timeout = aiohttp.ClientTimeout(total=10)
        async with aiohttp.ClientSession(timeout=timeout) as session:
            async with session.get(RESELLER_VALIDATE_URL, params=params) as resp:
                status_code = resp.status
                body = await resp.text()
    except asyncio.TimeoutError:
        return False, "Validation service timed out. Try again.", None
    except aiohttp.ClientError:
        return False, "Could not reach validation service. Try again.", None

    try:
        data = json.loads(body)
    except json.JSONDecodeError:
        return False, "Validation service returned invalid JSON.", None

    if not isinstance(data, dict):
        return False, "Validation service returned an invalid response.", None

    if status_code != 200:
        return False, str(data.get("message") or f"Validation failed (HTTP {status_code})."), None

    if data.get("status") != "success" or not data.get("valid", False):
        return False, str(data.get("message") or "Key is not valid."), None

    return True, "", data


async def grant_customer_role(user_id: int, reason: str = "Verified purchase"):
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
                await member.add_roles(role, reason=reason)
                return True
            except discord.Forbidden:
                continue
    return False


async def send_reseller_log(interaction: discord.Interaction, license_key: str, key_type: str):
    embed = accent_embed(
        "Reseller Whitelist",
        (
            f"{interaction.user.mention} whitelisted through reseller.\n"
            f"License: `{license_key}`\n"
            f"Type: `{key_type}`"
        ),
        discord.Color.green(),
    )

    channel = bot.get_channel(RESELLER_LOG_CHANNEL_ID) if RESELLER_LOG_CHANNEL_ID else None
    if channel is None and interaction.channel:
        channel = interaction.channel

    if channel:
        await channel.send(embed=embed)


def generate_license_key():
    return "-".join("".join(random.choices(string.ascii_uppercase + string.digits, k=4)) for _ in range(3))


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


class LicenseKeyView(View):
    def __init__(self, user_id, roblox_user_id, roblox_username):
        super().__init__(timeout=None)
        self.user_id = str(user_id)
        self.roblox_user_id = roblox_user_id
        self.roblox_username = roblox_username
        self.data = load_data()

        label = "Get My Key" if self.user_id in self.data else "Verify My Purchase"
        button = Button(label=label, style=discord.ButtonStyle.success, custom_id="verify_or_get_key")
        button.callback = self.verify_or_get
        self.add_item(button)

    async def verify_or_get(self, interaction: discord.Interaction):
        await interaction.response.defer()
        can_ephemeral = interaction.guild_id is not None
        self.data = load_data()

        if self.user_id in self.data:
            license_key = self.data[self.user_id]["license_key"]
            await interaction.followup.send(
                embed=accent_embed("Your License Key", f"You've already verified your purchase.\nHere is your license key:\n```{license_key}```"),
                ephemeral=can_ephemeral,
            )
            return

        if any(info.get("roblox_user_id") == self.roblox_user_id for info in self.data.values()):
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
                ),
                ephemeral=can_ephemeral,
            )
            return

        license_key = generate_license_key()
        timestamp = datetime.utcnow().isoformat()

        self.data[self.user_id] = {
            "roblox_username": self.roblox_username,
            "roblox_user_id": self.roblox_user_id,
            "datetime": timestamp,
            "license_key": license_key,
        }
        save_data(self.data)

        await grant_customer_role(int(self.user_id), reason="Verified Roblox shirt purchase")

        await interaction.followup.send(
            embed=accent_embed("Purchase Verified", "You've successfully purchased the product."),
        )
        await interaction.followup.send(
            embed=accent_embed(
                "License Key",
                f"Here is your license key:\n```{license_key}```\nPlease store it safely.",
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

        if any(info.get("roblox_user_id") == user_id for info in load_data().values()):
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
                (
                    f"Thanks! Username `{uname}` received.\n\n"
                    "I've sent you a message - please check your DMs to continue.\n"
                    "If you don't see it, make sure your DMs are enabled."
                ),
            ),
            ephemeral=True,
        )

        try:
            user = interaction.user

            purchase_embed = accent_embed(
                "Your Key is Almost Ready!",
                (
                    "To unlock access, follow these steps:\n"
                    "1) Open the product page using the button below.\n"
                    "2) Purchase the shirt with Robux.\n"
                    "3) Use the button in the next message to complete verification."
                ),
            )
            purchase_embed.set_image(url="https://media.discordapp.net/attachments/1370296862254563338/1370299705287901214/image.png")

            class ProductLink(View):
                def __init__(self):
                    super().__init__(timeout=None)
                    self.add_item(
                        discord.ui.Button(
                            label="Go to Product",
                            url="https://www.roblox.com/catalog/78409400320183/IMG-1296",
                            style=discord.ButtonStyle.link,
                        )
                    )

            await user.send(embed=purchase_embed, view=ProductLink())

            verify_embed = accent_embed(
                "Final Step: Claim Access",
                "Click the button below to verify your purchase or view your key.",
            )

            await user.send(embed=verify_embed, view=LicenseKeyView(user.id, user_id, uname))

        except discord.Forbidden:
            await interaction.followup.send("I couldn't DM you. Please enable DMs and try again.", ephemeral=True)


class PurchaseView(View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Purchase with Robux", style=discord.ButtonStyle.secondary, custom_id="purchase_robux")
    async def purchase_button(self, interaction: discord.Interaction, button: Button):
        await interaction.response.send_modal(RobloxUsernameModal())


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

        claims = load_reseller_claims()
        user_claim = claims.get(str(interaction.user.id))
        if user_claim:
            await interaction.response.send_message(
                embed=accent_embed(
                    "Already Claimed",
                    f"You already claimed a reseller key.\nYour key: `{user_claim['license_key']}`",
                    discord.Color.red(),
                ),
                ephemeral=True,
            )
            return

        key_claim = find_reseller_claim_by_key(license_key)
        if key_claim:
            owner_id, _ = key_claim
            if owner_id == str(interaction.user.id):
                await interaction.response.send_message(
                    embed=accent_embed("Already Claimed", f"You already claimed this key: `{license_key}`"),
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

        valid, message, key_info = await validate_reseller_key(license_key)
        if not valid:
            await interaction.response.send_message(
                embed=accent_embed("Invalid Key", message, discord.Color.red()),
                ephemeral=True,
            )
            return

        key_type = str((key_info or {}).get("type") or "lifetime")
        claims[str(interaction.user.id)] = {
            "license_key": license_key,
            "key_type": key_type,
            "datetime": datetime.utcnow().isoformat(),
        }
        save_reseller_claims(claims)

        role_granted = await grant_customer_role(interaction.user.id, reason="Whitelisted through reseller")

        response_lines = ["You've been whitelisted through reseller."]
        if role_granted:
            response_lines.append("Your customer role has been granted.")
        else:
            response_lines.append("I couldn't grant your role automatically. Please contact staff.")

        await interaction.response.send_message(
            embed=accent_embed("Reseller Claim Complete", "\n".join(response_lines), discord.Color.green()),
            ephemeral=True,
        )

        await send_reseller_log(interaction, license_key, key_type)


class ResellerClaimView(View):
    def __init__(self):
        super().__init__(timeout=None)

    @discord.ui.button(label="Claim Key", style=discord.ButtonStyle.success, custom_id="reseller_claim_key")
    async def claim_button(self, interaction: discord.Interaction, button: Button):
        await interaction.response.send_modal(ResellerClaimModal())


@bot.command()
@commands.has_permissions(administrator=True)
async def buyerinfo(ctx, member: discord.Member):
    data = load_data()
    reseller_claims = load_reseller_claims()
    user_id = str(member.id)

    if user_id not in data and user_id not in reseller_claims:
        await ctx.send(f"No purchase data found for {member.mention}.")
        return

    if user_id in data:
        entry = data[user_id]
        dt = datetime.fromisoformat(entry["datetime"])
        formatted_time = dt.strftime("%d %B %Y at %I:%M %p")

        roblox_username = entry["roblox_username"]
        roblox_url = f"https://www.roblox.com/users/profile?username={roblox_username}"

        embed = accent_embed(
            "Buyer Info",
            (
                f"**User:** {member.mention}\n"
                f"**Roblox Username:** [{roblox_username}]({roblox_url})\n"
                f"**License Key:** `{entry['license_key']}`\n"
                f"**Purchased At:** `{formatted_time}`"
            ),
        )
    else:
        entry = reseller_claims[user_id]
        dt = datetime.fromisoformat(entry["datetime"])
        formatted_time = dt.strftime("%d %B %Y at %I:%M %p")
        embed = accent_embed(
            "Buyer Info",
            (
                f"**User:** {member.mention}\n"
                f"**License Key:** `{entry['license_key']}`\n"
                f"**Duration:** `{entry.get('key_type', 'lifetime')}`\n"
                "**Payment Method:** `Reseller`\n"
                f"**Whitelisted At:** `{formatted_time}`"
            ),
        )
    await ctx.send(embed=embed)


@bot.command()
async def send_purchase(ctx):
    embed = accent_embed("Buy Premium Access", "Click below to purchase with Robux.")
    await ctx.send(embed=embed, view=PurchaseView())


@bot.command()
@commands.has_permissions(administrator=True)
async def send_reseller_panel(ctx):
    embed = accent_embed(
        "Claim Reseller Key",
        "Bought through a reseller? Click below, enter your key, and get whitelisted.",
    )
    await ctx.send(embed=embed, view=ResellerClaimView())


@bot.event
async def on_ready():
    if not getattr(bot, "_reseller_view_registered", False):
        bot.add_view(ResellerClaimView())
        bot._reseller_view_registered = True
    print(f"Logged in as {bot.user} ({bot.user.id})")


bot.run("MTQ1Mzg1MzU4NzgyMTI5NzcyNA.GHk2ov._nGNXczRHiiheqhvU2Oj9Wydf106dj9RfGwYUE")
