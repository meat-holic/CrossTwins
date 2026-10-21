# CT 미채택 upstream 계약의 보존 테스트

여기의 `.inc`는 root Host 등록 대상이 아니다. 원문 assertion은 그대로 보존했으며 CT 생산 코드에 synthetic glyph metrics, clipping SourceRange, upstream CSS 구조를 추가하지 않는다. CT 기존 테스트는 원래 assertion 그대로 활성 경로에 유지한다. 분리 근거와 원문 body 해시는 Slice 10A-Fix 보고 자료의 `host-classification.json`을 따른다.


## Slice 10B-Fix 감사 반영

원문 36개는 이력 보존을 위해 유지한다. CSS `FontInheritanceCacheRejectsOldVersion`의 공통 버전 거부 assertion은 active `CssParserTest.CacheRejectsOldVersion`으로 복원했다. 사전 `PageCountLimitAlsoStopsParsing`의 CT cap64 실패·empty·cleanup은 active `DictionaryHtmlContract.PageCountCapDiscardsOutputAndTemporaryFileAtNormalHeap`으로 복원했다. 문단 조기 중단 기대는 계속 미채택이다. EmDash 1건은 token 표현 차이, 3건은 기존 고정 폭 Host fixture의 줄 표시 차이이며 기존 CT 한계/미채택 개선으로 분류한다. 실제 기기 시각 품질은 미검증이며 한국어 golden은 변경하지 않는다.
