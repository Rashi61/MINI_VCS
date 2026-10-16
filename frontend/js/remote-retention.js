const $ = (id) => document.getElementById(id);

function showMessage(text, ok) {
  const m = $('msg');
  m.textContent = text;
  m.className = 'msg ' + (ok ? 'ok' : 'err');
  m.hidden = false;
}

async function post(url, data) {
  try {
    const res = await fetch(url, { method: 'POST', body: new URLSearchParams(data || {}) });
    const d = await res.json();
    showMessage(d.message, d.ok);
    await refresh();
    return d;
  } catch (e) {
    showMessage("Couldn't reach the server. Check that minivcs is running.", false);
  }
}

function cell(text, cls) {
  const td = document.createElement('td');
  td.textContent = text;
  if (cls) td.className = cls;
  return td;
}
function badgeCell(text) {
  const td = document.createElement('td');
  const b = document.createElement('span');
  b.className = 'badge ' + text.toLowerCase().split(' ')[0];
  b.textContent = text;
  td.appendChild(b);
  return td;
}
function fill(tbodyId, rows) {
  const tb = $(tbodyId);
  tb.innerHTML = '';
  rows.forEach((cells) => {
    const tr = document.createElement('tr');
    cells.forEach((c) => tr.appendChild(c));
    tb.appendChild(tr);
  });
}
const ageText = (d) => (d < 0 ? '-' : d + (d === 1 ? ' day' : ' days'));

async function refresh() {
  try {
    const res = await fetch('/api/status');
    const d = await res.json();

    if ($('statLocal')) {
      $('statLocal').textContent = d.local;
      $('statRemote').textContent = d.remote;
      $('statAhead').textContent = d.ahead;
      $('statBehind').textContent = d.behind;
    }

    if ($('historyRows')) {
      fill('historyRows', d.snapshots.map((s) => [
        cell(s.name, 'mono'), cell(s.message), cell(s.date.slice(0, 10)),
        cell(ageText(s.age)), badgeCell(s.status),
      ]));
      $('empty').hidden = d.snapshots.length > 0;
    }

    if ($('retentionRows')) {
      if (document.activeElement !== $('days')) $('days').value = d.retention;
      $('pending').textContent =
        d.pending + ' snapshot(s) are older than ' + d.retention +
        ' days and will be deleted. The latest version is always kept.';
      const live = d.snapshots.filter((s) => s.status === 'ACTIVE');
      fill('retentionRows', live.map((s) => [
        cell(s.name, 'mono'), cell(ageText(s.age)), badgeCell(s.result),
      ]));
      $('empty').hidden = live.length > 0;
    }
  } catch (e) {
    showMessage("Couldn't load data. Check that minivcs is running.", false);
  }
}

const on = (id, fn) => { if ($(id)) $(id).onclick = fn; };

on('btnClone', () => post('/api/clone'));
on('btnPush', () => post('/api/push'));
on('btnPull', () => post('/api/pull'));
on('btnCommit', async () => {
  const d = await post('/api/commit', {
    message: $('cMsg').value.trim(),
    content: $('cContent').value,
    age: $('cAge').value || '0',
  });
  if (d && d.ok) { $('cMsg').value = ''; $('cContent').value = ''; }
});
on('btnSaveDays', () => post('/api/retention', { days: $('days').value }));
on('btnCleanup', () => post('/api/cleanup'));

refresh();
