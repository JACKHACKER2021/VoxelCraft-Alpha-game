#pragma once
#include <initializer_list>
#include <string>

namespace vc {

// Languages of the game. English is the main one; the texts in the source code are
// Czech and serve as keys into the translation table (LangTable.inc).
enum class Language { English, Czech, Spanish, Russian, German, Count };

void setLanguage(Language lang);
Language language();
const char* languageName(Language lang);  // in that language: "English", "Čeština", ...
const char* languageCode(Language lang);  // "en", "cs", "es", "ru", "de"
Language languageFromCode(const std::string& code);  // English when unknown

// The text in the current language; texts without a translation come back unchanged
// (player names, world names, numbers).
std::string tr(const std::string& czech);
// Like tr, then {0}, {1}, ... are replaced by `args`, so every language can put them
// where its grammar wants them.
std::string trf(const std::string& czech, std::initializer_list<std::string> args);

// For tests: whether `czech` has an entry, and how many entries there are.
bool hasTranslation(const std::string& czech);
int translationCount();

}  // namespace vc
