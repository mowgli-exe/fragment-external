<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>License Key Dashboard</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css">
    <script src="https://cdn.tailwindcss.com"></script>
    <script>
        tailwind.config = {
            theme: {
                extend: {
                    fontFamily: {
                        display: ['Space Grotesk', 'ui-sans-serif', 'system-ui']
                    }
                }
            }
        }
    </script>
</head>
<body class="min-h-screen bg-slate-950 text-slate-100 font-display relative overflow-x-hidden">
    <div class="pointer-events-none absolute inset-0 -z-10 bg-[radial-gradient(circle_at_20%_20%,rgba(79,70,229,0.18),transparent_35%),radial-gradient(circle_at_85%_10%,rgba(16,185,129,0.18),transparent_30%),radial-gradient(circle_at_50%_90%,rgba(59,130,246,0.14),transparent_32%)]"></div>
    <div class="pointer-events-none absolute inset-0 -z-20 bg-[linear-gradient(120deg,rgba(255,255,255,0.04)_1px,transparent_1px),linear-gradient(0deg,rgba(255,255,255,0.03)_1px,transparent_1px)] bg-[size:36px_36px] opacity-30"></div>

    <div class="relative max-w-6xl mx-auto px-4 py-10 space-y-8">
        <header class="flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
            <div>
                <p class="text-xs uppercase tracking-[0.35em] text-indigo-300">Control Center</p>
                <h1 class="mt-2 text-3xl font-semibold leading-tight text-white">License Key Dashboard</h1>
                <p class="mt-2 text-sm text-slate-400">Issue, revoke, extend, and export keys from a refreshed dark UI.</p>
            </div>
            <div class="flex items-center gap-3 text-xs text-slate-300">
                <span class="inline-flex items-center gap-2 rounded-full border border-emerald-500/50 bg-emerald-500/10 px-3 py-1 font-semibold text-emerald-100">
                    <span class="h-2 w-2 rounded-full bg-emerald-400 animate-pulse"></span>
                    Live
                </span>
                <span class="inline-flex items-center gap-2 rounded-full border border-indigo-500/50 bg-indigo-500/10 px-3 py-1 font-semibold text-indigo-100">
                    <i class="fa-solid fa-moon"></i>
                    Dark Mode
                </span>
            </div>
        </header>

        <section class="rounded-2xl border border-slate-800/70 bg-slate-900/70 shadow-[0_20px_70px_rgba(0,0,0,0.45)] backdrop-blur-xl p-6 space-y-4">
            <div class="flex items-start justify-between gap-4">
                <div>
                    <div class="inline-flex items-center gap-2 rounded-full border border-indigo-400/40 bg-indigo-500/10 px-3 py-1 text-xs font-semibold text-indigo-100">
                        <i class="fa-solid fa-shield-halved"></i>
                        Global Settings
                    </div>
                    <h2 class="mt-3 text-xl font-semibold text-white">API Key</h2>
                    <p class="text-sm text-slate-400">Set one API key to drive every action. You can optionally store it locally in the browser.</p>
                </div>
                <div class="text-xs text-slate-400">Client-side only</div>
            </div>
            <div class="grid gap-4 md:grid-cols-[1.6fr_auto] md:items-end">
                <div class="space-y-3">
                    <label for="globalApiKey" class="text-sm font-semibold text-slate-200">API Key</label>
                    <input type="password" id="globalApiKey" placeholder="Enter your API key" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-4 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 placeholder:text-slate-500 focus:border-indigo-400 focus:outline-none focus:ring-2 focus:ring-indigo-400/40">
                    <label class="inline-flex items-center gap-2 text-sm text-slate-300">
                        <input type="checkbox" id="saveApiKey" class="h-4 w-4 rounded border-slate-700 bg-slate-900 text-indigo-400 focus:ring-indigo-400/40">
                        Remember API key on this device
                    </label>
                </div>
                <div class="flex items-center gap-3">
                    <span id="apiKeySaved" class="hidden rounded-lg border border-emerald-500/50 bg-emerald-500/10 px-3 py-2 text-xs font-semibold text-emerald-100">Saved locally</span>
                </div>
            </div>
        </section>

        <section class="grid gap-4 lg:grid-cols-3">
            <div class="lg:col-span-2 rounded-2xl border border-slate-800/70 bg-slate-900/70 shadow-[0_20px_70px_rgba(0,0,0,0.45)] backdrop-blur-xl p-6 space-y-4">
                <div class="flex items-start justify-between gap-3">
                    <div>
                        <h2 class="text-xl font-semibold text-white">Generate Keys</h2>
                        <p class="text-sm text-slate-400">Create one or many keys with optional masks to keep them branded.</p>
                    </div>
                    <span class="rounded-full bg-indigo-500/15 px-3 py-1 text-xs font-semibold text-indigo-100">Create</span>
                </div>
                <div class="grid gap-4 md:grid-cols-3">
                    <div class="space-y-2">
                        <label for="duration" class="text-sm font-semibold text-slate-200">Duration</label>
                        <select id="duration" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 focus:border-indigo-400 focus:outline-none focus:ring-2 focus:ring-indigo-400/40">
                            <option value="day" selected>1 Day</option>
                            <option value="2 day">2 Days</option>
                            <option value="3 day">3 Days</option>
                            <option value="week">1 Week</option>
                            <option value="month">1 Month</option>
                            <option value="3 month">3 Months</option>
                            <option value="lifetime">Lifetime</option>
                        </select>
                    </div>
                    <div class="space-y-2">
                        <label for="amount" class="text-sm font-semibold text-slate-200">Amount</label>
                        <input type="number" id="amount" min="1" max="100" value="1" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 focus:border-indigo-400 focus:outline-none focus:ring-2 focus:ring-indigo-400/40">
                    </div>
                    <div class="space-y-2">
                        <label for="keyMask" class="text-sm font-semibold text-slate-200">Key Mask</label>
                        <input type="text" id="keyMask" value="****-****-****" placeholder="****-****-****" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 focus:border-indigo-400 focus:outline-none focus:ring-2 focus:ring-indigo-400/40">
                        <label class="inline-flex items-center gap-2 text-sm text-slate-300">
                            <input type="checkbox" id="saveKeyMask" class="h-4 w-4 rounded border-slate-700 bg-slate-900 text-indigo-400 focus:ring-indigo-400/40">
                            Remember mask
                        </label>
                        <span id="keyMaskSaved" class="hidden text-xs font-semibold text-emerald-200">Mask saved</span>
                    </div>
                </div>
                <button id="generateBtn" class="inline-flex items-center justify-center gap-2 rounded-xl bg-indigo-500 px-4 py-3 text-sm font-semibold text-white shadow-lg shadow-indigo-500/25 transition hover:bg-indigo-400 focus-visible:outline focus-visible:outline-2 focus-visible:outline-indigo-300">
                    <i class="fa-solid fa-wand-magic-sparkles"></i>
                    Generate key(s)
                </button>
                <div id="keyList" class="grid gap-3 sm:grid-cols-2"></div>
                <button id="copyAllBtn" class="hidden w-full rounded-xl border border-slate-700 bg-slate-800 px-4 py-3 text-sm font-semibold text-slate-100 transition hover:border-indigo-400 hover:bg-indigo-500/20 focus-visible:outline focus-visible:outline-2 focus-visible:outline-indigo-300">
                    <i class="fa-regular fa-copy"></i>
                    Copy all keys
                </button>
            </div>

            <div class="rounded-2xl border border-slate-800/70 bg-slate-900/70 shadow-[0_20px_70px_rgba(0,0,0,0.45)] backdrop-blur-xl p-6 space-y-4">
                <div class="flex items-start justify-between gap-3">
                    <div>
                        <h2 class="text-xl font-semibold text-white">Quick Links</h2>
                        <p class="text-sm text-slate-400">Fast actions for existing keys.</p>
                    </div>
                    <span class="rounded-full bg-emerald-500/15 px-3 py-1 text-xs font-semibold text-emerald-100">Live</span>
                </div>
                <div class="space-y-3">
                    <label for="revokeKey" class="text-sm font-semibold text-slate-200">Revoke a key</label>
                    <input type="text" id="revokeKey" placeholder="Enter key to revoke" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 focus:border-rose-400 focus:outline-none focus:ring-2 focus:ring-rose-400/30">
                    <button id="revokeBtn" class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-rose-500 px-4 py-3 text-sm font-semibold text-white shadow-lg shadow-rose-500/25 transition hover:bg-rose-400 focus-visible:outline focus-visible:outline-2 focus-visible:outline-rose-300">
                        <i class="fa-solid fa-ban"></i>
                        Revoke key
                    </button>
                </div>
                <div class="pt-2 border-t border-slate-800/70 space-y-3">
                    <label for="resetKey" class="text-sm font-semibold text-slate-200">Reset HWID</label>
                    <input type="text" id="resetKey" placeholder="Enter key to reset HWID" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 focus:border-amber-400 focus:outline-none focus:ring-2 focus:ring-amber-400/30">
                    <button id="resetBtn" class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-amber-500 px-4 py-3 text-sm font-semibold text-slate-900 shadow-lg shadow-amber-500/20 transition hover:bg-amber-400 focus-visible:outline focus-visible:outline-2 focus-visible:outline-amber-300">
                        <i class="fa-solid fa-rotate-left"></i>
                        Reset HWID
                    </button>
                </div>
            </div>
        </section>

        <section class="grid gap-4 md:grid-cols-2">
            <div class="rounded-2xl border border-slate-800/70 bg-slate-900/70 shadow-[0_20px_70px_rgba(0,0,0,0.45)] backdrop-blur-xl p-6 space-y-4">
                <div class="flex items-start justify-between gap-3">
                    <div>
                        <h2 class="text-xl font-semibold text-white">Time Management</h2>
                        <p class="text-sm text-slate-400">Extend access for everyone or just unused keys.</p>
                    </div>
                    <span class="rounded-full bg-sky-500/15 px-3 py-1 text-xs font-semibold text-sky-100">Adjust</span>
                </div>
                <div class="space-y-3">
                    <label for="timeType" class="text-sm font-semibold text-slate-200">Add Time</label>
                    <select id="timeType" class="w-full rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-100 shadow-inner shadow-black/30 focus:border-sky-400 focus:outline-none focus:ring-2 focus:ring-sky-400/30">
                        <option value="day">1 Day</option>
                        <option value="2 day">2 Days</option>
                        <option value="3 day">3 Days</option>
                    </select>
                    <div class="grid gap-3 sm:grid-cols-2">
                        <button id="addTimeAllBtn" class="inline-flex items-center justify-center gap-2 rounded-xl bg-sky-500 px-4 py-3 text-sm font-semibold text-slate-900 shadow-lg shadow-sky-500/25 transition hover:bg-sky-400 focus-visible:outline focus-visible:outline-2 focus-visible:outline-sky-300">
                            <i class="fa-solid fa-clock"></i>
                            Add to all keys
                        </button>
                        <button id="addTimeUnusedBtn" class="inline-flex items-center justify-center gap-2 rounded-xl bg-slate-800 px-4 py-3 text-sm font-semibold text-slate-100 border border-slate-700 transition hover:border-sky-400 hover:bg-sky-500/15 focus-visible:outline focus-visible:outline-2 focus-visible:outline-sky-300">
                            <i class="fa-regular fa-clock"></i>
                            Add to unused only
                        </button>
                    </div>
                </div>
            </div>

            <div class="rounded-2xl border border-slate-800/70 bg-slate-900/70 shadow-[0_20px_70px_rgba(0,0,0,0.45)] backdrop-blur-xl p-6 space-y-4">
                <div class="flex items-start justify-between gap-3">
                    <div>
                        <h2 class="text-xl font-semibold text-white">Export Keys</h2>
                        <p class="text-sm text-slate-400">Preview, filter, copy, or download a CSV of everything.</p>
                    </div>
                    <span class="rounded-full bg-teal-500/15 px-3 py-1 text-xs font-semibold text-teal-100">Exports</span>
                </div>
                <button id="exportKeysBtn" class="w-full inline-flex items-center justify-center gap-2 rounded-xl bg-teal-500 px-4 py-3 text-sm font-semibold text-slate-900 shadow-lg shadow-teal-500/25 transition hover:bg-teal-400 focus-visible:outline focus-visible:outline-2 focus-visible:outline-teal-300">
                    <i class="fa-solid fa-file-export"></i>
                    Export all keys
                </button>
                <div id="exportArea" class="hidden space-y-3">
                    <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
                        <label class="inline-flex items-center gap-2 text-sm text-slate-300">
                            <input type="checkbox" id="filterUnused" class="h-4 w-4 rounded border-slate-700 bg-slate-900 text-teal-400 focus:ring-teal-400/40">
                            Show only unused
                        </label>
                        <div class="flex flex-col gap-2 sm:flex-row">
                            <button id="copyExportBtn" class="inline-flex items-center justify-center gap-2 rounded-lg border border-slate-700 bg-slate-800 px-3 py-2 text-xs font-semibold text-slate-100 transition hover:border-teal-400 hover:bg-teal-500/15">
                                <i class="fa-regular fa-copy"></i>
                                Copy table
                            </button>
                            <button id="downloadCSVBtn" class="inline-flex items-center justify-center gap-2 rounded-lg border border-slate-700 bg-slate-800 px-3 py-2 text-xs font-semibold text-slate-100 transition hover:border-teal-400 hover:bg-teal-500/15">
                                <i class="fa-solid fa-download"></i>
                                Download CSV
                            </button>
                        </div>
                    </div>
                    <div class="rounded-xl border border-slate-800 bg-slate-900/60">
                        <div class="max-h-72 overflow-auto">
                            <table id="exportTable" class="w-full text-sm">
                                <thead class="sticky top-0 bg-slate-900/80 backdrop-blur">
                                    <tr class="text-left text-slate-300">
                                        <th class="px-4 py-3 font-semibold">Key</th>
                                        <th class="px-4 py-3 font-semibold">Status</th>
                                        <th class="px-4 py-3 font-semibold">Expiry</th>
                                        <th class="px-4 py-3 font-semibold">Type</th>
                                    </tr>
                                </thead>
                                <tbody class="divide-y divide-slate-800"></tbody>
                            </table>
                        </div>
                    </div>
                </div>
            </div>
        </section>
    </div>

    <div id="notification" class="fixed right-5 top-5 z-50 hidden rounded-xl border border-emerald-500/50 bg-emerald-600/80 px-4 py-3 text-sm font-semibold text-white shadow-2xl backdrop-blur transition-all duration-300"></div>

    <script src="dashboard.js"></script>
</body>
</html>
