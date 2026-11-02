# 신규 몬스터 6종 — 모델·리깅·키프레임

2026-10-26. `MonsterDefinitions.h/.cpp`, `MonsterBehavior.cpp`, 몬스터 제작 인계 문서를 기준으로 만든 별도 모델 팩이다. 기존 게임 코드와 v3 몬스터 인덱스는 변경하지 않았다.

| MonsterKind | speciesId | modelKey / GLB | 특징 |
|---:|---|---|---|
| 3 | ash_wolf | `AshWolf.glb` | 마른 사족, 재가 낀 갈기·주둥이, 송곳니, 긴 꼬리 |
| 4 | blight_bear | `BlightBear.glb` | 넓은 어깨, 굵은 앞발, 별도 메시의 등 균낭 |
| 5 | thorn_stag | `ThornStag.glb` | 긴 다리·목, 가지 뿔과 철사가 얽힌 가시관 |
| 6 | high_demon | `HighDemonAzar.glb` | 역삼각 체형, 부러진 왕관형 뿔, 접힌 날개, 가슴 불씨 |
| 7 | fallen_paladin | `FallenPaladin.glb` | 실제 머리가 있는 판금 투구, 큰 방패, 검, 해진 제복 |
| 8 | fallen_priest | `FallenPriest.glb` | 긴 제의, 얼굴 가면, 지팡이와 봉합용 도구 |

**MonsterKind 3·4는 기존 ModelLibrary::Monster(3·4)의 변이체·잔재와 다르다.** `modelKey` 또는 `speciesId`로 이 폴더의 개별 GLB를 연결한다. 현재 로더에 이 팩을 자동으로 등록하는 변경은 하지 않았다.

## 전달 파일

- `monsters_v1.blend`: 모델·본·웨이트·Action·NLA 원본과 전시 복제
- `<modelKey>.glb`: 모델별 독립 스킨과 애니메이션
- `monster_catalog.json`: 본 이름, 메시, 치수, 클립·이벤트·AI 상태 대응
- `validation.json`: 웨이트, 루프, 실제 GLB와 타이밍 검사
- `previews/lineup.png`, 모델별 PNG·GIF: 실제 모델 렌더와 특수 행동

외부 이미지 텍스처, 모프, Draco/meshopt 압축에 의존하지 않는 OPAQUE 단색 PBR 모델이다. 변형용 본과 키프레임을 파일 안에 넣었다. 미터, Blender Z 위·정면 -Y, glTF Y 위·정면 +Z, 발바닥 중앙 원점이다. 별도 height_scale을 다시 곱하지 않는다.

## 리그

동물 3종은 인간 골격을 재사용하지 않는 **사족 전용 리그**다. 각 앞/뒷다리에 upper·lower·paw 본, 중심·목·머리·턱·꼬리와 부착점을 둔다. 곰은 균낭, 사슴은 뿔 본도 갖는다. 걷기·달리기의 발 목표를 기준으로 관절 키를 만들고, 웅크리기와 특수 행동에서 모델이 바닥 아래로 들어가는 부분을 보정했다.

성기사·사제는 기존 NPC 31개 본 이름과 호환되며 체격별 바인드와 역바인드를 가진다. 아자르는 날개 4개 본과 시전 부착점을 더한 확장 리그다. 몸통·턱·다리 등에는 이름 있는 정점 그룹을 사용한다. 원본에서 단단한 부분은 주로 단일 본, 동물 몸통은 hips/chest 가중치로 연결했다. 날개·균낭·뿔·장비는 별도 메시다.

## 애니메이션 123개

클립 이름은 `<modelKey>_<동작>`이다. 모델별 카탈로그에 19~23개씩 있다. 본 위치·회전·스케일의 60fps 타임라인 키프레임이며, GLB에도 샘플링한 채널을 담았다.

공통 동작: Idle, Walk, Run, 좌우 45·90·180도 Turn, Hit, Death, DeadHold, Spawn.

| 종류 | 추가 동작 |
|---|---|
| 늑대 | Alert, Bite, PouncePrepare/Travel/Land, FlankL/R |
| 곰 | Swipe, Slam, SlamPrepare/Impact/Recover, Enrage |
| 사슴 | Alert, Flee, Gore, ChargePrepare/Travel/Stop |
| 아자르 | Volley 및 준비/발사/회복, Rift 및 준비/발동/회복, Enrage, Roar |
| 성기사 | Slash 및 준비/타격/회복, WardPrepare/Hold/End |
| 사제 | Backstep, Curse 및 준비/발사/회복, Mend 및 준비/발동/회복 |

전체 동작 클립과 단계별 클립을 모두 제공하는 경우 **둘 중 한 방식만 선택**한다. 단계별 메타데이터를 중복 재생하거나 이벤트를 두 번 적용하면 안 된다.

| 종류 | 기본 준비 / 회복 | 특수 준비 | 특수 발동 후 |
|---|---|---|---|
| 늑대 | 0.35 / 0.75초 | 0.45초 | 제자리 도약 Travel, 착지/회복 0.75초 |
| 곰 | 0.80 / 1.40초 | 0.95초 | 내려찍기 접촉 후 회복 1.40초 |
| 사슴 | 0.55 / 1.10초 | 0.90초 | 제자리 돌진 Travel, 정지/회복 1.10초 |
| 아자르 | 1.10 / 1.60초 | 1.40초 | 균열 발동 후 회복 1.60초 |
| 성기사 | 0.70 / 1.15초 | 0.60초 | 방벽 유지 2.40초, 시각 해제 0.35초 |
| 사제 | 0.85 / 1.25초 | 1.20초 | 봉합 발동 후 회복 0.50초 |

원본 행동 코드가 실제 피해·탄환·회복을 처리한다. 모델의 이벤트는 시각 동기화 알림이며 게임 판정을 새로 호출하지 않는다. 성기사의 0.25초 특수 후 recovery와 2.4초 guardRemaining은 별도 타이머이므로 방벽 Hold를 전체 Recover로 오해하지 않는다.

걷기·달리기·도약 Travel·돌진 Travel은 **제자리**이며 실제 월드 이동은 AI가 수행한다. 이동 속도에 맞춘 재생률/발 위상 연결은 적용 작업에서 처리한다. Turn은 끝에 root 회전 델타를 남기는 단발이므로 actor yaw와 이중 적용하지 않는다. 도약의 실제 충돌·거리·종료는 현 시뮬레이션을 따른다.

아자르의 접힌 날개는 지상 시전 실루엣과 작은 움직임용이다. 비행·소환·다단계 AI는 추가하지 않았다. 광역 지면 이펙트, 탄환, 분노 색 전환, 체력바 위치와 충돌 반경 조정은 모델 제작에 포함하지 않았다.

## 원본과 검수

`NEW_MONSTER_LIBRARY`는 원점의 원본, `MONSTER_PREVIEWS`는 같은 메시 데이터를 공유하는 배치 예시다. `SOURCE_RIG`는 기존 동작 참고용 숨긴 리그다. DISPLAY/PRESENTATION은 게임 에셋이 아니다. Action Editor에서 동작을 선택하거나 같은 이름의 NLA 트랙 하나만 활성화해 확인한다.

모든 정점 웨이트와 합계, GLB 스킨·클립 이름, 루프 시작/끝, 유한 키 값, 기본·특수 발동 시점을 검사했다. 모델 렌더와 특수 행동 GIF로 확인하고, 최종 GLB 재가져오기 검사는 validation.json에 기록한다. 모든 임의 포즈·장비 조합·실시간 충돌을 검증했다는 뜻은 아니다. 게임 소스 수정·빌드·실제 전투 실행은 하지 않았다.
