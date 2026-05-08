# CLion 색인 설정

`clion/` 폴더는 CLion 사용자가 선택적으로 사용하는 보조 설정입니다.

이 CMake 설정은 실제 Pintos 빌드용이 아니라 코드 탐색과 자동완성을 위한 색인용입니다. `../pintos` 아래의 `.c`, `.h`, `.S` 파일을 색인하며 `build/` 산출물은 제외합니다.

실제 빌드와 테스트는 Docker 컨테이너 안에서 Pintos Makefile로 실행합니다. 테스트 실행 방법은 `TESTING.md`를 따릅니다.

VSCode 사용자나 CLI 사용자는 이 폴더를 무시해도 됩니다.
