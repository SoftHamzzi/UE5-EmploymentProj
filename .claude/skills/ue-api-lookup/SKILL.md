---
name: ue-api-lookup
description: Look up UE5 API class/function documentation
disable-model-invocation: true
argument-hint: <ClassName or query>
---

## API Reference
!`python .claude/scripts/fetch-ue-api.py $ARGUMENTS`

위 검색 결과를 기반으로 해당 UE5 클래스/API에 대해 한국어로 정리해주세요.
영어 기술 용어(클래스명, 함수명, 매크로 등)는 그대로 유지합니다.

정리 형식:
1. **개요**: 클래스/기능의 목적 1-2문장
2. **주요 멤버**: 핵심 함수/프로퍼티 목록과 간단한 설명
3. **사용 패턴**: 일반적인 사용 방법이나 코드 예시
4. **관련 클래스**: 함께 사용되는 클래스 참조
