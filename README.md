# CoSync

CoSync는 Linux 터미널에서 여러 사용자가 채팅하며 텍스트 파일 한 개를 함께 편집하는 프로그램입니다. 편집기에서 파일을 저장하면 서버가 다른 참여자에게 내용을 전달합니다. 원하는 시점에 버전을 남겼다가 이전 내용으로 복원할 수 있습니다.

이 브랜치는 **2024년 12월 시스템 프로그래밍 팀 과제**에서 구현한 버전을 설명합니다. 원본 소스 기준은 [d1d9011](https://github.com/Keytype-Re/CoSync/tree/d1d9011)입니다. 이후 개인 작업은 [개선판 README](https://github.com/Keytype-Re/CoSync/blob/codex/stage3-stability-refactor/README.md)에 기록했습니다.

## Ubuntu에서 시작하기

개발 환경은 VMware의 Ubuntu 24.04.1입니다. 같은 VM에서 서버 한 개와 클라이언트 두 개를 실행하는 예입니다. 각 터미널에서 같은 브랜치의 빌드 결과를 사용하세요.

**1. 빌드** · 새로 내려받는 경우 아래 명령을 실행합니다. 이미 해당 브랜치에 있다면 저장소 루트에서 `src=...` 줄부터 진행합니다. GCC와 Make가 필요합니다. Ubuntu 패키지는 `build-essential`입니다.

```bash
sudo apt install git build-essential
git clone --branch main https://github.com/Keytype-Re/CoSync.git
cd CoSync
src='[2024.12]_SystemProgramming_Project_CoSync_Final_Source_Code'
make -B -C "$src/server"
make -B -C "$src/client"
```

`-B`는 저장소에 포함된 실행 파일과 오브젝트 파일이 있어도 모든 소스를 다시 빌드합니다. 원본 클라이언트의 접속 주소는 `client/main.c`의 `inet_addr("0.0.0.0")`에 고정되어 있습니다. 같은 VM에서 루프백 주소를 명시하려면 해당 인자를 `"127.0.0.1"`로 바꾸고 클라이언트를 다시 빌드하세요. 다른 VM에서는 서버의 IPv4 주소를 사용합니다. 주소를 지정하는 환경변수는 없습니다.

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
(cd "$work" && exec "$client")
```

실행 위치마다 `user.txt`와 데이터가 따로 생깁니다. 같은 사용자 또는 서버 기록으로 재시작하려면 처음 출력된 디렉터리에서 해당 실행 파일을 다시 여세요.

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
| `/commit` | 명령을 처리할 때 서버에 있는 공유 파일 내용을 새 버전으로 보관합니다. |
| `/log` | 저장된 파일 버전 목록을 요청합니다. |
| `/rebase N` | N번 버전으로 서버 공유 파일을 복원하고 참여자에게 전달합니다. |
| `/help`, `/quit` | 도움말 표시, 클라이언트 종료 |

## 구조와 데이터

```text
클라이언트 A ── TCP ── 서버 ── TCP ── 클라이언트 B
편집기 저장 → inotify → 공유 파일 저장 → 다른 클라이언트의 파일에 반영
```

- [클라이언트](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/client) · POSIX 스레드로 키보드 입력, 서버 수신, 파일 변경 감시를 분담합니다. 메인 루프가 큐에서 수신 패킷을 꺼내 처리합니다.
- [서버](%5B2024.12%5D_SystemProgramming_Project_CoSync_Final_Source_Code/server) · 연결별 수신 스레드가 큐에 패킷을 넣으면 메인 루프가 채팅·파일·버전 담당 함수로 전달합니다.

| 실행 위치 | 저장되는 데이터 |
| --- | --- |
| 클라이언트 | `user.txt`에 로컬 사용자 이름, `watch/shared_file.txt`에 편집할 사본 |
| 서버 | `user.txt`에 등록 이름 목록, `chat_log.txt`에 채팅 이력, `shared_file.txt`에 현재 공유 내용 |
| 서버의 `version_logs/` | `1_shared_file.txt`와 같이 번호를 붙인 버전별 사본 |

## 검증 범위

원본은 Ubuntu에서 실행과 기능을 수동 확인한 팀 프로젝트 버전입니다. 위 두 사용자 시나리오로 채팅·동기화·버전 복원을 확인할 수 있습니다. 이 브랜치에는 자동 통합 테스트가 없습니다. 문서 작성 시 빌드와 실행을 다시 수행하지 않았습니다.

## 알려진 제약과 이후 작업

- 원본 패킷의 파일 입력 칸은 4,096바이트이며 큰 파일을 분할 전송하지 않습니다. 짧은 텍스트로 사용하세요. 감시 폴더의 다른 파일도 전송될 수 있으므로 예제 파일만 두세요.
- 파일 전체를 주고받으므로 동시 편집 시 나중에 서버가 처리한 내용이 앞선 수정을 덮을 수 있습니다.
- 사용자 이름으로 접속하며 통신 암호화와 비밀번호 인증은 구현되어 있지 않습니다. 신뢰하는 실습 네트워크에서 사용하세요.
- 원본에서는 파일 저장과 커밋이 별개 요청입니다. 커밋 전에 다른 참여자에게 수정 내용이 반영됐는지 확인하세요. 통신 오류 처리와 파일 저장 순서는 개선판에서 보완했습니다.
