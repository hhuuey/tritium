/**
 * Show Citation Counts
 * @author volatile-static
 * @usage plug-and-play
 * @see https://github.com/windingwind/zotero-actions-tags/discussions/150
 */
const pluginID = 'zoterotag@euclpts.com', // 与package.json一致
        window = require('window'),
        _caches = {};  // 缓存获取到的引用数

// ...existing code...
function renderCell(_, data, column) {
    const span = window.document.createElement('span');
    span.classList.add('cell');
    column.className.split(' ').forEach(name => span.classList.add(name));

    // 使用 dataProvider 传过来的值（直接由 extra 中的 cite 提取）
    try {
        // data 已经是提取后的数字字符串或 '-1'
        span.textContent = String(data ?? '-1');
    } catch (e) {
        span.textContent = '-1';
    }
    return span;
}
// ...existing code...

require('Zotero').ItemTreeManager.registerColumns({
        dataKey: 'action-citation-counts',
        label: 'Citations',
        dataProvider: item => {
            try {
                const extra = item.getField && item.getField('extra') || '';
                const m = extra.match(/cite\s*:\s*(-?\d+)/i);
                return m ? String(Number(m[1])) : '';
            } catch (e) {
                return '';
            }
        },
        pluginID,  // 禁用Tag插件后消失
        renderCell
});