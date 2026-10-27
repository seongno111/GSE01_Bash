'use strict';

const $ = id => document.getElementById(id);
const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const number = (value, digits = 0) => Number.isFinite(value) ? value.toLocaleString('ko-KR', { maximumFractionDigits: digits }) : '—';
const position = p => p ? `${number(p.x, 1)}, ${number(p.y, 1)}` : '—';
let data = null;
let selected = null;
let paused = false;
const eventNames = { murder: '살인', kill: '인간 격퇴', death: '사망', rumor: '소문 전달', hunt: '토벌', raid: '약탈', heal: '치료', blessing: '축복', trade: '거래', frenzy: '광폭화' };
Object.assign(eventNames, { monster_ability: '몬스터 특수 행동', monster_heal: '오염 봉합', monster_enrage: '몬스터 광폭화', expedition: '원정 출발', expedition_arrival: '원정 도착', expedition_return: '원정 귀환', expedition_failed: '원정 중단', village_rumor: '마을 소문 교환', outpost: '적 거점 형성', outpost_spawn: '거점 증원', outpost_expansion: '행동 반경 확대', monster_wave: '집단 습격', wave_retreat: '습격 철수', pigeon_departure: '전서구 발송', pigeon_arrival: '전서구 도착', pigeon_rumor: '전서구 소식', migration_departure: '이사 출발', migration_arrival: '이사 완료', migration_failed: '이사 보류' });

function structuredEvents() {
    return (data?.events || []).filter(event => event && typeof event === 'object');
}

function renderEvents() {
    if (!data) return;
    const all = structuredEvents();
    const type = $('event-type').value;
    const location = $('event-village').value;
    const query = $('event-search').value.trim().toLocaleLowerCase();
    const events = all.filter(event => (!type || event.type === type)
        && (!location || (location === 'outside' ? event.villageId === null : event.villageId === location))
        && (!query || `${event.id} ${event.sourceEventId || ''} ${event.actorName} ${event.subjectName} ${event.text}`.toLocaleLowerCase().includes(query)));
    const count = type => data.eventCounts?.find(item => item.type === type)?.count || 0;
    $('event-totals').innerHTML = metric('누적 살인', count('murder'), '불법 공격으로 인간을 살해')
        + metric('누적 소문 전달', count('rumor') + count('village_rumor') + count('pigeon_rumor'), '실제로 정보를 전한 횟수')
        + metric('누적 토벌', count('hunt'), '플레이어·NPC 처치 포함')
        + metric('누적 약탈', count('raid'), '실제 금화 이전이 발생한 횟수');
    $('event-note').textContent = data.schemaVersion < 2 ? '이전 JSON 형식입니다. 게임을 새로 빌드해야 구조화된 사건을 받습니다.'
        : `현재 세션 누적 집계 · 최근 최대 ${data.eventRetention?.capacity || 256}건 보관 · 표시 ${events.length}/${all.length}건 · 보관 시작 ID ${data.eventRetention?.firstId || '없음'} · 마을 소식은 외교적 우호·경계 판단에 반영되며 개인의 공격 원한과 구분됩니다.`;
    $('event-log').innerHTML = [...events].reverse().map(event => {
        const source = all.find(item => item.id === event.sourceEventId);
        const provenance = event.sourceEventId ? (source
            ? `<a href="#event-${escape(source.id)}">원본 사건 #${escape(source.id)}</a>`
            : `원본 사건 #${escape(event.sourceEventId)} · 보관 범위 밖`) : '직접 발생';
        return `<tr id="event-${escape(event.id)}"><td>${number(event.time, 1)}초<small>#${escape(event.id)}</small></td>
            <td class="${event.type === 'murder' ? 'error' : ''}">${escape(eventNames[event.type] || event.type)}</td>
            <td>${escape(event.actorName || '미상')} → ${escape(event.subjectName || '없음')}</td>
            <td>${escape(villageName(event.villageId))}<small>${position(event.position)}</small></td>
            <td class="event-text">${escape(event.text)}<small>${provenance}${event.type !== 'rumor' ? ` · 목격·전달 NPC ${(event.knowers || []).length}명` : ''}</small></td></tr>`;
    }).join('') || '<tr><td colspan="5">조건에 맞는 사건이 없습니다.</td></tr>';
}

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
        <td>${escape(npc.role)}<small>${escape(npc.goal)} · ${escape(npc.alignment?.label || '성향 미수신')}</small></td>
        <td>${escape(villageName(npc.currentVillageId))}<small>${position(npc.position)}</small></td>
        <td>${number(npc.health)} / ${number(npc.maxHealth)}<progress max="${Math.max(1, npc.maxHealth || 1)}" value="${Math.max(0, npc.health || 0)}" aria-label="체력"></progress></td>
        <td>${escape(npc.mode)} · ${escape(npc.action)}<small>${escape(npc.phase)}${npc.mission?.active ? ` · ${escape(npc.mission.name)} (${escape(npc.mission.stage)})` : ''}${npc.distant ? ' · 원거리 관측' : ''}</small></td>
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
        ['성향 / 점수', npc.alignment ? `${npc.alignment.label} / ${number(npc.alignment.score, 1)}` : '이전 데이터'],
        ['돌봄·토벌 / 불법 폭력', npc.alignment ? `${npc.alignment.careEvents} / ${npc.alignment.violentEvents}` : '—'],
        ['현재 목표', npc.goal], ['목적지', position(npc.destination)], ['대상 ID', npc.target === '0' ? '없음' : npc.target],
        ['이사 완료 / 사유', `${number(npc.migration?.completed)}회 · ${npc.migration?.reason || '없음'}`],
        ['마을 임무', npc.mission?.active ? `${npc.mission.name} · ${npc.mission.stage} · ${villageName(npc.mission.villageId)}` : '마을 일상'],
        ['태생 체력 / 무력 / 성력', npc.birth ? `${number(npc.birth.health)} / ${number(npc.birth.strength, 1)} / ${number(npc.birth.holyPower)}` : '—'],
        ['태생 지혜 / 금화', npc.birth ? `${number(npc.birth.wisdom)} / ${number(npc.birth.gold)}` : '—'],
        ['방어 / 이동 속도', `${number(npc.armor)} / ${number(npc.moveSpeed, 2)}`],
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
    const points = [...data.npcs, ...data.monsters, ...(data.outposts || [])].map(item => item.position).filter(p => Number.isFinite(p?.x) && Number.isFinite(p?.y));
    const bounds = data.villages.map(v => v.bounds);
    const minX = Math.min(-12, ...points.map(p => p.x), ...bounds.map(b => b.minX)) - 2;
    const maxX = Math.max(22, ...points.map(p => p.x), ...bounds.map(b => b.maxX)) + 2;
    const minY = Math.min(-8, ...points.map(p => p.y), ...bounds.map(b => b.minY)) - 2;
    const maxY = Math.max(13, ...points.map(p => p.y), ...bounds.map(b => b.maxY)) + 2;
    const scale = Math.min((canvas.width - 60) / (maxX - minX), (canvas.height - 60) / (maxY - minY));
    const point = p => ({ x: 30 + (p.x - minX) * scale, y: 30 + (p.y - minY) * scale });
    ctx.font = '12px "Malgun Gothic", sans-serif';
    for (const route of data.villageRoutes || []) {
        const from = data.villages.find(v => v.id === route.fromVillageId);
        const to = data.villages.find(v => v.id === route.toVillageId);
        if (!from?.center || !to?.center) continue;
        const a = point(from.center), b = point(to.center);
        ctx.strokeStyle = '#47534a'; ctx.lineWidth = 2;
        ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
    }
    ctx.lineWidth = 1;
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
    for (const camp of data.outposts || []) {
        const p = point(camp.position);
        ctx.strokeStyle = '#ba687c';
        ctx.setLineDash([3, 4]);
        ctx.beginPath(); ctx.arc(p.x, p.y, camp.radius * scale, 0, Math.PI * 2); ctx.stroke();
        ctx.setLineDash([]);
        ctx.fillStyle = '#e78996';
        const size = Math.max(5, (camp.halfExtent || 5) * scale);
        ctx.strokeRect(p.x - size, p.y - size, size * 2, size * 2);
        ctx.fillText(`${camp.name} (${camp.alive}/${camp.capacity})`, p.x + 9, p.y - 9);
    }
    for (const item of [...data.monsters, ...data.npcs]) {
        if (!Number.isFinite(item.position?.x) || !Number.isFinite(item.position?.y)) continue;
        const p = point(item.position);
        const isNpc = item.role !== undefined;
        ctx.fillStyle = !item.active ? '#69767b' : isNpc ? (item.frenzy ? '#d193ef' : '#7addb7') : '#ef8f85';
        ctx.beginPath();
        ctx.arc(p.x, p.y, isNpc && item.id === selected ? 6 : 2, 0, Math.PI * 2);
        ctx.fill();
        if (isNpc && (data.npcs.length <= 60 || item.id === selected)) ctx.fillText(item.name, p.x + 7, p.y - 7);
        if (isNpc && item.id === selected && Number.isFinite(item.destination?.x) && Number.isFinite(item.destination?.y)) {
            const to = point(item.destination);
            ctx.strokeStyle = '#e6bc73';
            ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(to.x, to.y); ctx.stroke();
            ctx.setLineDash([]);
        }
    }
    for (const post of data.carrierPosts || []) {
        if (!Number.isFinite(post.position?.x) || !Number.isFinite(post.position?.y)) continue;
        const p = point(post.position);
        ctx.fillStyle = '#f4e6b6';
        ctx.beginPath(); ctx.moveTo(p.x, p.y - 5); ctx.lineTo(p.x - 4, p.y + 3);
        ctx.lineTo(p.x + 4, p.y + 3); ctx.closePath(); ctx.fill();
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
        ['공동체 성향', v.culture || '—'], ['마을 중심', v.center ? position(v.center) : '—'],
        ['소속 주민 생존 / 등록', `${v.aliveResidents} / ${v.residents}`], ['현재 체류 NPC', v.presentNpcs],
        ['전투·대피 / 광폭', `${v.combatNpcs} / ${v.frenzyNpcs}`], ['영역 내 활성 몬스터', v.activeMonsters],
        ['마을 분위기', v.atmosphere || v.status],
        ['선한 / 중립 / 악한 성향', v.alignment ? `${v.alignment.good} / ${v.alignment.neutral} / ${v.alignment.evil}` : '이전 데이터'],
        ['체류 주민 평균 성향', number(v.alignment?.averageScore, 1)],
        ['체류 주민 평균 사기', number(v.averageMorale === null ? null : v.averageMorale * 100) + '%'],
        ['최근 60초 살인 / 약탈', v.recentEvents ? `${v.recentEvents.murder} / ${v.recentEvents.raid}${v.recentEvents.windowComplete ? '' : ' (일부 기록)'}` : '—'],
        ['최근 60초 토벌 / 소문', v.recentEvents ? `${v.recentEvents.hunt} / ${v.recentEvents.rumor}` : '—'],
        ['평균 체력 / 오염', `${v.averageHealthRatio === null ? '—' : number(v.averageHealthRatio * 100) + '%'} / ${number(v.averageCorruption, 1)}`],
        ['마을 금고 / 주민 금화', `${number(v.treasury)} / ${number(v.residentGold)}`],
        ['시장 식량 / 약품 / 전리품', `${v.food} / ${v.medicine} / ${v.goods}`],
        ['원정 파견 횟수', number(v.expeditions)],
        ['전서구 발송 / 수신', `${number(v.postsSent)} / ${number(v.postsReceived)}`],
        ['이사 완료 전입 / 전출', `${number(v.immigrants)} / ${number(v.emigrants)}`],
        ['이사 중 전입 / 전출', `${number(v.migratingIn)} / ${number(v.migratingOut)}`],
        ['보고된 지리 위험도', number(v.geographicRisk, 2)],
        ['주변 마을 소식', (v.knowledge || []).map(k => `${villageName(k.villageId)}: ${k.stance} (${number(k.relation)}) · 성향 ${number(k.alignment)} · ${k.observedAt < 0 ? '위치만 확인' : `${number(k.observedAt, 1)}초 관측, 출처 ${villageName(k.sourceVillageId)}`}${k.threatened ? ' · 위험 보고' : ''}`).join(' / ') || '아직 전달된 정보 없음'],
    ])}</article>`).join('') || '<p>등록된 마을이 없습니다.</p>';
    $('outposts').innerHTML = (data.outposts || []).map(c => `<article class="village-card"><h3>${escape(c.name)}</h3>${stats([
        ['위치', position(c.position)], ['생존 / 정원', `${c.alive} / ${c.capacity}`],
        ['주둔 / 출격 임계치', `${c.garrison} / ${c.threshold}`], ['습격 중', c.raiding],
        ['거점 너비 / 출격 규모', `${number((c.halfExtent || 5) * 2)}칸 / ${number(c.waveSize || 6)}마리`],
        ['순찰 반경', c.radius], ['누적 웨이브 / 증원', `${c.waves} / ${c.spawned}`],
        ['습격 대상', c.targetVillageId ? villageName(c.targetVillageId) : '없음'],
        ['다음 증원까지', `${number(Math.max(0, c.nextSpawn - data.simulationTime), 1)}초`],
    ])}</article>`).join('') || '<p>적 거점 데이터 수신 대기</p>';
    $('carrier-posts').innerHTML = (data.carrierPosts || []).map(p => `<article class="village-card"><h3>전서구 #${escape(p.id)}</h3>${stats([
        ['출발 → 도착', `${villageName(p.sourceVillageId)} → ${villageName(p.destinationVillageId)}`],
        ['도착까지', `${number(Math.max(0, p.arrivesAt - data.simulationTime), 1)}초`],
        ['속도 / 실은 마을 보고', `${number(p.speed)}칸/초 · ${number(p.reports)}건`],
        ['현재 위치', position(p.position)],
    ])}</article>`).join('') || '<p>현재 비행 중인 전서구가 없습니다.</p>';
    $('travel-rules').textContent = data.settlementRules ? `마을당 초기 ${data.settlementRules.initialPopulationPerVillage}명 · 이웃 마을 경계 사이 ${data.settlementRules.roadGap}칸 / 일반 걷기 약 30초 · 이사 중에도 도착 전까지 기존 소속을 유지합니다.` : '새 월드 규모 정보 수신 대기';
    options('event-type', Object.entries(eventNames), '<option value="">모든 사건</option>');
    options('village', data.villages.map(v => [v.id, v.name]), '<option value="">전체 위치</option><option value="outside">마을 밖</option>');
    options('role', [...new Set(data.npcs.map(n => n.role))].map(role => [role, role]), '<option value="">전체 직업</option>');
    options('event-village', data.villages.map(v => [v.id, v.name]), '<option value="">모든 장소</option><option value="outside">마을 밖</option>');
    const recent = data.schemaVersion >= 2 ? structuredEvents().slice(-5).map(event => `${number(event.time, 1)}초 · ${eventNames[event.type] || event.type} · ${event.text}`) : data.events;
    $('events').innerHTML = [...recent].reverse().map(event => `<li>${escape(event)}</li>`).join('') || '<li>최근 사건 없음</li>';
    $('monsters').innerHTML = data.monsters.map(m => `<tr><td>${escape(m.name)}<small>${escape(m.id)}</small></td><td>${escape(m.family || m.kind)}<small>${escape(m.state || '')}${m.enraged ? ' · 광폭화' : ''}</small></td><td>${!m.enabled ? '비활성' : m.alive ? '활성' : '사망'}</td><td>${number(m.health)} / ${number(m.maxHealth)}</td><td>${position(m.position)}</td></tr>`).join('');
    renderResidents();
    renderEvents();
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
for (const id of ['event-type', 'event-village', 'event-search']) $(id).addEventListener('input', renderEvents);
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
