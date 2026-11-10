# 망치 파지와 휘두르는 각도 수정

2026-11-05. 기존 동작은 손의 긴 축이 손잡이 축과 나란해 손목이 크게 꺾였고, 준비·방어 자세의 망치 머리가 얼굴을 가렸다.

- 손잡이가 주먹을 가로질러 지나가도록 양손 방향을 바꿨다. 손바닥의 파지 중심은 계속 손잡이에 붙어 있다.
- 양손 간격을 0.22m에서 **0.30m**로 조정하고, 전완과 손이 일직선을 유지할 수 있도록 팔꿈치를 함께 계산해 키프레임으로 구웠다.
- 준비·방어 때 망치를 캐릭터 오른쪽에 비스듬히 두어 얼굴과 시야를 확보했다.
- 내려치기는 오른쪽 어깨 위에서 전방 아래로, 올려치기는 낮은 준비 자세에서 전방 위로 휘두른다. 망치 머리의 양쪽 타격면이 그 궤적을 따르도록 축을 조정했다.

`HERO_Hammer_Ready`, `HERO_Hammer_Smash`, `HERO_Hammer_Uppercut`, `HERO_Hammer_Guard`의 **기존 이름·길이·루프·접촉 이벤트 시점은 유지**했다. 망치 모델 자체, 공통 Walk/Run, 다른 무기 6종의 GLB와 나머지 37개 본 애니메이션도 유지했다.

## 적용할 파일

같은 경로의 다음 파일을 갱신했다.

- `hero_weapon_animations_v1.blend`: 편집 원본
- `hero_weapon_animation_library.glb`: 전체 41개 클립, 그중 망치 4개 수정
- `hero_warhammer_actions.glb`: 실제 망치를 장착한 주인공과 수정한 4개 동작
- `animation_catalog.json`: 손 간격 등 수정 정보

원래 `HeroWeapons_v1`의 정적 망치 메시나 손 본 장착 회전을 변경하지 않는다. 이 수정은 본 애니메이션의 손·팔·무기 자세에 포함되어 있다. 게임 적용·빌드는 이번 작업에 포함하지 않았다.

## 확인 자료

- [준비 자세 전후 비교](previews/hammer_grip_before_after.jpg)
- [내려치기](previews/hero_warhammer.gif)
- [올려치기](previews/hero_warhammer_uppercut.gif)
- [수정한 4개 동작](previews/hammer_grip_four_actions.jpg)

`hammer_grip_validation.json`은 수정 전후 실제 GLB의 모든 60fps 키프레임을 비교한다. 손잡이에 대한 손의 직각 파지, 손바닥 중심 접촉, 손목 꺾임 감소와 손목 5도 이내를 검사한다. `validation.json`은 기존 본·스킨·발 고정·루프·파지 간격과 다른 37개 동작 및 다른 6종 GLB의 보존을 검사한다.

이전 원본과 GLB는 `revisions/20261105_before_hammer_grip`에 보관했다.
