---
paths: ["EmploymentProj/Source/**"]
---

# UE C++ 관례 (이 프로젝트)

- 게임플레이 클래스에는 리플렉션 매크로를 붙인다 (`UCLASS`, `UPROPERTY`, `UFUNCTION`).
- 클라이언트 반응이 필요한 복제 변수는 `ReplicatedUsing` + `OnRep_`.
- RPC 접두사: `Server_`, `Client_`, `Multicast_`.
- `UActorComponent` 하위 클래스는 `HasAuthority()`가 아니라 `GetOwner()->HasAuthority()`.
- 헤더에서는 전방 선언, `#include`는 `.cpp`에서만.
- 무기 데이터는 `WeaponDef->`로 읽는다 (타입 `UEPWeaponDefinition`).
- NativeGameplayTags: `Public/GAS/EPNativeGameplayTags.h` (`namespace EmpGameplayTags`).
- 플랫폼: Windows (win32).

Claude는 이 폴더의 파일을 수정하지 않는다 — 구현 방법은 문서에 쓴다 (`CLAUDE.md` 작업 방식).
