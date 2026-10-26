#include "Lang.h"

#include <unordered_map>

namespace vc {

namespace {

struct Entry {
    const char* text[(int)Language::Count];  // en, cs, es, ru, de
};

// {English, Czech, Spanish, Russian, German}
const Entry kTable[] = {
#include "LangTable.inc"
};

Language g_language = Language::English;

const std::unordered_map<std::string, const Entry*>& index() {
    static const std::unordered_map<std::string, const Entry*> map = [] {
        std::unordered_map<std::string, const Entry*> m;
        for (const Entry& e : kTable) m.emplace(e.text[(int)Language::Czech], &e);
        return m;
    }();
    return map;
}

}  // namespace

void setLanguage(Language lang) { g_language = lang; }
Language language() { return g_language; }

const char* languageName(Language lang) {
    static const char* kNames[] = {"English", "Čeština", "Español", "Русский", "Deutsch"};
    return kNames[(int)lang];
}

const char* languageCode(Language lang) {
    static const char* kCodes[] = {"en", "cs", "es", "ru", "de"};
    return kCodes[(int)lang];
}

Language languageFromCode(const std::string& code) {
    for (int i = 0; i < (int)Language::Count; ++i)
        if (code == languageCode((Language)i)) return (Language)i;
    return Language::English;
}

std::string tr(const std::string& czech) {
    if (g_language == Language::Czech || czech.empty()) return czech;
    auto it = index().find(czech);
    if (it == index().end()) return czech;
    const char* t = it->second->text[(int)g_language];
    return t && *t ? t : czech;
}

std::string trf(const std::string& czech, std::initializer_list<std::string> args) {
    std::string s = tr(czech);
    int i = 0;
    for (const std::string& a : args) {
        const std::string mark = "{" + std::to_string(i++) + "}";
        for (size_t p = s.find(mark); p != std::string::npos; p = s.find(mark, p + a.size())) s.replace(p, mark.size(), a);
    }
    return s;
}

bool hasTranslation(const std::string& czech) { return index().count(czech) > 0; }
int translationCount() { return (int)index().size(); }

}  // namespace vc
