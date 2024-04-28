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
    std::cerr << "  -n <namespaces>        Specify the comma separated list of namespaces to export\n";
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
            for (const auto& ns : std::views::split(namespaces, std::string_view{","})) {
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
            arguments.exclude = std::regex{"^(.*::)?(_[_a-zA-Z]|_*detail).*$"};
        }
    }
}

static CXIndex index;
static CXTranslationUnit translationUnit;
static std::set<std::string> exportedNames;
static std::map<std::string, std::string> namespaceAliases;
static int depth = 0;
static std::string currentNamespaceAlias;

static auto fromClangString(CXString s) {
    std::string owned_string{clang_getCString(s)};
    clang_disposeString(s);
    return owned_string;
}

static auto getFullyQualifiedName(CXCursor cursor, bool lexical = true) {
    std::vector<std::string> path;
    for (auto cur = cursor;; cur = (lexical ? clang_getCursorLexicalParent(cur) : clang_getCursorSemanticParent(cur))) {
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
    auto kind = clang_getCursorKind(cursor);
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

    // for (int i = 0; i < depth; ++i) {
    //     std::cerr << "    ";
    // }
    // std::cerr << fqn << " " << fromClangString(clang_getCursorKindSpelling(kind)) << std::endl;

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

    if (kind == CXCursor_NamespaceAlias) {
        // oh my god, somebody please tell me there's a better way to do this
        // for some reason, NamespaceRef doesn't want to give me its fqn otherwise
        auto policy = clang_getCursorPrintingPolicy(cursor);
        clang_PrintingPolicy_setProperty(policy, CXPrintingPolicy_FullyQualifiedName, 1);
        auto sourceNamespace = fromClangString(clang_getCursorPrettyPrinted(cursor, policy));
        auto spacePos = sourceNamespace.rfind(' ');
        if (spacePos != std::string::npos) {
            sourceNamespace = sourceNamespace.substr(spacePos + 1);
        }
        clang_PrintingPolicy_dispose(policy);
        namespaceAliases[fqn] = sourceNamespace;
        willEmitName = true;
    }

    if (isAnonymous || isOutOfLine || fqn.contains(' ')) {
        willEmitName = false;
    }

    if (willEmitName) {
        exportedNames.emplace(fqn);
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

struct ExportedName {
    std::string name;
    std::string alias;
};

static void outputExports(auto&& stream) {
    std::map<std::string, std::vector<ExportedName>> namespaces;

    for (const auto& fqn : exportedNames) {
        bool matchesFilter = !arguments.filter || std::regex_match(fqn, *arguments.filter);
        bool matchesExclude = arguments.exclude && std::regex_match(fqn, *arguments.exclude);
        if (!matchesFilter) {
            for (const auto& alias : namespaceAliases) {
                if (std::regex_match(alias.first, *arguments.filter)) {
                    std::string prefix = alias.second + "::";
                    if (fqn.starts_with(prefix)) {
                        matchesFilter = true;
                        break;
                    }
                }
            }
        }

        if (matchesFilter && !matchesExclude) {
            auto idx = fqn.rfind("::");
            std::string alias;
            if (namespaceAliases.contains(fqn)) {
                alias = namespaceAliases.at(fqn);
            }
            if (idx == std::string::npos) {
                namespaces[""].push_back(ExportedName{fqn, alias});
            } else {
                namespaces[fqn.substr(0, idx)].push_back(ExportedName{fqn.substr(idx + 2), alias});
            }
        }
    }

    // for (const auto& [alias_ns, ns] : namespaceAliases) {
    //     if (!namespaces.contains(ns)) {
    //         continue;
    //     }

    //     for (const auto& name : namespaces.at(ns)) {
    //         namespaces[alias_ns].emplace(name);
    //     }
    // }

    for (const auto& [ns, names] : namespaces) {
        bool generated_export_namespace = false;

        for (const auto& name : names) {
            auto fqn = (ns == "") ? name.name : (ns + "::" + name.name);
            if (!generated_export_namespace && ns != "") {
                stream << std::format("export namespace {} {{\n", ns);
                generated_export_namespace = true;
            }
            if (!ns.empty()) {
                stream << "  ";
            } else {
                stream << "export ";
            }

            if (name.alias == "") {
                stream << std::format("using ::{};\n", fqn);
            } else {
                stream << std::format("namespace {} = {};\n", name.name, name.alias);
            }
        }

        if (generated_export_namespace) {
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
