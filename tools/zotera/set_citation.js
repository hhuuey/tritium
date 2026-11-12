// ...existing code...
async function fetchCitationCountByDOI(doi) {
    if (!doi) return -1;
    try {
        const res = await (typeof fetch !== 'undefined' ? fetch : window.fetch)('https://doi.org/' + doi, {
            headers: {
                Accept: 'application/vnd.citationstyles.csl+json',
                'User-Agent': `mailto:zoterotag@euclpts.com; ${typeof navigator !== 'undefined' ? navigator.userAgent : 'unknown'}`
            }
        });
        if (!res || !res.ok) return -1;
        const json = await res.json();
        const cnt = json && (json['is-referenced-by-count'] ?? json['isReferencedByCount']);
        if (typeof cnt === 'number') return cnt;
        const parsed = Number(cnt);
        return Number.isFinite(parsed) ? parsed : -1;
    } catch (e) {
        return -1;
    }
}

function nowStr() {
    const d = new Date();
    const p = n => String(n).padStart(2, '0');
    return `${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())}-${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}`;
}

(async function processItems(items) {
    if (!items) return 'no items provided';
    let iterable = items;
    if (typeof items.forEach !== 'function' && typeof items[Symbol.iterator] !== 'function' && typeof items.length === 'number') {
        iterable = (function*(arr){ for (let i=0;i<arr.length;i++) yield arr[i]; })(items);
    }

    let total = 0, updated = 0;
    for await (const item of iterable) {
        total++;
        try {
            const getField = typeof item.getField === 'function' ? item.getField.bind(item) : () => '';
            const setField = typeof item.setField === 'function' ? item.setField.bind(item) : null;

            let extra = (getField('extra') || '');
            // normalize line endings
            extra = extra.replace(/\r\n/g, '\n');

            const citeMatch = extra.match(/cite\s*:\s*(-?\d+)/i);
            const timeRe = /^\s*cite_update_time\s*:\s*.*$/im;

            if (!citeMatch) {
                // add cite:-1 and update time
                const appendCite = 'cite:-1';
                const ts = `cite_update_time: ${nowStr()}`;
                // remove existing cite_update_time if any, then append
                const cleaned = extra.replace(timeRe, '').trim();
                const newExtra = cleaned ? `${cleaned}\n${appendCite}\n${ts}` : `${appendCite}\n${ts}`;
                if (setField) {
                    try { setField('extra', newExtra); } catch (_) {}
                    try {
                        if (typeof item.saveTx === 'function') await item.saveTx();
                        else if (typeof item.save === 'function') { const r = item.save(); if (r && typeof r.then === 'function') await r; }
                    } catch (_) {}
                    updated++;
                }
                continue;
            }

            const origCount = parseInt(citeMatch[1], 10);
            const doi = (getField('DOI') || '').trim();
            const fetched = await fetchCitationCountByDOI(doi);

            // choose which count to keep: fetched if valid, otherwise keep original
            let useCount = origCount;
            if (fetched !== -1) useCount = fetched;

            // always update cite_update_time regardless of success
            const ts = `cite_update_time: ${nowStr()}`;

            // replace existing cite and cite_update_time (if present) then append time
            let replaced = extra.replace(/cite\s*:\s*-?\d+/i, `cite:${useCount}`);
            // remove prior cite_update_time lines
            replaced = replaced.replace(timeRe, '').trim();
            const newExtra = replaced ? `${replaced}\n${ts}` : ts;

            if (setField) {
                try { setField('extra', newExtra); } catch (_) {}
                try {
                    if (typeof item.saveTx === 'function') await item.saveTx();
                    else if (typeof item.save === 'function') { const r = item.save(); if (r && typeof r.then === 'function') await r; }
                } catch (_) {}
                updated++;
            }
        } catch (e) {
            // ignore single item errors
        }
    }

    return `检查${total}个条目，转存/更新${updated}条知网引用量`;
})(typeof items !== 'undefined' ? items : null);