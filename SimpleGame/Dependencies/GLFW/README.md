# GLFW 3.5.1

공식 [GLFW 3.5.1 Windows 배포본](https://github.com/glfw/glfw/releases/tag/3.5.1)에서 가져온 파일이다. `LICENSE.md`에 배포 라이선스를 포함한다.

| Visual Studio 플랫폼 | 배포 ZIP | 포함 파일 |
| --- | --- | --- |
| Win32 | `glfw-3.5.1.bin.WIN32.zip` | `lib/Win32/glfw3dll.lib`, `lib/Win32/glfw3.dll` |
| x64 | `glfw-3.5.1.bin.WIN64.zip` | `lib/x64/glfw3dll.lib`, `lib/x64/glfw3.dll` |

두 플랫폼 모두 배포본의 `lib-vc2022` 바이너리와 공용 `include/GLFW` 헤더를 사용한다. 솔루션의 Debug와 Release는 플랫폼에 맞는 import library를 링크하고 빌드 후 GLFW 및 GLEW DLL을 실행 파일 옆으로 복사한다. 체크아웃에 필요한 GLFW 파일이 모두 들어 있어 별도 설치나 패키지 복원이 필요 없다.

빌드에는 프로젝트에 지정된 MSVC v145와 Windows SDK가 필요하다. 실행 환경에는 해당 아키텍처의 Microsoft Visual C++ v14 런타임과 OpenGL 3.3 Core 지원 드라이버가 필요하다.

## 프레임 비교

같은 PC에서 동일한 해상도, 장면, 후처리 설정과 빌드 구성을 사용한다. 초기 로딩과 최소화 구간을 제외하고 기존 `Profiling` 로그의 FPS, 프레임 시간 p95/p99, 업데이트·렌더·스왑 CPU 시간, 프레임당 드로우콜을 비교한다. GLFW 버전은 60Hz 목표로 이벤트를 대기하며, 프레임 루프에서 페이싱을 담당하도록 스왑 간격을 0으로 요청한다.
