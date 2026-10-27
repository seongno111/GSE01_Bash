# 성향별 마을 건축 6세트

**최신 수정: 2026-10-27 — 마을별 메인 건물 6종을 랜드마크로 재제작했다.** 쌍탑 대성당, 태양 첨탑 수도원, 지휘 요새, 시계탑 상인회관, 천구 관측 대서고, 송곳니 두목 전당이다. 일반 주택과 나란히 놓아도 높이·실루엣·입구·상징으로 구별된다. [랜드마크 상세·전달 안내](LANDMARKS_20261027.md), [6종 비교](previews/landmarks_overview.jpg), [기존 게임 크기 기준 비교](previews/landmarks_scale_comparison.jpg).

2026-10-26에 시작한 현재 `Villages.h`의 6개 ID·문화에 대응하는 3D 로우폴리 건물과 배치 예시다. 각 마을에 주택 A/B, 메인 건물(CHAPEL), 작업장, 성문, 담장 6개씩 **총 36개 메시**다. 10월 27일에는 기존 CHAPEL 6개의 실제 메시를 교체했으며 나머지 30개 모델은 유지했다. 기존 선택 이름·마을 ID·game prop kind 5와 스키마 1을 유지한다.

| village_id | 마을·성향 | 건축 형태와 분위기 |
|---|---|---|
| cathedral | 성당 마을 — 자비·상호부조 | 낮고 넓은 맞배지붕, 목재 보강, 공동 현관과 열린 마당 |
| dawn | 새벽 수도촌 — 신앙·정화 | 높은 급경사 지붕, 좁은 첨두창, 밝게 바랜 석재와 수도원식 축 |
| iron | 철벽 주둔촌 — 규율·무력 | 두꺼운 석벽, 평지붕·여장, 좁은 사격창, 직선 성벽과 병영 |
| market | 황금 장터 — 교역·부 | 우진각 지붕, 돌출 상층, 차양·간판·황동, 안쪽을 향한 상점 |
| seekers | 잿빛 탐구촌 — 탐험·깨달음 | 팔각 탑·원추 지붕, 부속실, 관측 장식과 비대칭 배치 |
| outlaws | 검은 이빨 소굴 — 약탈·힘 | 비스듬한 외쪽지붕, 덧댄 철판, 거친 기둥·뾰족 말뚝과 붉은 천 |

전체는 쇠락한 중세 다크 판타지의 탁한 색과 석재·목재·철을 공유한다. 색만 바꾼 모델이 아니라 지붕·몸체·담장·문·탑·상층·차양의 실제 형상을 바꿨다. 지붕을 생략한 폐허가 반복되지 않도록 맞배·우진각·외쪽지붕을 닫힌 입체로 검사했다. 건물은 외관용으로 문과 창도 벽 위의 디테일이며 실내는 없다.

## 파일

- `villages_v1.blend`: 36개 원본과 메시 데이터를 공유하는 마을 배치 전시
- `village_buildings.glb`: 36개 공용 건물 라이브러리
- `cathedral.glb` 등 6개 `<village_id>.glb`: 해당 성향의 6개 모델만 담은 라이브러리
- `village_catalog.json`: 모델 이름·마을 ID·게임 프롭 종류·치수·배치 예시
- `validation.json`: 이름, 지붕을 포함한 새 구성 입체의 폐쇄, 배치 참조, GLB 형상·법선·기존 30개 모델 유지 검사
- `previews/<village_id>.png`, `themes_overview.jpg`: 성향별 배치 비교
- `previews/<village_id>_landmark.png`, `landmarks_overview.jpg`: 새 메인 건물 상세
- `previews/<village_id>_scale_comparison.png`, `landmarks_scale_comparison.jpg`: 현재 로더의 크기 계산을 적용한 주택과 랜드마크 비교
- `revisions/20261027_before_landmarks/`: 교체 전 원본·GLB·카탈로그·미리보기 백업

모델 이름은 `VIL_<VILLAGE_ID>_<TYPE>`이며 타입은 HOUSE_A, HOUSE_B, CHAPEL, WORKSHOP, GATE, WALL이다. 모든 라이브러리 메시가 **바닥 중앙 원점에 겹쳐** 있으므로 이름으로 골라 배치한다. 오브젝트 전체를 한 건물처럼 그리면 안 된다. glTF는 미터·Y 위·정면 +Z이고 Blender는 Z 위·정면 -Y다. 배치 JSON에 양쪽 좌표를 제공한다.

## 게임 연결용 대응

| 기존 game prop kind | 선택할 모델 |
|---:|---|
| 2 폐벽/담장 | 해당 테마 WALL |
| 5 성당 | 해당 테마 CHAPEL |
| 7 주택 | 해당 테마 HOUSE_A 또는 HOUSE_B |
| 8 작업장 | 해당 테마 WORKSHOP |

GATE는 새 배치용 모델이며 기존 프롭 숫자를 임의로 재사용하지 않았다. 모델 선택 키는 village_id + game_kind + 주택 variant로 구성할 수 있다. 규칙은 카탈로그 `game_kind_choices`에 있다. 외부 이미지 없이 OPAQUE PBR 재질을 사용한다.

이 팩은 현재 마을의 **고유 문화**를 기준으로 한다. `NpcSociety`의 실시간 `alignment` 수치에 따라 모델이 자동으로 바뀌는 기능은 구현하지 않았다. 배치 예시는 미술 검토용이며 기존 지형·도로·NPC 집 위치·충돌체를 바꾸지 않는다. 실행 시 모델 교체와 배치 변경은 별도 적용 작업이다.

`VILLAGE_BUILDING_LIBRARY`는 숨긴 공용 원본이고 `VILLAGE_LAYOUT_PREVIEWS`는 링크된 배치 예시다. `LANDMARK_DETAIL_PREVIEWS`에는 6종 상세 전시, `LANDMARK_SCALE_COMPARISONS`에는 게임 크기 기준 주택 비교가 있다. `PRESENTATION`은 렌더용이다. 새 랜드마크는 건물당 하나의 메시이며 내부 구성 선택을 위한 이름 있는 버텍스 그룹이 있다. 건물별 인테리어·움직이는 문·LOD·콜리전·파괴 애니메이션은 포함하지 않았다.

6개 마을의 실제 Blender 렌더로 형태를 비교했고, 36개 모델 이름과 모든 배치 참조, 생성한 다각형 지붕 입체의 경계 폐쇄를 검사했다. 게임 코드·빌드·실제 맵 변경은 하지 않았다.
