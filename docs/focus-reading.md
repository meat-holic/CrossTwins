# Bionic Reading experiment

Bionic Reading emphasizes the beginning of eligible English and Korean words.
This is an experimental reading preference, not a proven reading-speed improvement.
EPUB and TXT/Markdown use the same layout engine. Source books and TXT-to-HTML
conversion output are not modified with emphasis markup.

## Controls

Enable **Bionic Reading** in the reader settings or the reader Text panel.
Choose **Emphasis strength** separately in Text Settings > Style or the Text panel.
The existing on/off preference is retained. A missing strength setting defaults to 100%.

| Displayed strength | Actual prefix ratio |
| --- | --- |
| 0% | 20% |
| 20% | 25% |
| 40% | 30% |
| 60% | 35% |
| 80% | 40% |
| 100% | 45% |

**0% is not OFF.** Only the independent toggle disables automatic emphasis.
**100% does not bold the whole word.** The prefix is capped at nine codepoints.
Korean words use a rounded-up prefix at every strength, with a minimum of one
codepoint. English keeps the original 45% floor calculation at the default 100%
stage, including its minimum of one and maximum of nine. English stages from 0%
through 80% use the selected ratio with rounding up. Long words can receive less
than the selected ratio because of the nine-codepoint cap.

## Style protection and limits

- Original Bold, Italic, underline/decoration, superscript/subscript, small caps,
  inline size changes and Ruby bases/RT retain their original styling without
  added emphasis. No meaning is inferred from the author's markup.
- Words fragmented by inline style changes or the parser's bounded input buffer
  are conservatively skipped rather than restarting emphasis in each fragment.
- A complete Korean word gets one prefix; wrapping distributes that prefix
  between lines without recalculating the ratio. Plain Korean/English mixed words
  are supported. Other CJK text, and words handled by the earlier CJK break path,
  remain unaccented in this experiment.
- Existing token rules still separate punctuation/numbers from letters. This is
  not a complete implementation of Unicode word segmentation or LibraTool parity.
- Settings are per-device. Toggling or changing active strength rebuilds affected
  section caches, including partial caches; page boundaries may change. Back up
  reading progress before testing experimental firmware.
- Display quality, actual heap use and page-turn latency require X3/X4 hardware
  validation with the installed SD fonts. A successful build or host test does
  not establish hardware correctness.

The earlier Focus Reading screenshots are not validation of this experiment.
