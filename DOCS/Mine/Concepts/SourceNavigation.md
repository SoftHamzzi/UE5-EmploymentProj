# 엔진 소스에서 원하는 기능 찾기

> "이런 걸 하는 함수가 있을 것 같은데 이름을 모르겠다"를 해결하는 방법.
> 이름 검색은 최후의 수단이고, 주력은 **아는 것 하나에서 출발해 따라가는 것**이다.

---

## 핵심 문제

**모르는 이름은 검색할 수 없다.**

`FFormatArgumentValue`를 찾으려고 `FFormatArgumentValue`를 검색할 수는 없다.
그 이름을 알았다면 이미 찾은 것이다. Search Everywhere(Shift 두 번)는 **이름을 아는 것으로 점프하는 도구**지 탐색 도구가 아니다.

---

## ① 앵커 추적 — 주력 (체감 80%)

아무 심볼이나 하나 잡고 **Go to Definition으로 따라간다.** 시그니처에 등장하는 인자 타입·반환 타입·enum이 전부 다음 단서다.

**실제 사례 — "`FText::Format`에 뭘 넣을 수 있나"를 알아낸 경로:**

```
FText::Format                                          ← 앵커 (이미 쓰고 있던 것)
    ↓ Go to Definition
Text.h:647   Format(FTextFormat Fmt, const FFormatNamedArguments& InArguments)
                    ~~~~~~~~~~~       ~~~~~~~~~~~~~~~~~~~~~~~     ← 새 이름 2개 획득
    ↓ Go to Definition (FFormatNamedArguments)
Text.h:156   typedef TSortedMap<FString, FFormatArgumentValue, ...>
                                         ~~~~~~~~~~~~~~~~~~~      ← 새 이름 1개 더
    ↓ Go to Definition
Text.h:927   class FFormatArgumentValue { 생성자 목록 = 답 }
```

**검색은 한 번도 하지 않았다.** 네 번의 점프로 도착했다.

앵커는 아무거나 된다 — 이미 쓰는 함수, 컴파일 에러에 뜬 타입, Blueprint 노드 이름, 로그에 찍힌 클래스명.

---

## ② 도착한 헤더를 통째로 훑기

목적지에 도착하면 **그 심볼만 보지 말고 파일 전체를 훑는다.** 언리얼 헤더는 관련된 것을 한 파일에 몰아둔다.

`Text.h` 하나에 들어 있는 것:
```
FText / FTextFormat / FFormatArgumentValue / FFormatNamedArguments
FFormatOrderedArguments / ETextGender / ETextPluralForm / ETextFormatFlags
```

한 번 열었을 때 목차를 읽어두면 **다음에 검색할 일이 없어진다.**

> IDE의 File Structure(파일 구조) 뷰를 켜면 헤더 전체 심볼이 목록으로 나온다. 2000줄 헤더도 1분이면 훑는다.

---

## ③ 앵커가 없을 때 — 주석을 자연어로 검색

**"원하는 기능을 문장으로 만들어 검색"은 맞는 발상인데, 대상이 인터넷이 아니라 엔진 소스여야 한다.**
언리얼 헤더의 doc comment가 사실상 공식 문서이고 자연어로 쓰여 있다.

```
검색 범위: Engine/Source/Runtime/**/Public/**/*.h

  "format pattern"      → FTextFormat 계열
  "lag compensation"    → 관련 시스템
  "line of sight"       → AIPerception / EnvQuery
```

**범위를 `Public/*.h`로 제한하는 것이 핵심이다.** 엔진 전체를 검색하면 `.cpp`와 생성 코드가 쏟아져 쓸모없다.

---

## ④ 명명 규칙을 검색 문법으로 쓰기

언리얼은 명명이 극단적으로 일관돼서 **이름을 추측할 수 있다.**

| 찾는 것 | 검색 패턴 |
|---|---|
| 설정값 모음 | `*Settings` (`UDeveloperSettings` 상속) |
| 전역 서비스 | `*Subsystem` |
| Blueprint 노드 모음 | `*BlueprintLibrary` / `*FunctionLibrary` / `*Statics` |
| 헬퍼 | `*Helper` / `*Utils` |
| 포맷 인자 | `FFormat*` |

접두사도 필터다:

| 접두사 | 의미 |
|---|---|
| `F` | 구조체 / 일반 클래스 |
| `U` | UObject 파생 |
| `A` | Actor 파생 |
| `I` | 인터페이스 |
| `E` | enum |
| `T` | 템플릿 |

---

## ⑤ 사용법이 궁금하면 호출부를 찾는다

**"이 함수를 어떻게 쓰는가"의 최고의 문서는 Epic의 호출부다.** 헤더 주석에 없는 규약이 거기 있다.

```
Find Usages, 또는 Engine/Source 범위에서 함수명 검색
```

Lyra는 이 용도로 존재한다고 봐도 된다. **"Epic은 X를 어떻게 하나"는 검색보다 Lyra를 읽는 게 빠르다.**

---

## ⑥ 타입 계층 (Type Hierarchy)

"이걸 상속한 게 뭐가 있나 / 이 인터페이스를 누가 구현했나"는 텍스트 검색으로 못 찾는다. IDE의 **Type Hierarchy**를 쓴다.

`UDeveloperSettings`를 열고 파생 목록을 보면 엔진의 모든 설정 클래스가 나온다.

---

## ⑦ 인터넷의 자리

| 상황 | 인터넷 |
|---|---|
| **개념/시스템 이름 자체를 모를 때** | ✅ "언리얼 서버 사이드 리와인드" → SSR이라는 이름을 얻음 |
| 특정 API 시그니처 | ❌ 소스가 정확하고 버전도 맞다 |
| 특정 함수의 동작 | ❌ 5.0 기준 블로그가 5.7에서 틀린 경우가 흔하다 |

**인터넷은 "이름을 얻는 데"까지만 쓰고, 이름을 얻으면 소스로 들어온다.**
개념 검색 → 앵커 확보 → ①번으로 복귀.

---

## 전제 조건 — 엔진 소스 인덱싱

**위의 전부가 IDE에 엔진 소스가 인덱싱돼 있어야 성립한다.**
소스가 없으면 Go to Definition이 헤더에서 멈추거나 아예 동작하지 않는다.

확인할 것:
- `Generate Project Files`로 만든 솔루션에 `UE5` 프로젝트(Engine)가 함께 있는가
- `C:\Program Files\Epic Games\UE_5.7\Engine\Source`가 실제로 채워져 있는가

---

## 찾은 뒤 — 모듈 의존성 확인

새 클래스를 발견하면 **어느 모듈 소속인지**를 본다. 경로가 `Runtime/<모듈명>/Public/...` 이므로 바로 알 수 있고, 그 모듈이 `Build.cs`에 없으면 링크 에러가 난다.

```
Runtime/UMG/Public/Components/ProgressBar.h
        ~~~                              ← UMG 모듈
```

전이 의존성으로 우연히 빌드가 통과할 수도 있지만, **엔진 버전업 시 깨지므로 명시적으로 추가한다.**

---

## 탐색 순서 요약

```
1. 아는 심볼에서 Go to Definition으로 따라간다      ← 대부분 여기서 끝
2. 도착한 헤더를 통째로 훑는다
3. 앵커가 없으면 Public/*.h에서 주석을 자연어로 검색
4. 명명 규칙으로 이름을 추측해 심볼 검색
5. 사용법은 엔진 / Lyra의 호출부를 본다
6. 개념 이름 자체를 모를 때만 인터넷
```

---

## 이 프로젝트에서의 적용 사례

| 알아낸 것 | 방법 | 근거 위치 |
|---|---|---|
| `FText::Format`에 넣을 수 있는 타입 | ① 앵커 4회 추적 | `Text.h:927` 생성자 목록 |
| `FText::Format` 이스케이프 문자가 백틱 | ③ 주석/상수 검색 | `TextFormatter.h:50` `EscapeChar = '`'` |
| `GetActiveEffectsTimeRemainingAndDuration`의 Pair 순서 | ⑤ 호출부 확인 | `GameplayAbility.cpp:1206` |
| FastArraySerializer가 배열 인덱스를 신뢰하지 않음 | ② 헤더 통독 | `FastArraySerializer.h:54,213,1193` |
| `UPrimaryDataAsset::GetPrimaryAssetId()` 기본 구현 | ① 앵커 추적 | `DataAsset.cpp` |
| `FPrimaryAssetType`이 `FName` 래퍼일 뿐임 | ① 앵커 추적 | `PrimaryAssetId.h` |
| AssetManager 타입 열거 API | ② 헤더 통독 | `AssetManager.h:262,265` / `AssetManagerTypes.h:131` |
| `IsDataValid`의 구버전 deprecated | ② 헤더 통독 | `Object.h:1100,1110` |
| `RemoveActiveGameplayEffect` authority 요구 | ⑤ 호출부/구현 확인 | `AbilitySystemComponent.cpp:1177` |

**공통점: 전부 소스에서 직접 확인했고, 인터넷 검색으로 얻은 것은 하나도 없다.**

---

## 안티패턴

| 하지 말 것 | 이유 |
|---|---|
| 이름을 모르는 채 Search Everywhere를 반복 | 모르는 이름은 검색할 수 없다. 앵커부터 잡는다 |
| 엔진 전체(`.cpp` 포함) 텍스트 검색 | 생성 코드·구현부 노이즈에 묻힌다. `Public/*.h`로 제한 |
| 블로그 코드를 그대로 복사 | 엔진 버전이 다르면 시그니처가 바뀌어 있다 |
| 심볼 하나만 보고 헤더를 닫기 | 같은 파일에 필요한 나머지가 다 들어 있다 |
| 헤더 주석을 안 읽고 동작을 추측 | Epic이 제약 조건을 주석에 적어두는 경우가 많다 |
