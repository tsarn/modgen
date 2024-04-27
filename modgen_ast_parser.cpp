#include <clang-c/Index.h>
#include <iostream>
#include <set>
#include <format>
#include <vector>
#include <ranges>
#include <cstdlib>
#include <map>
#include <optional>
#include <regex>
#include <fstream>

struct Arguments {
    std::string argv0;

    std::string astFile;
    std::string outputFile;
    std::vector<std::string> namespaces;
    std::optional<std::regex> filter;
    std::optional<std::regex> exclude;

    bool onlyNames = false;
};

static Arguments arguments;

static void usage() {
    std::cerr << std::format("Usage: {} [options] <ast file>\n", arguments.argv0);
    std::cerr << "\n";
    std::cerr << "Options:\n";
    std::cerr << "  -o <path>              Write output to the specified file\n";
    std::cerr << "  -n <namespaces>        Specify the semicolon separated namespaces to export\n";
    std::cerr << "  -f <regex>             Only export the names matching this regex\n";
    std::cerr << "  -e <regex>             Do not export the names matching this regex\n";
    std::cerr << "  -p                     Instead of generating a module, simply output the list of names\n";
    exit(1);
}

static void parseArgs(int argc, char **argv) {
    arguments.argv0 = argv[0] ? argv[0] : "";
    int curArg = 1;
    auto hasArg = [&] {
        return curArg < argc;
    };
    auto nextArg = [&] {
        if (!hasArg()) {
            usage();
        }
        return std::string_view{argv[curArg++]};
    };

    while (hasArg()) {
        auto arg = nextArg();
        if ((arg == "-" || !arg.starts_with("-")) && arguments.astFile.empty()) {
            arguments.astFile = arg;
        } else if (arg == "-o") {
            arguments.outputFile = nextArg();
        } else if (arg == "-n") {
            auto namespaces = nextArg();
            for (const auto& ns : std::views::split(namespaces, std::string_view{";"})) {
                arguments.namespaces.emplace_back(std::string_view{ns});
            }
        } else if (arg == "-f") {
            arguments.filter = std::regex{std::string{nextArg()}};
        } else if (arg == "-e") {
            arguments.exclude = std::regex{std::string{nextArg()}};
        } else if (arg == "-p") {
            arguments.onlyNames = true;
        } else {
            usage();
        }
    }

    if (arguments.astFile.empty()) {
        usage();
    }

    if (!arguments.filter) {
        if (!arguments.namespaces.empty()) {
            std::string filterString;
            for (const auto &ns: arguments.namespaces) {
                if (!filterString.empty()) {
                    filterString += "|";
                }
                filterString += std::format("({})", ns);
            }
            arguments.filter = std::regex{std::format("^({})(::.*)?$", filterString)};
        }

        if (!arguments.exclude) {
            arguments.exclude = std::regex{"^(.*::)?(_[_A-Z]|_*detail).*$"};
        }
    }
}

static CXIndex index;
static CXTranslationUnit translationUnit;
static std::set<std::string> exportedNames;
static int depth = 0;

static auto fromClangString(CXString s) {
    std::string owned_string{clang_getCString(s)};
    clang_disposeString(s);
    return owned_string;
}

static auto getFullyQualifiedName(CXCursor cursor) {
    std::vector<std::string> path;
    for (auto cur = cursor;; cur = clang_getCursorLexicalParent(cur)) {
        auto kind = clang_getCursorKind(cur);
        if (clang_isInvalid(kind) || kind == CXCursor_TranslationUnit) {
            break;
        }

        if (clang_Cursor_isInlineNamespace(cur)) {
            continue;
        }

        auto name = fromClangString(clang_getCursorSpelling(cur));
        if (!name.empty()) {
            path.emplace_back(std::move(name));
        }
    }

    std::string fqn;

    for (const auto &part: std::ranges::reverse_view(path)) {
        if (!fqn.empty()) {
            fqn += "::";
        }
        fqn += part;
    }

    return fqn;
}

static auto visitor(CXCursor cursor, CXCursor parent, CXClientData clientData) -> CXChildVisitResult {
    CXCursorKind kind = clang_getCursorKind(cursor);
    bool willRecurse = false;
    bool willEmitName = false;
    bool isAnonymous = clang_Cursor_isAnonymous(cursor);
    auto name = fromClangString(clang_getCursorSpelling(cursor));
    auto fqn = getFullyQualifiedName(cursor);
    bool isOutOfLine = false;
    auto linkage = clang_getCursorLinkage(cursor);

    if (kind == CXCursor_TranslationUnit || kind == CXCursor_Namespace ||
            (kind == CXCursor_UnexposedDecl && name.empty())) {
        willRecurse = true;
    } else {
        auto semanticParent = clang_getCursorSemanticParent(cursor);
        auto spKind = clang_getCursorKind(semanticParent);
        isOutOfLine = spKind != CXCursor_TranslationUnit && spKind != CXCursor_Namespace && spKind != CXCursor_UnexposedDecl;
    }

    if (kind == CXCursor_FunctionDecl || kind == CXCursor_VarDecl || kind == CXCursor_FunctionTemplate) {
        if (linkage == CXLinkage_External) {
            willEmitName = true;
        }
    }

    if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl || kind == CXCursor_EnumDecl ||
        kind == CXCursor_UnionDecl || kind == CXCursor_UsingDeclaration || kind == CXCursor_TypedefDecl ||
        kind == CXCursor_TypeAliasDecl || kind == CXCursor_ClassTemplate ||
        kind == CXCursor_ClassTemplatePartialSpecialization || kind == CXCursor_TypeAliasTemplateDecl ||
        kind == CXCursor_ConceptDecl || (kind == CXCursor_UnexposedDecl && !name.empty())) {
        willEmitName = true;
    }

    if (isAnonymous || isOutOfLine || fqn.contains(' ')) {
        willEmitName = false;
    }

    if (willEmitName) {
        bool matchesFilter = !arguments.filter || std::regex_match(fqn, *arguments.filter);
        bool matchesExclude = arguments.exclude && std::regex_match(fqn, *arguments.exclude);

        if (matchesFilter && !matchesExclude) {
            exportedNames.emplace(fqn);
        }
    }

    if (willRecurse) {
        depth++;
        clang_visitChildren(cursor, visitor, clientData);
        depth--;
    }

    return CXChildVisit_Continue;
}

static void doProcess() {
    index = clang_createIndex(
            /* excludeDeclarationsFromPCH = */ false,
            /* displayDiagnostics = */ true);
    translationUnit = clang_createTranslationUnit(index, arguments.astFile.c_str());
    if (!translationUnit) {
        std::cerr << "Failed to parse AST\n";
        exit(1);
    }

    CXCursor cursor = clang_getTranslationUnitCursor(translationUnit);
    clang_visitChildren(cursor, visitor, nullptr);
}

static void outputNames(auto&& stream) {
    for (const auto& name : exportedNames) {
        stream << name << std::endl;
    }
}

static void outputExports(auto&& stream) {
    std::map<std::string, std::vector<std::string>> namespaces;
    for (const auto& name : exportedNames) {
        auto idx = name.rfind("::");
        if (idx == std::string::npos) {
            namespaces[""].emplace_back(name);
        } else {
            namespaces[name.substr(0, idx)].emplace_back(name);
        }
    }

    for (const auto& [ns, names] : namespaces) {
        if (!ns.empty()) {
            stream << std::format("export namespace {} {{\n", ns);
        }

        for (const auto& name : names) {
            if (!ns.empty()) {
                stream << "  ";
            } else {
                stream << "export ";
            }
            stream << std::format("using ::{};\n", name);
        }

        if (!ns.empty()) {
            stream << "}\n";
        }
    }
}

static void outputResults(auto&& stream) {
    if (arguments.onlyNames) {
        outputNames(stream);
    } else {
        outputExports(stream);
    }

}

auto main(int argc, char **argv) -> int {
    parseArgs(argc, argv);
    doProcess();

    if (arguments.outputFile.empty() || arguments.outputFile == "-") {
        outputResults(std::cout);
    } else {
        std::ofstream output{arguments.outputFile};
        outputResults(output);
    }

    return 0;
}
