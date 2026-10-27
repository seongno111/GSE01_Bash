# 모델링 에셋

2026-10-27 수정: [마을별 메인 건물 6종 랜드마크](Villages_v1/LANDMARKS_20261027.md). 기존 6개 CHAPEL 키를 유지하면서 쌍탑 대성당·태양 첨탑 수도원·지휘 요새·시계탑 상인회관·천구 관측소·송곳니 두목 전당으로 형상을 교체했다. [랜드마크 비교](Villages_v1/previews/landmarks_overview.jpg).

2026-10-26 추가 전달본:

- [마을 성향별 6세트·36개 건물](Villages_v1/README.md): 문화별 주택·성당·작업장·성문·담장과 배치 예시
- [신규 몬스터 6종·리깅·123개 클립](Monsters_v1/README.md): `modelKey` 기반 별도 팩
- [망토·성당 지붕 수정 및 적용 인계](Repairs_20261026/README.md): 기존 Hero_v1과 LowPolyKit_v3의 이름·인덱스를 유지한 수정

현재 권장본은 **LowPolyKit_v3**다. NPC·몬스터 리깅과 키프레임 애니메이션, 확장한 환경 모델이 포함되어 있다. 게임 적용과 빌드는 별도 작업에서 진행한다.

주인공은 별도의 **Hero_v1** 에셋이다. 후드·견갑·가죽 바지에 왼손 램프와 오른손 롱소드를 갖추고, v3와 호환되는 기본 본 이름을 사용한다.

- [주인공 원본](Hero_v1/hero_v1.blend)
- [주인공 GLB](Hero_v1/hero_rigged.glb)
- [주인공 파트·리깅 규격](Hero_v1/README.md)

- [v3 조립·리깅·애니메이션 규격](LowPolyKit_v3/README.md)
- [v3 Blender 원본](LowPolyKit_v3/lowpoly_kit_v3.blend)
- [v3 모델 카탈로그](LowPolyKit_v3/asset_catalog.json)
- [v3 애니메이션과 게임 행동 대응](LowPolyKit_v3/animation_catalog.json)
- [v3 주민별 외형 정보](LowPolyKit_v3/npc_appearance_assignments.json)

v3에서는 폐쇄형 기사 투구에 옆·뒤 보호판을 추가하고 내부의 실제 머리를 유지했다. v2에서 사용했던 폐쇄형 투구의 head=-1 규칙은 v3에 적용하지 않는다. 모든 머리 장비는 헤어만 숨긴다.

이전 정적 모델링 버전:

- [v2 조립 규격과 전달 파일](LowPolyKit_v2/README.md)
- [v2 Blender 원본](LowPolyKit_v2/lowpoly_kit_v2.blend)
- [v2 NPC 카탈로그](LowPolyKit_v2/asset_catalog.json)
- [v2 주민별 외형 정보](LowPolyKit_v2/npc_appearance_assignments.json)
- [이전 v1 규격](LowPolyKit_v1/README.md)

v2부터 headgear 슬롯 20을 추가했으며 v3도 21슬롯 순서를 유지한다. 적용 시 선택한 버전의 규격과 카탈로그를 함께 사용한다.
