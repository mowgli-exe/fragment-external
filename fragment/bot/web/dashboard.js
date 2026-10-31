(() => {
    const elements = {
        apiKeyInput: document.getElementById('globalApiKey'),
        saveApiKey: document.getElementById('saveApiKey'),
        apiKeySaved: document.getElementById('apiKeySaved'),
        keyMaskInput: document.getElementById('keyMask'),
        saveKeyMask: document.getElementById('saveKeyMask'),
        keyMaskSaved: document.getElementById('keyMaskSaved'),
        duration: document.getElementById('duration'),
        amount: document.getElementById('amount'),
        generateBtn: document.getElementById('generateBtn'),
        keyList: document.getElementById('keyList'),
        copyAllBtn: document.getElementById('copyAllBtn'),
        revokeKey: document.getElementById('revokeKey'),
        revokeBtn: document.getElementById('revokeBtn'),
        resetKey: document.getElementById('resetKey'),
        resetBtn: document.getElementById('resetBtn'),
        timeType: document.getElementById('timeType'),
        addTimeAllBtn: document.getElementById('addTimeAllBtn'),
        addTimeUnusedBtn: document.getElementById('addTimeUnusedBtn'),
        exportKeysBtn: document.getElementById('exportKeysBtn'),
        exportArea: document.getElementById('exportArea'),
        exportTableBody: document.querySelector('#exportTable tbody'),
        filterUnused: document.getElementById('filterUnused'),
        copyExportBtn: document.getElementById('copyExportBtn'),
        downloadCSVBtn: document.getElementById('downloadCSVBtn'),
        notification: document.getElementById('notification')
    };

    document.addEventListener('DOMContentLoaded', () => {
        loadSavedValues();
        bindPersistence();
        bindActions();
    });

    function bindActions() {
        elements.generateBtn?.addEventListener('click', handleGenerate);
        elements.revokeBtn?.addEventListener('click', handleRevoke);
        elements.resetBtn?.addEventListener('click', handleReset);
        elements.addTimeAllBtn?.addEventListener('click', () => handleTimeChange(false));
        elements.addTimeUnusedBtn?.addEventListener('click', () => handleTimeChange(true));
        elements.exportKeysBtn?.addEventListener('click', handleExport);
        elements.filterUnused?.addEventListener('change', handleFilterUnused);
        elements.copyExportBtn?.addEventListener('click', handleCopyExport);
        elements.downloadCSVBtn?.addEventListener('click', handleDownloadCSV);
    }

    function bindPersistence() {
        elements.saveApiKey?.addEventListener('change', () => {
            togglePersistence('apiKey', elements.apiKeyInput.value, elements.saveApiKey.checked, elements.apiKeySaved, 'Saved locally');
        });

        elements.apiKeyInput?.addEventListener('input', () => {
            if (elements.saveApiKey.checked) {
                togglePersistence('apiKey', elements.apiKeyInput.value, true, elements.apiKeySaved, 'Saved locally');
            } else {
                setIndicator(elements.apiKeySaved, false);
            }
        });

        elements.saveKeyMask?.addEventListener('change', () => {
            togglePersistence('keyMask', elements.keyMaskInput.value, elements.saveKeyMask.checked, elements.keyMaskSaved, 'Mask saved');
        });

        elements.keyMaskInput?.addEventListener('input', () => {
            if (elements.saveKeyMask.checked) {
                togglePersistence('keyMask', elements.keyMaskInput.value, true, elements.keyMaskSaved, 'Mask saved');
            } else {
                setIndicator(elements.keyMaskSaved, false);
            }
        });
    }

    function loadSavedValues() {
        const savedApiKey = localStorage.getItem('apiKey');
        if (savedApiKey) {
            elements.apiKeyInput.value = savedApiKey;
            elements.saveApiKey.checked = true;
            setIndicator(elements.apiKeySaved, true, 'Saved locally');
        }

        const savedMask = localStorage.getItem('keyMask');
        if (savedMask) {
            elements.keyMaskInput.value = savedMask;
            elements.saveKeyMask.checked = true;
            setIndicator(elements.keyMaskSaved, true, 'Mask saved');
        }
    }

    function togglePersistence(key, value, shouldSave, indicator, text) {
        if (shouldSave && value) {
            localStorage.setItem(key, value);
            setIndicator(indicator, true, text);
        } else {
            localStorage.removeItem(key);
            setIndicator(indicator, false);
        }
    }

    function handleGenerate() {
        const apiKey = getApiKey();
        if (!apiKey) return;

        const url = buildUrl('create', {
            apikey: apiKey,
            duration: elements.duration.value,
            amount: elements.amount.value,
            keymask: elements.keyMaskInput.value
        });

        performAction(url, {
            successMessage: (data) => {
                if (data.keys && data.keys.length) {
                    return `Generated ${data.keys.length} key(s).`;
                }
                return 'Keys generated successfully.';
            },
            failureMessage: 'Error generating keys',
            onSuccess: (data) => {
                renderGeneratedKeys(data.keys || []);
            }
        });
    }

    function handleRevoke() {
        const apiKey = getApiKey();
        if (!apiKey) return;

        const keyToRevoke = elements.revokeKey.value.trim();
        if (!keyToRevoke) {
            showNotification('Key to revoke is required', false);
            return;
        }

        const url = buildUrl('revoke', { apikey: apiKey, key: keyToRevoke });
        performAction(url, {
            successMessage: (data) => data.message || 'Key revoked successfully.',
            failureMessage: 'Error revoking key',
            onSuccess: () => {
                elements.revokeKey.value = '';
            }
        });
    }

    function handleReset() {
        const apiKey = getApiKey();
        if (!apiKey) return;

        const keyToReset = elements.resetKey.value.trim();
        if (!keyToReset) {
            showNotification('Key to reset HWID is required', false);
            return;
        }

        const url = buildUrl('resethwid', { apikey: apiKey, key: keyToReset });
        performAction(url, {
            successMessage: (data) => data.message || 'HWID reset successfully.',
            failureMessage: 'Error resetting HWID',
            onSuccess: () => {
                elements.resetKey.value = '';
            }
        });
    }

    function handleTimeChange(onlyUnused) {
        const apiKey = getApiKey();
        if (!apiKey) return;

        const action = onlyUnused ? 'addtimeunused' : 'addtime';
        const url = buildUrl(action, { apikey: apiKey, timetype: elements.timeType.value });

        performAction(url, {
            successMessage: (data) => data.message || 'Time updated successfully.',
            failureMessage: 'Error updating time'
        });
    }

    function handleExport() {
        const apiKey = getApiKey();
        if (!apiKey) return;

        const url = buildUrl('exportkeys', { apikey: apiKey });
        performAction(url, {
            successMessage: (data) => data.message || 'Keys exported successfully.',
            failureMessage: 'Error exporting keys',
            onSuccess: (data) => {
                toggleVisibility(elements.exportArea, true);
                displayExportedKeys(data.keys || []);
            }
        });
    }

    function handleFilterUnused() {
        const showUnused = elements.filterUnused.checked;
        const rows = document.querySelectorAll('#exportTable tbody tr');
        rows.forEach((row) => {
            row.classList.toggle('hidden', showUnused && row.dataset.status !== 'Unused');
        });
    }

    function handleCopyExport() {
        const table = document.getElementById('exportTable');
        const filterUnused = elements.filterUnused.checked;
        let text = '';

        const headers = Array.from(table.querySelectorAll('thead th')).map((th) => th.textContent.trim());
        text += headers.join(',') + '\n';

        const rows = Array.from(table.querySelectorAll('tbody tr'));
        rows.forEach((row) => {
            if (filterUnused && row.dataset.status !== 'Unused') {
                return;
            }
            const cells = Array.from(row.querySelectorAll('td')).map((td) => td.textContent.trim());
            text += cells.join(',') + '\n';
        });

        copyToClipboard(text);
        showNotification('Export copied to clipboard.');
    }

    function handleDownloadCSV() {
        const table = document.getElementById('exportTable');
        const filterUnused = elements.filterUnused.checked;
        let csv = '';

        const headers = Array.from(table.querySelectorAll('thead th')).map((th) => th.textContent.trim());
        csv += headers.join(',') + '\n';

        const rows = Array.from(table.querySelectorAll('tbody tr'));
        rows.forEach((row) => {
            if (filterUnused && row.dataset.status !== 'Unused') {
                return;
            }
            const cells = Array.from(row.querySelectorAll('td')).map((td) => {
                const content = td.textContent.trim().replace(/"/g, '""');
                return `"${content}"`;
            });
            csv += cells.join(',') + '\n';
        });

        const blob = new Blob([csv], { type: 'text/csv' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.setAttribute('hidden', '');
        a.href = url;
        a.download = 'license_keys.csv';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);
    }

    function performAction(url, { successMessage, onSuccess, failureMessage }) {
        fetch(url)
            .then((response) => {
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                return response.json();
            })
            .then((data) => {
                if (data.status === 'success') {
                    if (onSuccess) {
                        onSuccess(data);
                    }
                    const resolvedMessage = typeof successMessage === 'function'
                        ? successMessage(data)
                        : successMessage || data.message;
                    if (resolvedMessage) {
                        showNotification(resolvedMessage, true);
                    }
                } else {
                    showNotification(failureMessage ? `${failureMessage}: ${data.message}` : data.message, false);
                }
            })
            .catch(() => {
                showNotification('Request failed. Please try again.', false);
            });
    }

    function renderGeneratedKeys(keys) {
        if (!elements.keyList) return;
        elements.keyList.innerHTML = '';

        if (!keys || keys.length === 0) {
            elements.keyList.innerHTML = '<p class="col-span-2 text-sm text-slate-400">No keys generated yet.</p>';
            toggleVisibility(elements.copyAllBtn, false);
            return;
        }

        keys.forEach((key) => {
            const item = document.createElement('div');
            item.className = 'flex items-center justify-between gap-3 rounded-xl border border-slate-800 bg-slate-900/70 px-4 py-3 text-sm shadow-inner shadow-black/30';

            const text = document.createElement('span');
            text.className = 'font-mono text-slate-100';
            text.textContent = key;

            const copyBtn = document.createElement('button');
            copyBtn.className = 'inline-flex items-center gap-1 rounded-lg border border-slate-700 bg-slate-800 px-3 py-2 text-xs font-semibold text-slate-100 transition hover:border-indigo-400 hover:bg-indigo-500/15';
            copyBtn.innerHTML = '<i class="fa-regular fa-copy"></i> Copy';
            copyBtn.addEventListener('click', () => {
                copyToClipboard(key);
                showNotification('Key copied.');
            });

            item.appendChild(text);
            item.appendChild(copyBtn);
            elements.keyList.appendChild(item);
        });

        toggleVisibility(elements.copyAllBtn, true);
        elements.copyAllBtn.onclick = () => {
            copyToClipboard(keys.join('\n'));
            showNotification(`Copied ${keys.length} key(s).`);
        };
    }

    function displayExportedKeys(keys) {
        if (!elements.exportTableBody) return;
        elements.exportTableBody.innerHTML = '';

        keys.forEach((keyData) => {
            const row = document.createElement('tr');
            row.dataset.status = keyData.status;
            row.className = 'text-sm text-slate-200 hover:bg-slate-800/50';

            const cells = [
                keyData.key,
                keyData.status,
                keyData.expiry || '-',
                keyData.type || '-'
            ];

            cells.forEach((cell) => {
                const td = document.createElement('td');
                td.className = 'px-4 py-3 align-top';
                td.textContent = cell;
                row.appendChild(td);
            });

            elements.exportTableBody.appendChild(row);
        });
    }

    function buildUrl(action, params) {
        const url = new URL('renter.php', window.location.href);
        url.searchParams.set('action', action);
        Object.entries(params || {}).forEach(([key, value]) => {
            if (value !== undefined && value !== null) {
                url.searchParams.set(key, value);
            }
        });
        return url.toString();
    }

    function getApiKey() {
        const apiKey = elements.apiKeyInput.value.trim();
        if (!apiKey) {
            showNotification('API key is required', false);
            return null;
        }
        return apiKey;
    }

    function copyToClipboard(text) {
        navigator.clipboard.writeText(text).catch(() => {
            showNotification('Clipboard unavailable', false);
        });
    }

    function showNotification(message, isSuccess = true) {
        if (!elements.notification) return;
        const el = elements.notification;
        el.textContent = message;

        el.classList.remove('hidden');
        el.classList.remove('bg-emerald-600/80', 'border-emerald-500/50', 'bg-rose-600/80', 'border-rose-500/60');

        if (isSuccess) {
            el.classList.add('bg-emerald-600/80', 'border-emerald-500/50');
        } else {
            el.classList.add('bg-rose-600/80', 'border-rose-500/60');
        }

        el.classList.add('opacity-0', 'translate-y-2');
        requestAnimationFrame(() => {
            el.classList.remove('opacity-0', 'translate-y-2');
        });

        clearTimeout(showNotification.timer);
        showNotification.timer = setTimeout(() => {
            el.classList.add('opacity-0', 'translate-y-2');
            setTimeout(() => el.classList.add('hidden'), 250);
        }, 2600);
    }

    function setIndicator(element, show, text) {
        if (!element) return;
        if (text) {
            element.textContent = text;
        }
        element.classList.toggle('hidden', !show);
    }

    function toggleVisibility(element, show) {
        if (!element) return;
        element.classList.toggle('hidden', !show);
    }
})();
