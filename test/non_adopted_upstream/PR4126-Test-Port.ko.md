# PR #4126 테스트 이식 결정

- upstream: https://github.com/crosspoint-reader/crosspoint-reader/pull/4126
- 원문 head: `c69984af65e0323bf1b3b2b0c29fcce5bdce4223`, merge: `3e2af48786d10609503f8b54b2117bdb30a9358f`.
- CT 비교 기준: `482861c860549415d77a188696b5da23923137e0`.
- 사용자가 TEST_PORT_POLICY를 명시적으로 승인했습니다. 목적은 staging allocation 제거이며, 명시적 하이픈 토큰화 기능은 이식하지 않습니다.
- 네 원문 전체는 `Pr4126KoreanLineBreakingTest.inc`에 보존합니다. 앞선 실패 로그는 외부 보고 위치의 `rc0-final-before-test.log`와 `rc0-final-cjk-test.log`에 남아 있습니다. 실패를 skip/disabled 처리하거나 기존 CT assertion을 완화하지 않습니다.

| 원문 테스트 | CT 처리 | 근거 및 활성 대체 검증 |
|---|---|---|
| MixedScriptsKeepBoundariesTouchingHangulTogether | 그대로 활성 | 기존 CT 토큰 및 오프셋 동일 |
| ExplicitHyphensBeforeFirstCjkCharacterRemainBreaks | 원문 보존, CT 테스트로 대체 | upstream은 `ab-`, `cd가-`, `나中`이나 CT는 `ab-cd가-나中` 한 토큰. CtExplicitHyphensPreserveSlice12Tokenization이 실제 ParsedText/레이아웃의 토큰·x좌표를 고정하며, FourUpstreamInputsKeepSlice12Offsets가 원래 오프셋 빈 배열을 양쪽 구현에서 확인 |
| LatinHyphenRunsKeepTheNonCjkPath | 원문 보존, CT 테스트로 대체 | upstream은 `ab--`, `cd`이나 CT는 `ab--cd` 한 토큰. CtLatinHyphenRunsPreserveSlice12Tokenization과 오프셋 빈 배열 검증으로 보존 |
| CombiningMarksAndCjkPunctuationStayAttached | 그대로 활성 | 기존 CT 결합 문자·문장부호 계약 동일 |

오프셋 differential harness는 실제 생산 scanner/predicate와 고정된 Slice12 scanner/predicate를 비교합니다. 동일 helper를 양쪽에서 공유하여 의미 변경이 가려지지 않도록 baseline predicate도 고정했습니다. 네 입력의 명시적 기대 오프셋은 각각 `[2,14,16]`, `[]`, `[]`, `[9,14]`입니다. 추가 조합·malformed UTF-8 비교와 기존 한국어 ON/OFF·금칙·layout 회귀도 유지합니다. Host renderer는 고정 glyph 폭 fixture이며 실제 디바이스 화면/heap 측정을 대신하지 않습니다.
