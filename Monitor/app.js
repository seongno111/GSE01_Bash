'use strict';

const $ = id => document.getElementById(id);
const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const number = (value, digits = 0) => Number.isFinite(value) ? value.toLocaleString('ko-KR', { maximumFractionDigits: digits }) : '—';
const position = p => p ? `${number(p.x, 1)}, ${number(p.y, 1)}` : '—';
let data = null;
let selected = null;
let paused = false;

function villageName(id) {
    return data?.villages.find(v => v.id === id)?.name || '마을 밖';
}

function metric(label, value, note) {
    return `<article class="metric"><span>${escape(label)}</span><strong>${number(value)}</strong><small>${escape(note)}</small></article>`;
}

function stats(entries) {
    return `<div class="stats-grid">${entries.map(([label, value]) => `<div><span>${escape(label)}</span>${escape(value)}</div>`).join('')}</div>`;
}

function options(id, items, initial) {
    const current = $(id).value;
    $(id).innerHTML = initial + items.map(([key, name]) => `<option value="${escape(key)}">${escape(name)}</option>`).join('');
    if ([...$(id).options].some(option => option.value === current)) $(id).value = current;
}

function filteredNpcs() {
    const query = $('search').value.trim().toLocaleLowerCase();
    return data.npcs.filter(npc => {
        const village = $('village').value;
        const state = $('state').value;
        return (!village || (village === 'outside' ? npc.currentVillageId === null : npc.currentVillageId === village))
            && (!$('role').value || npc.role === $('role').value)
            && (!state || (state === 'alive' ? npc.alive : state === 'dead' ? !npc.alive : npc.alive && ['전투', '대피'].includes(npc.mode)))
            && (!query || `${npc.name} ${npc.action} ${npc.id}`.toLocaleLowerCase().includes(query));
    });
}

function renderResidents() {
    if (!data) return;
    const rows = filteredNpcs();
    $('npcs').innerHTML = rows.map(npc => `<tr class="${selected === npc.id ? 'selected' : ''}">
        <td><button class="name-button" data-id="${escape(npc.id)}">${escape(npc.name)}</button><small>#${escape(npc.id)} · ${npc.alive ? '생존' : '사망'}</small></td>
        <td>${escape(npc.role)}<small>${escape(npc.goal)}</small></td>
        <td>${escape(villageName(npc.currentVillageId))}<small>${position(npc.position)}</small></td>
        <td>${number(npc.health)} / ${number(npc.maxHealth)}<progress max="${Math.max(1, npc.maxHealth || 1)}" value="${Math.max(0, npc.health || 0)}" aria-label="체력"></progress></td>
        <td>${escape(npc.mode)} · ${escape(npc.action)}<small>${escape(npc.phase)}${npc.distant ? ' · 원거리 요약' : ''}</small></td>
        <td>${number(npc.gold)}</td><td class="${npc.frenzy ? 'error' : ''}">${number(npc.corruption, 1)}${npc.frenzy ? ' · 광폭' : ''}</td>
        <td>${number(npc.distanceTravelled, 1)}<small>${number(npc.completed)} 완료 / ${number(npc.failed)} 실패</small></td></tr>`).join('');
    $('count').textContent = `표시 ${rows.length}명 / 등록 ${data.npcs.length}명 · 마을 필터는 현재 위치 기준`;
    if (!rows.length) $('npcs').innerHTML = '<tr><td colspan="8">조건에 맞는 NPC가 없습니다.</td></tr>';
    renderDetail();
    renderMap();
}

function renderDetail() {
    const npc = data?.npcs.find(n => n.id === selected);
    if (!npc) {
        $('detail').innerHTML = '<h2>주민 상세</h2><p>NPC 이름을 누르면 능력치와 행동 판단을 볼 수 있습니다.</p>';
        return;
    }
    const candidates = [...npc.candidates].sort((a, b) => b.probability - a.probability);
    $('detail').innerHTML = `<h2>${escape(npc.name)} · ${escape(npc.weapon)}</h2>${stats([
        ['소속 마을', npc.homeVillageId ? villageName(npc.homeVillageId) : '소속 없음'],
        ['현재 목표', npc.goal], ['목적지', position(npc.destination)], ['대상 ID', npc.target === '0' ? '없음' : npc.target],
        ['성력 / 무력 / 지혜', `${number(npc.holyPower)} / ${number(npc.strength)} / ${number(npc.wisdom)}`],
        ['피로 / 허기 / 사기', `${number(npc.fatigue * 100)}% / ${number(npc.hunger * 100)}% / ${number(npc.morale * 100)}%`],
        ['식량 / 약품 / 전리품', `${number(npc.food)} / ${number(npc.medicine)} / ${number(npc.goods)}`],
        ['동일 장소 체류', `${number(npc.stationarySeconds, 1)}초`],
    ])}<p>${escape(npc.lastResult)}</p><h3>최근 판단 #${number(npc.decisions)}</h3><div class="table-scroll"><table><thead><tr><th>후보 행동</th><th>허용</th><th>선택 확률</th><th>가중치</th><th>근거 / 제외 사유</th></tr></thead><tbody>${candidates.map(c => `<tr><td>${escape(c.name)}</td><td>${c.allowed ? '가능' : '제외'}</td><td>${number(c.probability * 100, 1)}%</td><td>${number(c.weight, 1)}</td><td>${escape(c.reason)}</td></tr>`).join('')}</tbody></table></div>`;
}

function renderMap() {
    const canvas = $('map');
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (!data) return;
    const points = [...data.npcs, ...data.monsters].map(item => item.position).filter(p => Number.isFinite(p?.x) && Number.isFinite(p?.y));
    const bounds = data.villages.map(v => v.bounds);
    const minX = Math.min(-12, ...points.map(p => p.x), ...bounds.map(b => b.minX)) - 2;
    const maxX = Math.max(22, ...points.map(p => p.x), ...bounds.map(b => b.maxX)) + 2;
    const minY = Math.min(-8, ...points.map(p => p.y), ...bounds.map(b => b.minY)) - 2;
    const maxY = Math.max(13, ...points.map(p => p.y), ...bounds.map(b => b.maxY)) + 2;
    const scale = Math.min((canvas.width - 60) / (maxX - minX), (canvas.height - 60) / (maxY - minY));
    const point = p => ({ x: 30 + (p.x - minX) * scale, y: 30 + (p.y - minY) * scale });
    ctx.font = '12px "Malgun Gothic", sans-serif';
    for (const village of data.villages) {
        const b = village.bounds;
        const p = point({ x: b.minX, y: b.minY });
        ctx.fillStyle = '#20332d';
        ctx.fillRect(p.x, p.y, (b.maxX - b.minX) * scale, (b.maxY - b.minY) * scale);
        ctx.strokeStyle = '#476152';
        ctx.strokeRect(p.x, p.y, (b.maxX - b.minX) * scale, (b.maxY - b.minY) * scale);
        ctx.fillStyle = '#e6bc73';
        ctx.fillText(village.name, p.x, p.y - 8);
    }
    for (const item of [...data.monsters, ...data.npcs]) {
        if (!Number.isFinite(item.position?.x) || !Number.isFinite(item.position?.y)) continue;
        const p = point(item.position);
        const isNpc = item.role !== undefined;
        ctx.fillStyle = !item.active ? '#69767b' : isNpc ? (item.frenzy ? '#d193ef' : '#7addb7') : '#ef8f85';
        ctx.beginPath();
        ctx.arc(p.x, p.y, isNpc && item.id === selected ? 7 : 4, 0, Math.PI * 2);
        ctx.fill();
        if (isNpc) ctx.fillText(item.name, p.x + 7, p.y - 7);
        if (isNpc && item.id === selected && Number.isFinite(item.destination?.x) && Number.isFinite(item.destination?.y)) {
            const to = point(item.destination);
            ctx.strokeStyle = '#e6bc73';
            ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(to.x, to.y); ctx.stroke();
            ctx.setLineDash([]);
        }
    }
}

function renderSnapshot(value) {
    if (data?.sessionId !== value.sessionId) selected = null;
    data = value;
    const total = data.totals;
    $('totals').innerHTML = metric('생존 NPC', total.aliveNpcs, `등록 ${total.npcs} · 사망 ${total.deadNpcs}`)
        + metric('활성 몬스터', total.activeMonsters, `등록 ${total.monsters} · 사망 ${total.deadMonsters} · 비활성 ${total.inactiveMonsters}`)
        + metric('전투·대피 NPC', total.combatNpcs, '현재 활성 주민 기준')
        + metric('활성 변이 잔재', total.activeEchoes, '활성 몬스터 수에 포함');
    $('villages').innerHTML = data.villages.map(v => `<article class="village-card"><h3>${escape(v.name)} <small class="${v.status === '위험' ? 'error' : 'live'}">${escape(v.status)}</small></h3>${stats([
        ['소속 주민 생존 / 등록', `${v.aliveResidents} / ${v.residents}`], ['현재 체류 NPC', v.presentNpcs],
        ['전투·대피 / 광폭', `${v.combatNpcs} / ${v.frenzyNpcs}`], ['영역 내 활성 몬스터', v.activeMonsters],
        ['평균 체력 / 오염', `${v.averageHealthRatio === null ? '—' : number(v.averageHealthRatio * 100) + '%'} / ${number(v.averageCorruption, 1)}`],
        ['마을 금고 / 주민 금화', `${number(v.treasury)} / ${number(v.residentGold)}`],
        ['시장 식량 / 약품 / 전리품', `${v.food} / ${v.medicine} / ${v.goods}`],
    ])}</article>`).join('') || '<p>등록된 마을이 없습니다.</p>';
    options('village', data.villages.map(v => [v.id, v.name]), '<option value="">전체 위치</option><option value="outside">마을 밖</option>');
    options('role', [...new Set(data.npcs.map(n => n.role))].map(role => [role, role]), '<option value="">전체 직업</option>');
    $('events').innerHTML = [...data.events].reverse().map(event => `<li>${escape(event)}</li>`).join('') || '<li>최근 사건 없음</li>';
    $('monsters').innerHTML = data.monsters.map(m => `<tr><td>${escape(m.name)}<small>${escape(m.id)}</small></td><td>${escape(m.kind)}</td><td>${!m.enabled ? '비활성' : m.alive ? '활성' : '사망'}</td><td>${number(m.health)} / ${number(m.maxHealth)}</td><td>${position(m.position)}</td></tr>`).join('');
    renderResidents();
}

async function poll() {
    if (paused) { setTimeout(poll, 1000); return; }
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 4000);
    try {
        if (location.protocol === 'file:') throw new Error('Start-Monitor.cmd를 실행한 뒤 http://127.0.0.1:8765로 접속하세요.');
        const response = await fetch('/api/snapshot', { cache: 'no-store', signal: controller.signal });
        const message = await response.json();
        if (!response.ok) throw new Error(message.message || 'JSON 수신 실패');
        if (paused) return;
        $('connection').className = message.status;
        $('connection').textContent = { live: '● 실시간 연결', stale: '● 데이터 갱신 중단', waiting: '○ 게임 데이터 대기' }[message.status] || '상태 확인';
        if (message.snapshot) {
            renderSnapshot(message.snapshot);
            $('clock').textContent = `게임 ${number(data.simulationTime, 1)}초 · 메시지 #${data.sequence} · ${number(message.ageSeconds, 1)}초 전`;
            $('source').textContent = `JSON: ${message.source} · 세션 ${data.sessionId} · 시드 ${data.worldSeed}`;
            $('notice').textContent = message.status === 'stale' ? '5초 이상 새 메시지가 없습니다. 게임 종료·일시정지·내보내기 오류를 확인하세요. 아래는 마지막 상태입니다.' : 'JSON 연결 정상 · 현재 등록된 Actor의 상태를 표시하고 있습니다.';
        } else {
            $('notice').textContent = `${message.message} ${data ? '아래는 마지막 수신 상태입니다.' : ''}`;
            $('source').textContent = `탐색 경로: ${(message.candidates || []).join(' | ')}`;
        }
    } catch (error) {
        if (!paused) {
            $('connection').className = 'error';
            $('connection').textContent = '● 연결 오류';
            $('notice').textContent = `${error.message} ${data ? '마지막 수신 데이터를 유지합니다.' : ''}`;
        }
    } finally {
        clearTimeout(timeout);
        setTimeout(poll, 1000);
    }
}

for (const id of ['village', 'role', 'state', 'search']) $(id).addEventListener('input', renderResidents);
$('npcs').addEventListener('click', event => {
    const button = event.target.closest('[data-id]');
    if (button) { selected = button.dataset.id; renderResidents(); }
});
$('pause').addEventListener('click', () => {
    paused = !paused;
    $('pause').textContent = paused ? '화면 갱신 재개' : '화면 갱신 일시정지';
    if (paused) {
        $('connection').className = 'stale';
        $('connection').textContent = 'Ⅱ 화면 갱신 일시정지';
        $('notice').textContent = '화면 갱신만 일시정지했습니다. 게임은 계속 진행됩니다.';
    }
});
poll();
