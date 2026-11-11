# Focus Reading

Focus Reading emphasizes the beginning of eligible English words and Korean
eojeol. It is a visual reading aid; this feature does not promise a measurable
increase in reading speed. EPUB and TXT use the reader layout path, and source
books are not modified with emphasis markup.

## Controls

Turn **Focus Reading** on or off in Reader Settings or the reader Text panel.
Choose **Focus Reading Intensity** in Text Settings or the Text panel. The
existing on/off preference is retained, and an unset intensity defaults to
100%.

| Displayed intensity | Actual prefix ratio |
| --- | --- |
| 0% | 20% |
| 20% | 25% |
| 40% | 30% |
| 60% | 35% |
| 80% | 40% |
| 100% | 45% |

**0% is not OFF.** Only the separate toggle disables automatic emphasis.
**100% does not bold the whole word.** Prefix length is limited to nine Unicode
codepoints, with at least one codepoint emphasized for an eligible word.

Korean emphasis is calculated once from the original eojeol before line-wrap
splitting; wrapped fragments carry that original boundary. The Korean ratio is
rounded up. English at the default 100% setting retains its legacy 45% floor
calculation. If a different intensity is selected, English uses that selected
ratio and rounds up. Long words may receive less than the selected ratio due to
the nine-codepoint limit.

## Fonts and styling

- Use a font family with both Regular and Bold faces for visible emphasis. A
  `.cpfont` family can carry both styles. If the selected font has no Bold face,
  the device may fall back to Regular, making the emphasis difficult to see.
- Original Bold text is not emphasized again. Italic, underline/decoration,
  superscript/subscript, small caps, inline size changes, and Ruby bases/RT keep
  their source styling; protected spans are skipped when emphasis cannot be
  added without changing that styling.
- Words fragmented by inline styles or parser input limits are conservatively
  skipped instead of restarting emphasis in each fragment.
- Other CJK text and words handled by the earlier CJK line-break path remain
  unaccented. The word rules do not implement complete Unicode segmentation.

## Caches and validation

Settings are per device. Toggling Focus Reading or changing its active intensity
invalidates affected EPUB section caches. TXT conversion does not insert markup
into the source text. The rendered result depends on the installed fonts and
requires X3/X4 device validation; host tests and firmware builds alone do not
establish display quality, heap use, or page-turn latency.
