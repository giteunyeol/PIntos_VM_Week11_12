# Pintos 테스트 실행 방법

이 문서는 Docker 컨테이너 안에서 Pintos 테스트를 실행하는 방법만 다룹니다. 호스트 환경의 도구 버전에 의존하지 않도록 기존 `.devcontainer/Dockerfile`로 Docker 이미지를 빌드한 뒤 컨테이너 내부에서 테스트를 실행합니다.

## Docker 이미지 빌드

저장소 루트에서 실행합니다.

```sh
docker build -t pintos-dev -f .devcontainer/Dockerfile .
```

## Docker 컨테이너 실행

저장소 루트에서 실행합니다.

```sh
docker run --rm -it -v "$PWD":/workspace -w /workspace pintos-dev
```

컨테이너의 작업 디렉터리는 `/workspace`입니다. 위 명령은 현재 저장소를 컨테이너의 `/workspace`에 마운트합니다.

## Pintos 환경 활성화

컨테이너 내부에서 실행합니다.

```sh
export PINTOS_ROOT=/workspace/pintos
source "$PINTOS_ROOT/activate"
```

## Project별 전체 테스트

컨테이너 내부에서 필요한 Project만 실행합니다.

```sh
make -C "$PINTOS_ROOT/threads" check
make -C "$PINTOS_ROOT/userprog" check
make -C "$PINTOS_ROOT/vm" check
make -C "$PINTOS_ROOT/filesys" check
```

## 특정 테스트 실행 예시

각 Project 디렉터리의 Makefile은 `build/` 아래의 개별 테스트 결과 타깃을 실행할 수 있습니다.

```sh
make -C "$PINTOS_ROOT/threads" build/tests/threads/alarm-single.result
make -C "$PINTOS_ROOT/userprog" build/tests/userprog/args-none.result
make -C "$PINTOS_ROOT/vm" build/tests/vm/pt-grow-stack.result
make -C "$PINTOS_ROOT/filesys" build/tests/filesys/base/lg-create.result
```

## 결과 파일 위치

테스트 결과는 각 Project의 `build/` 디렉터리 아래에 생성됩니다.

```text
$PINTOS_ROOT/<project>/build/results
$PINTOS_ROOT/<project>/build/grade
$PINTOS_ROOT/<project>/build/tests/.../*.output
$PINTOS_ROOT/<project>/build/tests/.../*.result
```

예를 들어 threads Project의 전체 결과는 `$PINTOS_ROOT/threads/build/results`에서 확인합니다.

## 주의사항

- Docker 이미지는 기존 `.devcontainer/Dockerfile`을 기준으로 빌드합니다.
- 컨테이너를 새로 열 때마다 `PINTOS_ROOT` 설정과 `source "$PINTOS_ROOT/activate"`를 다시 실행합니다.
- 실제 빌드와 테스트는 Pintos의 기존 Makefile을 사용합니다.
- 테스트 산출물은 `build/` 아래에 생성되며 Git에 커밋하지 않습니다.
- CLion 보조 설정은 코드 탐색용이며 테스트 실행 방법을 대체하지 않습니다.
