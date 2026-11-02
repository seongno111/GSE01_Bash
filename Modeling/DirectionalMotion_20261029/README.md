# 이전 방향별 애니메이션 — 현재 적용 대상 아님

2026-10-29 사용자 결정에 따라 이 방식은 폐기했다. 현재는 각 리그의 **Walk 1개·Run 1개**를 공통으로 재생하고, 게임 로직에서 모델을 회전한다.

**현재 전달본: [LocomotionShared_20261029](../LocomotionShared_20261029/README.md)**

이 폴더의 GIF와 `history_before_shared_locomotion/`은 이전 제작 이력이다. 현재 원본·GLB에는 ScreenUp/Down·TurnStep·TurnScreen 및 기존 Turn_L/R 클립이 없다. 기존 `motion_catalog.json`은 active=false와 빈 clips/files를 제공하는 폐기 안내다.

방향별 모델이 필요했던 이전 상태는 `../LocomotionShared_20261029/backups_before_cleanup/`에 보관했다. 이 폴더의 최초 backups는 방향별 작업 전 상태이므로 현재 정리본을 대체해 적용하지 않는다.
