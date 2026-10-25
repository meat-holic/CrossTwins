#include <I18n.h>
#include <gtest/gtest.h>

TEST(I18nLocaleFallbackTest, SupportsEnglishAndKoreanStrings) {
  I18N.setLanguage(Language::EN);
  EXPECT_STREQ("≫Read", I18N.get(StrId::STR_RESUME));
  EXPECT_STREQ("Settings", I18N.get(StrId::STR_SETTINGS_TITLE));

  I18N.setLanguage(Language::KO);
  EXPECT_STREQ("이어읽기", I18N.get(StrId::STR_RESUME));
  EXPECT_STREQ("설정", I18N.get(StrId::STR_SETTINGS_TITLE));
  I18N.setLanguage(Language::EN);
}

TEST(I18nLocaleFallbackTest, RemovedAndUnknownSavedCodesFallBackToEnglish) {
  EXPECT_EQ(Language::EN, I18n::languageFromCode("FR"));
  EXPECT_EQ(Language::EN, I18n::languageFromCode("DE"));
  EXPECT_EQ(Language::EN, I18n::languageFromCode("not-a-locale"));
  EXPECT_EQ(Language::EN, I18n::languageFromCode(nullptr));

  for (uint8_t i = 0; i < getLanguageCount(); ++i) {
    const auto language = static_cast<Language>(i);
    if (!isLanguageBuiltIn(language)) {
      EXPECT_EQ(Language::EN, I18n::languageFromCode(LANGUAGE_CODES[i]));
    }
  }

  I18N.setLanguage(Language::FR);
  EXPECT_EQ(Language::EN, I18N.getLanguage());
  EXPECT_STREQ("Settings", I18N.get(StrId::STR_SETTINGS_TITLE));
  I18N.setLanguage(Language::EN);
}

TEST(I18nLocaleFallbackTest, RemovedLocaleCharacterSetsFallBackToEnglish) {
  EXPECT_STREQ(I18n::getCharacterSet(Language::EN), I18n::getCharacterSet(Language::FR));
  EXPECT_STREQ(I18n::getCharacterSet(Language::EN), I18n::getCharacterSet(static_cast<Language>(255)));
}

TEST(I18nLocaleFallbackTest, EveryGeneratedStringIndexIsReadableInBothLocales) {
  for (uint16_t i = 0; i < static_cast<uint16_t>(StrId::_COUNT); ++i) {
    const auto key = static_cast<StrId>(i);
    I18N.setLanguage(Language::EN);
    ASSERT_NE(nullptr, I18N.get(key)) << "English string index " << i;
    I18N.setLanguage(Language::KO);
    ASSERT_NE(nullptr, I18N.get(key)) << "Korean string index " << i;
  }
  I18N.setLanguage(Language::EN);
}
