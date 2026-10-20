# 모델링 에셋

현재 권장본은 **LowPolyKit_v3**다. NPC·몬스터 리깅과 키프레임 애니메이션, 확장한 환경 모델이 포함되어 있다. 게임 적용과 빌드는 별도 작업에서 진행한다.

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
