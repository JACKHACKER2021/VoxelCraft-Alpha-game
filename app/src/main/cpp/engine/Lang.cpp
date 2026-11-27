#include "Lang.h"

#include <unordered_map>
#include "Blocks.h"
#include <vector>

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

namespace {

// 2.9.0 (on the way to 3.0.0): the Cornerstone Dimension is its own place: no "Nether" left in what the player
// reads. Whole phrases first, then the single words, per language.
std::string cornerstoneWords(std::string s) {
    if (!kFeatures290 || s.find("ether") == std::string::npos && s.find("езер") == std::string::npos) return s;
    static const std::vector<std::pair<std::string, std::string>> kCs = {
        {"Netherrackové schody", "Schody z úhelného kamene"}, {"Netherracková deska", "Deska z úhelného kamene"},
        {"Netherová křemenná ruda", "Úhelná křemenná ruda"}, {"Netherová tráva", "Úhelná tráva"}, {"Netherové výhonky", "Úhelné výhonky"},
        {"netherové pevnosti", "pevnosti úhelného kamene"}, {"nether cihel", "cihel úhelného kamene"}, {"Poutník Netheru", "Poutník úhelného kamene"},
        {"Vstup do Netheru", "Vstup do dimenze úhelného kamene"}, {"Ohnivý Nether", "Ohnivá dimenze úhelného kamene"},
        {"Nether z cihel", "Dimenze úhelného kamene z cihel"}, {"Netherperkov", "Úhelníkov"}, {"Netherrack", "Úhelný kámen"},
        {"Netherový", "Úhelný"}, {"Netherová", "Úhelná"}, {"Netherové", "Úhelné"}, {"Netheru", "dimenze úhelného kamene"},
        {"Nether", "Dimenze úhelného kamene"}, {"nether,", "úhelný kámen,"}};
    static const std::vector<std::pair<std::string, std::string>> kEn = {
        {"Entering the Nether", "Entering the Cornerstone Dimension"}, {"the Nether", "the Cornerstone Dimension"},
        {"Netherperk", "Cornerperk"}, {"Netherrack", "Cornerstone Rock"}, {"Nether quartz", "Cornerstone quartz"}, {"nether brick", "cornerstone brick"},
        {"Nether", "Cornerstone"}, {"nether,", "cornerstone,"}};
    static const std::vector<std::pair<std::string, std::string>> kOther = {{"Netherrack", "Cornerstone"}, {"Nether", "Cornerstone"}, {"Незер", "Краеугольный камень"}};
    const auto& list = g_language == Language::Czech ? kCs : g_language == Language::English ? kEn : kOther;
    for (const auto& [from, to] : list)
        for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size())) {
            // never inside a file name such as nether.json
            if (s.compare(p + from.size(), 5, ".json") == 0) break;
            s.replace(p, from.size(), to);
        }
    return s;
}

}  // namespace

std::string tr(const std::string& czech) {
    if (g_language == Language::Czech || czech.empty()) return cornerstoneWords(czech);
    auto it = index().find(czech);
    if (it == index().end()) return cornerstoneWords(czech);
    const char* t = it->second->text[(int)g_language];
    return cornerstoneWords(t && *t ? t : czech);
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

// 2.7.0: the same with the arguments in a vector (import reports read from a file).
std::string trf(const std::string& czech, const std::vector<std::string>& args) {
    std::string s = tr(czech);
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string mark = "{" + std::to_string(i) + "}";
        for (size_t p = s.find(mark); p != std::string::npos; p = s.find(mark, p + args[i].size())) s.replace(p, mark.size(), args[i]);
    }
    return s;
}

bool hasTranslation(const std::string& czech) { return index().count(czech) > 0; }
int translationCount() { return (int)index().size(); }

}  // namespace vc
