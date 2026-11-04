# CoSync

CoSync는 Linux 터미널에서 여러 사용자가 채팅하며 텍스트 파일 한 개를 함께 편집하는 프로그램입니다. 편집기에서 파일을 저장하면 서버가 다른 참여자에게 내용을 전달합니다. 원하는 시점에 버전을 남겼다가 이전 내용으로 복원할 수 있습니다.

이 브랜치는 **2024년 12월 시스템 프로그래밍 팀 과제의 개인 개선판**입니다. 팀 프로젝트에서 구현한 채팅·파일 동기화·버전 관리를 바탕으로 통신, 파일 저장, 로그인과 터미널 입력 처리를 개선했습니다. [팀 프로젝트 README](https://github.com/Keytype-Re/CoSync/blob/main/README.md) · [개선 기록](#개선-기록)

## Ubuntu에서 시작하기

개발 환경은 VMware의 Ubuntu 24.04.1입니다. 같은 VM에서 서버 한 개와 클라이언트 두 개를 실행하는 예입니다. 각 터미널에서 같은 브랜치의 빌드 결과를 사용하세요.

**1. 빌드** · 새로 내려받는 경우 아래 명령을 실행합니다. 이미 해당 브랜치에 있다면 저장소 루트에서 `src=...` 줄부터 진행합니다. GCC와 Make, GNU Readline 개발 헤더가 필요합니다. Ubuntu 패키지는 `build-essential libreadline-dev`입니다. 자동 테스트에는 Python 3도 필요합니다.

```bash
sudo apt install git build-essential libreadline-dev python3
git clone --branch codex/stage3-stability-refactor https://github.com/Keytype-Re/CoSync.git
cd CoSync
src='[2024.12]_SystemProgramming_Project_CoSync_Final_Source_Code'
make -B -C "$src/server"
make -B -C "$src/client"
```

`-B`는 저장소에 포함된 실행 파일과 오브젝트 파일이 있어도 모든 소스를 다시 빌드합니다.

**2. 서버 실행** · 저장소 루트에서 시작합니다. 서버는 TCP 8080을 사용하며, 아래 명령은 채팅 기록과 공유 파일을 담을 새 실행 디렉터리를 만듭니다.

```bash
server="$PWD/[2024.12]_SystemProgramming_Project_CoSync_Final_Source_Code/server/server_app"
state=$(mktemp -d)
echo "서버 데이터: $state"
(cd "$state" && exec "$server")
```

**3. 클라이언트 실행** · 새 터미널을 열어 같은 저장소 루트로 이동한 뒤 실행합니다. 두 번째 참여자도 별도 터미널에서 아래 블록을 반복하고 서로 다른 이름을 입력합니다.

```bash
client="$PWD/[2024.12]_SystemProgramming_Project_CoSync_Final_Source_Code/client/client_app"
work=$(mktemp -d)
echo "편집할 파일: $work/watch/shared_file.txt"
(cd "$work" && COSYNC_SERVER_ADDR=127.0.0.1 exec "$client")
```

`COSYNC_SERVER_ADDR`는 클라이언트가 접속할 서버의 IPv4 주소입니다. 다른 VM에서는 예제의 `127.0.0.1`을 서버 주소로 바꾸세요. 미설정 시 코드의 기본값은 `0.0.0.0`입니다. 실행 위치마다 `user.txt`와 데이터가 따로 생깁니다. 같은 사용자 또는 서버 기록으로 재시작하려면 처음 출력된 디렉터리에서 해당 실행 파일을 다시 여세요.

## 두 사람으로 기능 확인하기

1. A가 채팅창에 `hello`를 입력하고 B 화면에 메시지가 표시되는지 확인합니다.
2. 별도 편집기에서 A의 실행 폴더에 있는 `watch/shared_file.txt`에 `draft one`을 저장합니다. B의 같은 이름 파일에도 반영되는지 확인합니다.
3. A가 채팅창에 `/commit`, 이어서 `/log`를 입력해 버전 번호를 확인합니다.
4. 내용을 `draft two`로 바꿔 저장하고 B에게 반영되면, A에서 `/rebase 1`을 입력합니다. 새 서버 디렉터리 기준으로 두 사람의 파일에 첫 버전인 `draft one`이 돌아와야 합니다.
5. 클라이언트에서 `/quit`을 입력하고 서버 터미널에서 `Ctrl+C`로 마칩니다.

| 입력 | 동작 |
| --- | --- |
| 일반 텍스트 | 참여자에게 채팅을 보내고 서버가 이력을 기록합니다. |
| `/load PATH` | 로컬 텍스트 파일을 공유 파일로 불러옵니다. 상대 경로는 클라이언트의 실행 위치 기준입니다. |
| `/new` | 현재 공유 파일의 내용을 비웁니다. |
| `/commit` | 클라이언트에서 읽은 공유 파일 내용을 새 버전으로 보관합니다. |
| `/log` | 저장된 파일 버전 목록을 요청합니다. |
| `/rebase N` | N번 버전으로 서버 공유 파일을 복원하고 참여자에게 전달합니다. |
| `/help`, `/quit` | 도움말 표시, 클라이언트 종료 |

## 구조와 데이터

```text
클라이언트 A ── TCP ── 서버 ── TCP ── 클라이언트 B
편집기 저장 → inotify → 공유 파일 저장 → 다른 클라이언트의 파일에 반영
```

- [클라이언트](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/client) · 키보드와 소켓을 기다리는 화면 루프와 파일 감시 스레드를 사용합니다. GNU Readline이 입력 중인 줄을 보관합니다.
- [서버](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/server) · 연결마다 로그인과 수신을 담당하고, 채팅·파일·버전 요청은 잠금으로 순서를 정해 처리합니다.
- [공통 프로토콜](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/protocol.h) · 양쪽이 같은 패킷 선언을 빌드하며 [공통 구현](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/protocol.c)에서 소켓 송수신과 텍스트 파일 처리를 공유합니다.

| 실행 위치 | 저장되는 데이터 |
| --- | --- |
| 클라이언트 | `user.txt`에 로컬 사용자 이름, `watch/shared_file.txt`에 편집할 사본 |
| 서버 | `user.txt`에 등록 이름 목록, `chat_log.txt`에 채팅 이력, `shared_file.txt`에 현재 공유 내용 |
| 서버의 `version_logs/` | `1_shared_file.txt`와 같이 번호를 붙인 버전별 사본 |

## 자동 테스트

저장소 루트에서 실행합니다.

```bash
python3 '[2024.12]_SystemProgramming_Project_CoSync_Final_Source_Code/server/test_login_capacity.py' -v
```

[통합 테스트](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/server/test_login_capacity.py)는 임시 디렉터리로 소스를 복사해 서버와 클라이언트를 다시 빌드합니다. 루프백 연결과 임시 포트로 접속 정원, 파일 이벤트, 커밋·복원, 종료, 입력 중 창 크기 변경을 검사합니다.

## 개선 기록

| 문제 | 판단과 변경 | 확인 방법과 결과 |
| --- | --- | --- |
| 파일 저장 직후 커밋하면 서버의 이전 내용이 보관될 수 있었습니다. | 커밋 요청에 로컬에서 읽은 내용을 포함하고, 서버가 해당 내용으로 새 버전 파일을 생성합니다. | 테스트는 저장 직후 커밋하고 생성된 버전 파일의 내용을 검사합니다. [변경](https://github.com/Keytype-Re/CoSync/commit/899fc34) |
| 첫 로그인 중 클라이언트가 설명 없이 종료됐다는 보고가 있었습니다. | 사용자 입력을 기다리는 시간을 서버의 로그인 제한 시간에서 분리하고, 응답을 받기 전 연결이 닫히면 원인을 표시합니다. | 테스트는 이름 입력 전 연결 여부와 연결 종료 메시지를 검사합니다. 당시 종료 원인은 특정되지 않았습니다. [변경](https://github.com/Keytype-Re/CoSync/commit/e12cbf4) |
| 응답 출력과 입력이 겹치면 다음 명령이 사라진 것처럼 보이고 서버의 처리 결과를 확인하기 어려웠습니다. | 화면 루프가 입력 초안을 복원하고 창 너비로 정렬합니다. 서버는 요청 수신과 처리 결과를 기록합니다. | 가상 터미널 테스트는 입력 중 창을 줄이고 한글 메시지를 받은 뒤 다음 명령·채팅·서버 로그를 검사합니다. [변경](https://github.com/Keytype-Re/CoSync/commit/cc006fc) |

Ubuntu에서 개선판의 기능 동작은 수동으로 확인했습니다. 위 자동 테스트의 전체 통과 여부는 이번 문서 작성 시 재검증하지 않았습니다.

## 알려진 제약과 이후 작업

- 공유 파일은 NUL 바이트가 없는 최대 4,095바이트의 텍스트입니다.
- 파일 전체를 주고받으므로 동시 편집 시 나중에 서버가 처리한 내용이 앞선 수정을 덮을 수 있습니다.
- 사용자 이름으로 접속하며 통신 암호화와 비밀번호 인증은 구현되어 있지 않습니다. 신뢰하는 실습 네트워크에서 사용하세요.
- 향후 개선 후보는 동시 수정 충돌 감지와, 서로 다른 시스템에서도 읽을 수 있도록 C 구조체 전송을 명시적인 메시지 형식으로 바꾸는 작업입니다.
