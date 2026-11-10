# 새 무기용 주인공 키프레임 애니메이션

2026-11-05. 기존 후드·견갑·망토 주인공과 `HeroWeapons_v1`의 무기 7종에 맞춘 **새 동작 30개**다. 기존 33개 본, 바인드, 주인공 메시·웨이트와 기존 11개 동작을 유지한 별도 전달본이다. 원래 `Hero_v1` 및 `HeroWeapons_v1` 파일은 변경하지 않았다.

## 무기별 동작

| 무기 | 클립 접두사 | 동작 | 개수 |
|---|---|---|---:|
| 지팡이 | `HERO_Staff_` | Ready, CastBolt, CastSweep, Channel | 4 |
| 활 | `HERO_Bow_` | Ready, Nock, Draw, Aim, Release, Lower | 6 |
| 철퇴 | `HERO_Mace_` | Ready, Strike, Sweep, Guard | 4 |
| 도끼 | `HERO_Axe_` | Ready, Chop, Cleave, Guard | 4 |
| 전투 망치 | `HERO_Hammer_` | Ready, Smash, Uppercut, Guard | 4 |
| 창 | `HERO_Spear_` | Ready, Thrust, Sweep, Guard | 4 |
| 단검 | `HERO_Dagger_` | Ready, Stab, Slash, Guard | 4 |

공격은 준비 → 타격 → 후속 움직임 → 준비 자세 복귀로 구성했다. 철퇴·도끼는 무게를 실은 호와 내려치기, 망치는 긴 준비와 느린 회수, 창은 전방 찌르기와 긴 횡베기, 단검은 짧고 빠른 동작을 사용한다. 지팡이는 전방 시전·넓은 시전과 집중 루프를 제공한다. Ready·Guard·Channel·Aim은 처음과 끝이 같은 루프다.

활은 **Ready → Nock → Draw → Aim → Release → Ready**, 발사 취소는 **Aim → Lower → Ready**로 연결한다. Nock에서 화살을 시위에 대고, Draw에서 왼팔을 펼치며 오른손으로 당긴다. Release는 시위를 놓고 작게 반동한 뒤 활을 내린다. 화살의 실제 비행 모델 생성은 후속 게임 로직에서 담당한다.

모든 새 클립은 **60fps의 위치·쿼터니언 회전·스케일 키프레임**으로 구웠다. 편집 시 사용한 관절 해법이 실행 환경에 필요하지 않으며, 런타임 IK·드라이버·물리 의존성이 없다. 발은 바닥에 고정하고 root의 이동과 방향 회전은 0으로 유지했다. 몸통 비틀기와 자세 낮추기는 동작에 포함한다.

Blender의 시작 프레임 1은 GLB의 시간 0초에 대응한다. 카탈로그의 이벤트 초는 클립 시작 기준이며, 프레임은 `1 + 초 × 60`이다. GIF는 용량을 줄인 12fps 미리보기이고 전달 애니메이션 데이터는 60fps다.

## 전달 파일

| 파일 | 내용 |
|---|---|
| `hero_weapon_animations_v1.blend` | 원본 주인공·무기·새 Action·시위/화살 shape key |
| `hero_weapon_animation_library.glb` | 기존 주인공 모델과 **기존 11 + 새 30 = 41개** 본 애니메이션 |
| `hero_staff_actions.glb` 등 무기별 GLB 7개 | 해당 무기를 실제 장착한 주인공과 그 무기의 4개/6개 새 동작 |
| `animation_catalog.json` | 무기 키 대응·클립 이름·길이·루프·이벤트·장비 표시·마스크 정보 |
| `authoring_keyposes.json` | 동작별 주요 포즈와 구간별 시간 |
| `validation.json`, `reimport_validation.json` | 실제 GLB 데이터 및 재임포트 검수 |
| `previews/animations_overview.jpg` | 무기 7종 대표 공격 비교 |
| `previews/hero_bow.gif` 등 | 실제 무기별 GLB 재생 미리보기 |
| `previews/all_clips_contact.jpg` | 새 30개 동작의 대표 프레임 |
| `scripts/` | 재제작·내보내기·검수용 도구. 게임 소스가 아님 |

전체 라이브러리는 기본 검·램프 메시를 그대로 보존하므로 **새 무기의 외형을 검수할 때는 무기별 GLB를 사용**한다. 무기별 GLB에는 오른손 기본 검이 빠져 있고, 활·망치·창에는 왼손 램프도 빠져 있다. 별도 무기를 중복으로 붙이지 않는다. 새 무기는 이 팩 안에서 원래 손 본에 강체 웨이트로 연결되어 있다.

무기별 GLB의 원래 주인공 파트 이름·본 이름과 새 무기의 `weapon_key`, `__Body`, `__String` 이름을 유지했다. 원래 정적 무기를 별도로 부착하는 방식으로 구현한다면 `HeroWeapons_v1/weapon_catalog.json`의 손 본 장착 회전을 사용한다. 이 팩의 스킨에 포함된 무기에는 그 장착 회전을 다시 적용하지 않는다.

## 이동과 장비 조립 인계

**걷기는 `HERO_Walk` 하나, 달리기는 `HERO_Run` 하나**다. 새 무기별 이동·화면 방향별 이동·방향 전환 클립을 추가하지 않았다. 방향은 다른 작업에서 Actor 회전으로 처리한다. 공통 이동은 전체 라이브러리에만 한 번 포함했다.

이동 중 무기 자세가 필요할 때 공통 Walk/Run의 하체와 해당 무기 Ready의 상체를 조합하도록 `upper_body_mask`를 제공한다. 특히 양손 무기는 기존 검·램프용 전체 몸 이동을 그대로 덮으면 양손 파지가 풀리므로 이 마스크가 필요하다. 레이어 블렌딩 실행 코드는 포함하지 않았다.

- 지팡이·철퇴·도끼·단검: 오른손 무기, 왼손 램프 유지.
- 활: 왼손 활, 오른손 화살/시위. 기존 검·램프 메시와 램프 광원을 숨긴다.
- 망치·창: 오른손 주 파지 + 왼손 보조 파지. 기존 검·램프 메시와 램프 광원을 숨긴다.
- 골격·클립을 옮길 때 본의 **이름**으로 대응한다. 파일의 노드 인덱스가 다른 파일과 같다고 가정하지 않는다.

`animation_catalog.json`의 무기별 `hide_original_meshes`, `hide_lamp_light`, `clip_names`와 `weapon_key`가 적용 기준이다. 게임 enum이나 NPC 슬롯 번호를 새로 할당하지 않았다.

## 시위·활대·화살

활의 `__Body`와 `__String`에 `Draw` shape key가 있다. 중립 0에서 완전 당김 1까지 활대 끝이 휘고 시위 중앙이 약 0.29m 추가로 당겨진다. 오른손은 시위 중앙과 같은 위치로 움직인다. 활 전용 GLB의 6개 클립에 본과 동기화한 morph weight 채널을 포함했다. **본 동작만 담긴 전체 라이브러리에는 이 무기 메시 채널이 없으므로**, 활은 전용 GLB의 동일 이름 클립을 함께 사용해야 한다.

화살 메시의 `Hide` shape key는 0이면 실제 화살, 1이면 nock 위치로 축소되어 보이지 않는다. 보이는 시점과 발사 시점을 STEP 키로 구웠으며 `arrow_show`, `arrow_hide`, `projectile_release` 이벤트도 카탈로그에 기록했다. 엔진이 morph를 지원하지 않는 경우 이벤트로 화살 표시와 시위 변형을 연결하는 처리가 필요하다.

발사는 `HERO_Bow_Release` 시작 후 **0.10초**다. 게임에서 이때 탄체를 생성한다. 타격·시전도 클립별 `melee_contact`, `cast_release`와 초·프레임 정보를 제공한다. 이벤트 메타데이터는 `animation_catalog.json`에 있으며 GLB가 게임의 피해 판정이나 이펙트를 자동으로 실행하지는 않는다.

## 검수

8개 GLB를 읽어 실제 클립 수·시간·유한 채널·루프·root·장비 파트와 morph를 검사했다. 원래 Hero GLB와 본 계층·역바인드 행렬·메시 정점/노멀/웨이트를 비교하고, 기존 11개 동작의 변환도 샘플링해 유지 여부를 확인했다. 새 30개 클립은 실제 내보낸 GLB를 Blender에 재임포트하여 60fps 전 프레임의 발 바닥·발 고정·양손 파지와 루프를 검사했다. 실제 파일로 공격 GIF와 대표 포즈를 렌더링했다.

게임 코드 수정·빌드·장비 전환·애니메이션 상태 머신·레이어 블렌딩·피해·탄체·마법 이펙트는 이번 전달 범위에 포함하지 않았다. 모델과 동작의 적용 자료까지 제작한 상태다.

재제작: Blender 5.2에서 `scripts/build_animations.py`를 실행한다. 이어 `preview_and_validate.py`로 재임포트 검사, Python/numpy로 `validate_assets.py`를 실행한다. 내보내기 도구의 shape key NLA 샘플링 문제는 `bake_bow_channels.py`가 원본과 같은 60fps morph 키를 GLB에 기록하여 처리하며, 원래 스킨·본 데이터는 건드리지 않는다. 새로 내보낸 뒤에는 실제 GLB 검수를 다시 실행한다.
