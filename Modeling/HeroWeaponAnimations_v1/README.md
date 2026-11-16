# 주인공 키프레임 애니메이션

2026-11-10 체형 개정. 후드를 제외한 키 1.84m와 머리 높이 약 0.365m를 기준으로 **5.04등신**을 만들었다. 기본 장검과 램프를 포함한 모델은 1,849에서 6,221삼각형으로 늘렸다. 옷깃, 이음새, 버클, 무릎 보호대와 부츠, 망토 주름을 모델에 넣었다. [체형 비교](previews/hero_proportions.jpg).

49개 동작을 새 어깨·팔꿈치·골반·무릎 위치에 맞춰 구웠다. 걷기·달리기는 지지 발에서 무게가 넘어가는 동안 반대 발이 가속해 따라오도록 바꿨다. 공격은 준비에서 힘을 모으고, 타격을 빠르게 지나간 뒤 무기와 몸통이 따라가며 감속한다. 망치의 양손 간격과 활의 시위 타이밍, 전용 스킬의 타격 시점은 카탈로그에서 확인할 수 있다. [공통 동작 미리보기](previews/common_motion_overview.gif).

2026-11-10. 공통 동작 11개와 무기별 동작 30개를 개정하고 전용 스킬 8개를 추가했다. 걷기·달리기는 착지와 발끝 밀기, 골반의 체중 이동을 다시 잡았다. 공격에는 준비와 타격의 속도 차이, 피격과 일어나기에는 무릎과 몸통의 반응을 넣었다. 망치의 손잡이 파지는 0.30m 간격의 양손 파지다. 전체 라이브러리에 **49개 동작**이 있다.

## 전용 스킬

| 무기 | 스킬 클립 | 접촉 시점(초) | 길이(초) |
|---|---|---|---:|
| 지팡이 · 잔불의 문양 | `HERO_Staff_SkillAshField` | 0.45 / 0.80 / 1.15 | 1.40 |
| 활 · 가시 부채살 | `HERO_Bow_SkillArrowFan` | 0.55 | 0.95 |
| 철퇴 · 방패 분쇄 | `HERO_Mace_SkillGuardBreak` | 0.38 | 0.82 |
| 도끼 · 사형 집행 | `HERO_Axe_SkillExecution` | 0.55 | 1.05 |
| 망치 · 대지 강타 | `HERO_Hammer_SkillGroundSlam` | 0.72 | 1.35 |
| 창 · 잿빛 관통 | `HERO_Spear_SkillLongThrust` | 0.40 | 0.86 |
| 단검 · 세 번의 상흔 | `HERO_Dagger_SkillFlurry` | 0.14 / 0.27 / 0.40 | 0.67 |
| 장검 · 잿바람 회전베기 | `HERO_Sword_SkillWhirlwind` | 0.35 / 0.75 | 1.12 |

명세의 시점을 GLB 샘플과 카탈로그의 `skill_contact`에 기록했다. 스킬은 1회 재생이며 시작과 끝은 장착 무기의 Ready 또는 기본 장검의 Idle에 맞췄다. 장검은 `Hero_v1/hero_rigged.glb`에도 들어 있으며 0.15~0.95초에 고정 원점에서 두 바퀴 회전한다. 활의 손과 시위는 0.55초에 풀리고 같은 시점에 장착 화살이 숨겨진다. 타격·탄체의 게임 시각은 [제작 명세](../../doc/무기_스킬_애니메이션_명세.md)를 따른다. [스킬 8종 미리보기](previews/skill_overview.gif).

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

위치·쿼터니언 회전·스케일을 60fps 키프레임으로 구웠다. 대지 강타는 양손 파지의 보간 오차를 줄이기 위해 한 프레임을 네 구간으로 나눠 샘플을 넣었다. 디딤이 있는 동작은 발을 들어 옮기며 모든 클립은 원점을 고정한다. 장검 회전베기는 수직축 회전이 동작에 포함된다.

Blender 프레임 1은 GLB 시간 0초다. 60fps 정수 프레임과 명세 시각이 어긋나는 스킬은 Blender 마커를 가장 가까운 프레임에 두고 GLB 샘플은 명세의 정확한 초에 맞췄다. GIF는 12fps 미리보기다.

## 전달 파일

| 파일 | 내용 |
|---|---|
| `hero_weapon_animations_v1.blend` | 원본 주인공·무기·새 Action·시위/화살 shape key |
| `hero_weapon_animation_library.glb` | 개정한 41개와 스킬 8개를 포함한 49개 본 애니메이션 |
| `hero_staff_actions.glb` 등 무기별 GLB 7개 | 장착 모델과 해당 무기의 기본 동작 및 전용 스킬 |
| `../Hero_v1/hero_rigged.glb` | 공통 동작 11개와 장검 스킬 |
| `animation_catalog.json` | 무기 키 대응·클립 이름·길이·루프·이벤트·장비 표시·마스크 정보 |
| `authoring_keyposes.json` | 동작별 주요 포즈와 구간별 시간 |
| `validation.json`, `reimport_validation.json` | 실제 GLB 데이터 및 재임포트 검수 |
| `previews/animations_overview.jpg` | 무기 7종 대표 공격 비교 |
| `previews/hero_bow.gif` 등 | 실제 무기별 GLB 재생 미리보기 |
| `previews/all_clips_contact.jpg` | 49개 동작의 대표 프레임 |
| `previews/common_motion_overview.gif`, `previews/skill_overview.gif` | 공통 동작과 무기 스킬의 미리보기 |
| `scripts/` | 재제작·내보내기·검수용 도구. 게임 소스가 아님 |

전체 라이브러리는 기본 검·램프 메시를 그대로 보존하므로 **새 무기의 외형을 검수할 때는 무기별 GLB를 사용**한다. 무기별 GLB에는 오른손 기본 검이 빠져 있고, 활·망치·창에는 왼손 램프도 빠져 있다. 별도 무기를 중복으로 붙이지 않는다. 새 무기는 이 팩 안에서 원래 손 본에 강체 웨이트로 연결되어 있다.

25개 파트와 33개 본은 기존 이름으로 찾을 수 있다. 이번 체형에 맞춰 본의 바인드와 웨이트를 갱신했으므로 공통 모델과 장착 모델은 같은 체형 개정본을 함께 사용한다. 독립 무기를 붙이는 경우 `HeroWeapons_v1/weapon_catalog.json`의 손 본 장착 회전을 사용한다.

## 이동과 장비 조립 인계

**걷기는 `HERO_Walk` 하나, 달리기는 `HERO_Run` 하나**다. 새 무기별 이동·화면 방향별 이동·방향 전환 클립을 추가하지 않았다. 방향은 다른 작업에서 Actor 회전으로 처리한다. 공통 이동은 전체 라이브러리에만 한 번 포함했다.

이동 중 무기 자세가 필요할 때 공통 Walk/Run의 하체와 해당 무기 Ready의 상체를 조합하도록 `upper_body_mask`를 제공한다. 특히 양손 무기는 기존 검·램프용 전체 몸 이동을 그대로 덮으면 양손 파지가 풀리므로 이 마스크가 필요하다. 레이어 블렌딩 실행 코드는 포함하지 않았다.

- 지팡이·철퇴·도끼·단검: 오른손 무기, 왼손 램프 유지.
- 활: 왼손 활, 오른손 화살/시위. 기존 검·램프 메시와 램프 광원을 숨긴다.
- 망치·창: 오른손 주 파지 + 왼손 보조 파지. 기존 검·램프 메시와 램프 광원을 숨긴다.
- 골격·클립을 옮길 때 본의 **이름**으로 대응한다. 파일의 노드 인덱스가 다른 파일과 같다고 가정하지 않는다.

`animation_catalog.json`의 무기별 `hide_original_meshes`, `hide_lamp_light`, `clip_names`와 `weapon_key`가 적용 기준이다. 게임 enum이나 NPC 슬롯 번호를 새로 할당하지 않았다.

## 시위·활대·화살

활의 `__Body`와 `__String`에 `Draw` shape key가 있다. 중립 0에서 완전 당김 1까지 활대 끝이 휘고 시위 중앙이 약 0.29m 당겨진다. 오른손은 시위 중앙을 따라 움직인다. 활 전용 GLB의 7개 클립에는 본과 동기화한 morph weight 채널이 있다. 시위와 화살까지 재생할 때는 활 전용 GLB를 사용한다.

화살 메시의 `Hide` shape key는 0이면 실제 화살, 1이면 nock 위치로 축소되어 보이지 않는다. 보이는 시점과 발사 시점을 STEP 키로 구웠으며 `arrow_show`, `arrow_hide`, `projectile_release` 이벤트도 카탈로그에 기록했다. 엔진이 morph를 지원하지 않는 경우 이벤트로 화살 표시와 시위 변형을 연결하는 처리가 필요하다.

발사는 `HERO_Bow_Release` 시작 후 **0.10초**다. 게임에서 이때 탄체를 생성한다. 타격·시전도 클립별 `melee_contact`, `cast_release`와 초·프레임 정보를 제공한다. 이벤트 메타데이터는 `animation_catalog.json`에 있으며 GLB가 게임의 피해 판정이나 이펙트를 자동으로 실행하지는 않는다.

## 검수

9개 GLB에서 본 계층, 바인드, 메시와 49개 동작을 비교했다. Blender 재임포트에서 접지, 양손 파지, 루프, 전환 포즈를 확인했다. 스킬의 타격 샘플 시각, 활 시위와 화살의 전환, 장검의 회전량을 검사했다. 결과는 `validation.json`과 `reimport_validation.json`에 있다.

재제작 시 Blender 5.2에서 `scripts/build_animations.py`에 `out=` 출력 폴더를 지정하면 공통·장착 모델과 Blender 원본을 내보낸다. 같은 출력 폴더에 대해 Blender로 `preview_and_validate.py`를 실행하고 Python/numpy로 `validate_assets.py`를 실행한다.
