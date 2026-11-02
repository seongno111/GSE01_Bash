# 공통 걷기·달리기와 게임 회전

2026-11-02 추가 개정: 이 문서의 공통 Walk/Run·게임 회전 정책은 유지한다. 사족 동물 3종의 관절과 보행은 [AnatomyRevision_20261102](../AnatomyRevision_20261102/README.md)에서 갱신했다. 본 수·스킨·현재 파일 해시는 새 개정 안내를 기준으로 한다. 아래 데이터 동일성 검사는 10월 29일 정리 당시의 비교 기록이다.

2026-10-29 최종 결정에 맞춘 현재 전달본이다. **각 캐릭터 리그에 Walk 1개·Run 1개를 공통 이동 클립으로 유지**했다. 앞·뒤·좌·우 방향은 동일한 클립을 재생하면서 게임의 Actor/모델 전체를 회전해 표현한다. 기본 대기는 기존 Idle을 사용한다.

현재 Hero_v1, LowPolyKit_v3, Monsters_v1의 원본 3개·GLB 18개·카탈로그를 직접 정리했다. 화면별 이동·대기 복제와 TurnStep/TurnScreen, 기존 Turn_L45/R45/L90/R90/L180/R180을 모두 제거했다. 고유 클립 286개를 제거하고 참조하지 않는 GLB 버퍼도 정리해 18개 GLB에서 약 35.2MB를 줄였다.

모델 메시·본·스킨·바인드·장비·파트 인덱스와 남긴 모든 애니메이션 데이터는 정리 전과 같다. 공격·생활·피격·사망·시전·돌진·도약 등 개별 행동은 유지했다. Flee·Strafe·WalkBack·Backstep 등 기존의 특수 전술/연출 동작도 호환을 위해 유지하며, 기본 이동 선택은 Walk/Run 두 클립을 사용한다.

## 다른 채팅에서 연결할 이름

| 대상 / 접두사 | 공통 걷기 | 공통 달리기 | 현재 전체 클립 수 |
|---|---|---|---:|
| 주인공 | HERO_Walk | HERO_Run | 11 |
| 공용 NPC·직군별 5개 예시 | NPC_Walk | NPC_Run | 각 39 |
| WitheredHollow | MON_00_Walk | MON_00_Run | 9 |
| TaintedRaider | MON_01_Walk | MON_01_Run | 9 |
| FieldWarden | MON_02_Walk | MON_02_Run | 11 |
| MutatedHuman | MON_03_Walk | MON_03_Run | 10 |
| DeathEcho | MON_04_Walk | MON_04_Run | 10 |
| AshWolf | AshWolf_Walk | AshWolf_Run | 14 |
| BlightBear | BlightBear_Walk | BlightBear_Run | 13 |
| ThornStag | ThornStag_Walk | ThornStag_Run | 13 |
| HighDemonAzar | HighDemonAzar_Walk | HighDemonAzar_Run | 17 |
| FallenPaladin | FallenPaladin_Walk | FallenPaladin_Run | 14 |
| FallenPriest | FallenPriest_Walk | FallenPriest_Run | 16 |

캐릭터마다 체형·장비에 맞는 보행은 유지하므로 서로 다른 체형의 클립을 하나의 스킨으로 합치지 않는다. 공용 NPC 31본은 모든 직군·성별·머리/장비 조합에서 같은 NPC_Walk/NPC_Run을 사용한다. 몬스터 종류 숫자와 이름 대응도 바꾸지 않았다.

## 회전 적용 규약

- GLB는 Y 위·정면 +Z, Blender는 Z 위·정면 -Y, 미터 단위다.
- Walk/Run은 방향 회전이 없는 제자리 보행이다. 캐릭터 위치와 바라보는 방향은 게임 로직에서 결정한다.
- 원하는 이동 방향으로 Actor/모델 전체를 회전한다. 애니메이션 root yaw를 따로 더하지 않는다.
- 걷기·달리기 사이에는 보행 위상을 유지하고 속도에 맞게 재생률을 조절한다. 부드러운 회전·블렌드와 입력 연결은 다른 채팅에서 구현한다.
- 좌우를 음수 스케일로 뒤집지 않고 실제 회전을 사용하면 주인공의 왼손 램프·오른손 검과 법선이 유지된다.
- 이전 ScreenUp/Down·Turn* 이름은 현재 GLB에 없으므로 선택하지 않는다. 현재 애니메이션 카탈로그와 이 폴더의 이름표를 기준으로 한다.

게임 코드·렌더러·회전 로직·빌드는 이 작업에서 수정하지 않았다. 실행 폴더에 복사된 이전 모델과 캐시는 적용 작업에서 최신 파일로 교체한다.

## 전달 및 검수

- 원본: `../Hero_v1/hero_v1.blend`, `../LowPolyKit_v3/lowpoly_kit_v3.blend`, `../Monsters_v1/monsters_v1.blend`
- 모델: 기존 Hero GLB, v3 NPC 6개·몬스터 5개 GLB, 신규 몬스터 6개 GLB
- `locomotion_catalog.json`: 13개 리그의 Idle/Walk/Run 이름과 18개 파일 대응
- `validation.json`: 정리·모델 유지·클립 데이터 유지·재가져오기 검사
- `removed_clips.json`: 제거한 이름과 파일별 개수
- `delivery_files.json`: 현재 전달 파일 SHA256
- `previews/hero_walk_run.gif`: 공통 Walk/Run의 실제 GLB 재생 비교
- `backups_before_cleanup/`: 방향별 클립을 포함했던 정리 직전 상태

GLB는 애니메이션 이름만 숨긴 것이 아니라 사용하지 않는 채널·접근자·버퍼 참조를 정리해 저장했다. 모든 정적 노드·메시·재질·스킨·역바인드와 남긴 애니메이션 채널의 원시 데이터가 이전 GLB와 일치하는지 비교했다. 원본에서도 관련 Action과 NLA 트랙을 제거했다. 정리된 GLB를 새 Blender 장면에 불러와 모델 수·클립 수·공통 보행의 중립 root 회전을 확인한다.

`../DirectionalMotion_20261029/`는 폐기한 방향별 제작 이력이며 현재 적용 대상이 아니다.
