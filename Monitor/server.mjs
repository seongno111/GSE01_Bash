import http from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(directory, '..');
let port = 8765;
let snapshotPath = null;
for (let i = 2; i < process.argv.length; i++) {
    const argument = process.argv[i];
    if (argument === '--port' && process.argv[i + 1]) port = Number(process.argv[++i]);
    else if (argument === '--snapshot' && process.argv[i + 1]) snapshotPath = path.resolve(process.argv[++i]);
    else throw new Error('사용법: node server.mjs [--port 8765] [--snapshot "경로/NpcAI/monitor.json"]');
}
if (!Number.isInteger(port) || port < 1 || port > 65535) throw new Error('포트 범위: 1~65535');

const candidates = snapshotPath ? [snapshotPath] : [
    'x64/Release', 'x64/Debug', 'Release', 'Debug', 'SimpleGame/x64/Release',
    'SimpleGame/x64/Debug', 'SimpleGame/Release', 'SimpleGame/Debug',
].map(folder => path.join(root, folder, 'NpcAI', 'monitor.json'));

const assets = new Map([
    ['/', ['index.html', 'text/html; charset=utf-8']],
    ['/index.html', ['index.html', 'text/html; charset=utf-8']],
    ['/app.js', ['app.js', 'text/javascript; charset=utf-8']],
    ['/style.css', ['style.css', 'text/css; charset=utf-8']],
]);

function send(response, status, value, type = 'application/json; charset=utf-8') {
    response.writeHead(status, {
        'Content-Type': type,
        'Cache-Control': 'no-store',
        'X-Content-Type-Options': 'nosniff',
        'Content-Security-Policy': "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
    });
    response.end(type.startsWith('application/json') ? JSON.stringify(value) : value);
}

async function snapshot() {
    const files = (await Promise.all(candidates.map(async file => {
        try {
            const info = await stat(file);
            return info.isFile() ? { file, modified: info.mtimeMs, size: info.size } : null;
        } catch { return null; }
    }))).filter(Boolean).sort((a, b) => b.modified - a.modified);
    if (!files.length) return { status: 'waiting', message: '게임의 JSON 수신 대기 중입니다. 새 빌드로 게임을 시작하세요.', snapshot: null, candidates };
    const source = files[0];
    if (source.size > 8 * 1024 * 1024) throw new Error('JSON 크기가 8MB 제한을 초과했습니다.');
    const value = JSON.parse(await readFile(source.file, 'utf8'));
    if (value.schemaVersion !== 1 || value.type !== 'world.snapshot' || !Array.isArray(value.npcs)
        || !Array.isArray(value.villages) || !Array.isArray(value.monsters)
        || !Number.isFinite(value.generatedAtUnixMs) || !value.totals) {
        throw new Error('지원하지 않는 모니터링 메시지 형식입니다.');
    }
    const ageSeconds = Math.max(0, (Date.now() - value.generatedAtUnixMs) / 1000);
    return { status: ageSeconds > 5 ? 'stale' : 'live', ageSeconds, source: source.file, snapshot: value };
}

const server = http.createServer(async (request, response) => {
    try {
        if (request.method !== 'GET') return send(response, 405, { message: '읽기 전용 모니터입니다.' });
        const pathname = new URL(request.url, `http://127.0.0.1:${port}`).pathname;
        if (pathname === '/api/snapshot') return send(response, 200, await snapshot());
        const asset = assets.get(pathname);
        if (!asset) return send(response, 404, { message: '찾을 수 없습니다.' });
        send(response, 200, await readFile(path.join(directory, asset[0])), asset[1]);
    } catch (error) {
        send(response, 503, { status: 'error', message: error.message });
    }
});
server.on('error', error => {
    console.error(`모니터 시작 실패: ${error.message}\n포트 충돌 시 --port 8766을 사용하세요.`);
    process.exitCode = 1;
});
server.listen(port, '127.0.0.1', () => {
    console.log(`NPC 모니터: http://127.0.0.1:${port}\n종료: Ctrl+C`);
    console.log(snapshotPath ? `JSON: ${snapshotPath}` : `자동 탐색: ${candidates.join('\n')}`);
});
process.on('SIGINT', () => server.close());
