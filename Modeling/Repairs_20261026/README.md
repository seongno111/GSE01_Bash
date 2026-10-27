# 2026-10-26 모델링 수정·추가 전달

게임 소스·빌드·렌더러·월드 배치 코드는 수정하지 않았다. 아래는 다른 적용 작업에서 사용할 실제 모델링 에셋과 메타데이터다.

## 기존 파일 수정

### 주인공 망토

`Hero_v1/hero_v1.blend`, `hero_rigged.glb`, `hero_catalog.json`, `validation.json`과 해당 미리보기를 갱신했다. 짧은 등 망토를 폭 0.94m, 종아리까지 내려오는 주름·두께가 있는 망토로 교체했다. **메시명 `HERO_CAPE_00_WornMantle`, part_id, 33개 본과 기존 17개 클립 이름을 유지**한다. 기존 제의 본을 활용해 이동 중에도 변형된다.

### 마을 성당 지붕

실제 game kind 5는 기존 카탈로그에서 prop_index 19 `PROP_019_RuinedChapel`로 연결된다. 지붕 일부를 생략했던 원본을 닫힌 입체로 교체했다. `LowPolyKit_v3/lowpoly_kit_v3.blend`, `props/props_library.glb`, `asset_catalog.json`, `validation.json`을 갱신했다. 지붕 조각의 모든 모서리가 두 면을 공유하는지 검사했으며, 전면/상부 렌더는 이 폴더의 `previews/chapel_roof_*.png`다.

기존 메시 이름·인덱스·다른 NPC/몬스터 파일은 변경하지 않았다. 수정 전 주요 파일은 `backups/`에 보존했다. 이미 실행 중인 게임의 에셋 캐시는 자동 갱신하지 않았으며, 실행 파일 옆 복사본에 파일을 배포하거나 재시작하는 작업도 수행하지 않았다.

## 신규 몬스터 팩

`Modeling/Monsters_v1`에 신규 6종의 독립 GLB·원본·카탈로그·검수·미리보기를 제공한다. `modelKey`는 현재 MonsterDefinitions.cpp와 같다.

| MonsterKind | modelKey | GLB |
|---:|---|---|
| 3 | AshWolf | `Monsters_v1/AshWolf.glb` |
| 4 | BlightBear | `Monsters_v1/BlightBear.glb` |
| 5 | ThornStag | `Monsters_v1/ThornStag.glb` |
| 6 | HighDemonAzar | `Monsters_v1/HighDemonAzar.glb` |
| 7 | FallenPaladin | `Monsters_v1/FallenPaladin.glb` |
| 8 | FallenPriest | `Monsters_v1/FallenPriest.glb` |

기존 v3 Monster(3)=MutatedHuman, Monster(4)=DeathEcho와 충돌하므로 새 MonsterKind를 기존 숫자 API에 그대로 전달하지 않는다. 현재 Load(path)로 파일을 읽는 것과 speciesId/modelKey에 대응해 월드에서 표시하는 것은 별도 구현이다. 이벤트는 현재 AI가 수행하는 피해·발사·회복을 중복 호출하지 않도록 시각용으로만 사용한다.

## 마을별 건물 팩

`Modeling/Villages_v1`에 6개 문화 테마·36개 건물을 제공한다. `village_catalog.json`에 현재 village_id와 game prop kind의 후보 모델, 주택 2형, 추가 성문, 배치 예시를 기록했다. 새 마을 배치는 미술 예시이고 현재 World.cpp의 도로·건물·NPC 위치를 덮어쓰지 않는다. 동적인 society.alignment에 따른 자동 외형 변환도 추가하지 않았다.

모든 새 파일은 단색 OPAQUE PBR이며 외부 이미지·모프·메시 압축에 의존하지 않는다. 치수·스킨·클립 정보는 각 팩의 카탈로그와 검증 JSON을 기준으로 한다. 설치·빌드·게임 플레이 실행은 적용 작업에서 진행한다.
