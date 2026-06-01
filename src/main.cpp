#include "sheet.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static const char* VERSION = "0.1.0";

/* ---- Config handling ---- */

struct Cheatpath {
    std::string name;
    std::string path;
    std::vector<std::string> tags;
    bool readOnly = false;
};

struct Config {
    std::vector<Cheatpath> cheatpaths;
    bool doColorize = false;
    std::string editor;
    std::string pager;
};

static std::string findConfig() {
    /* Search paths in order */
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    std::vector<std::string> paths = {
        std::string(home) + "/.config/cheat/conf.yml",
        std::string(home) + "/.cheat/conf.yml",
        "/etc/cheat/conf.yml",
    };
    for (auto& p : paths) {
        struct stat st;
        if (stat(p.c_str(), &st) == 0) return p;
    }
    return "";
}

static Config loadConfig(const std::string& confpath) {
    Config c;
    c.editor = getenv("EDITOR") ? getenv("EDITOR") : "vim";
    c.pager = getenv("PAGER") ? getenv("PAGER") : "less";

    if (confpath.empty()) return c;

    std::ifstream f(confpath);
    if (!f) return c;

    std::string line;
    Cheatpath* curCp = nullptr;

    while (std::getline(f, line)) {
        /* Trim */
        size_t s = 0;
        while (s < line.size() && (line[s] == ' ' || line[s] == '\t')) s++;
        if (s >= line.size() || line[s] == '#') continue;

        line = line.substr(s);
        if (line.back() == '\r') line.pop_back();

        /* Key: value */
        if (line.find("editor:") == 0) {
            c.editor = line.substr(7);
            s = 0; while (s < c.editor.size() && c.editor[s] == ' ') s++;
            c.editor = c.editor.substr(s);
        } else if (line.find("pager:") == 0) {
            c.pager = line.substr(6);
            s = 0; while (s < c.pager.size() && c.pager[s] == ' ') s++;
            c.pager = c.pager.substr(s);
        } else if (line.find("colorize:") == 0) {
            std::string val = line.substr(9);
            s = 0; while (s < val.size() && val[s] == ' ') s++;
            c.doColorize = (val.find("true") != std::string::npos);
        } else if (line.find("cheatpaths:") == 0) {
            continue; /* just a marker */
        } else if (line.find("- name:") == 0) {
            Cheatpath cp;
            cp.name = line.substr(7);
            s = 0; while (s < cp.name.size() && cp.name[s] == ' ') s++;
            cp.name = cp.name.substr(s);
            c.cheatpaths.push_back(cp);
            curCp = &c.cheatpaths.back();
        } else if (curCp && line.find("path:") == 0) {
            curCp->path = line.substr(5);
            s = 0; while (s < curCp->path.size() && curCp->path[s] == ' ') s++;
            curCp->path = curCp->path.substr(s);
            /* Expand ~ */
            if (!curCp->path.empty() && curCp->path[0] == '~') {
                const char* h = getenv("HOME");
                if (h) curCp->path = std::string(h) + curCp->path.substr(1);
            }
        } else if (curCp && line.find("tags:") == 0) {
            std::string t = line.substr(5);
            s = 0; while (s < t.size() && t[s] == ' ') s++;
            t = t.substr(s);
            /* Simple CSV */
            size_t pos = 0;
            while (pos < t.size()) {
                size_t comma = t.find(',', pos);
                std::string tag = (comma == std::string::npos) ? t.substr(pos) : t.substr(pos, comma - pos);
                size_t ts = 0; while (ts < tag.size() && tag[ts] == ' ') ts++;
                tag = tag.substr(ts);
                if (!tag.empty()) curCp->tags.push_back(tag);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        } else if (curCp && line.find("readonly:") == 0) {
            std::string val = line.substr(9);
            s = 0; while (s < val.size() && val[s] == ' ') s++;
            curCp->readOnly = val.find("true") != std::string::npos;
        }
    }

    /* Default cheatpath if none configured */
    if (c.cheatpaths.empty()) {
        const char* h = getenv("HOME");
        std::string home = h ? h : "/tmp";
        Cheatpath cp;
        cp.name = "personal";
        cp.path = home + "/.cheat";
        c.cheatpaths.push_back(cp);
    }

    return c;
}

/* ---- Command implementations ---- */

static void cmdView(std::vector<Sheet>& sheets, const std::string& name, bool colorize, const std::string& pager) {
    /* Find exact match */
    auto it = std::find_if(sheets.begin(), sheets.end(), [&](const Sheet& s) {
        return s.title == name;
    });
    if (it == sheets.end()) {
        printf("No cheatsheet found for '%s'.\n", name.c_str());
        exit(2);
    }

    std::string text = it->text;
    if (colorize && !it->syntax.empty()) {
        text = applyColor(text, it->syntax);
    }

    fputs(text.c_str(), stdout);
}

static void cmdList(const std::vector<Sheet>& sheets) {
    for (auto& s : sheets) {
        printf("%-30s %s\n", s.title.c_str(), s.path.c_str());
    }
}

static void cmdSearch(const std::vector<Sheet>& sheets, const std::string& phrase,
                      bool regex, bool colorize) {
    auto results = searchSheets(sheets, phrase, regex);
    if (results.empty()) {
        printf("No cheatsheets found matching '%s'.\n", phrase.c_str());
        return;
    }
    for (auto& s : results) {
        printf("--- %s ---\n", s.title.c_str());
        std::string text = s.text;
        if (colorize && !s.syntax.empty()) text = applyColor(text, s.syntax);
        /* Highlight the search term with ANSI reverse video */
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        std::string plower = phrase;
        std::transform(plower.begin(), plower.end(), plower.begin(), ::tolower);
        size_t pos = 0;
        std::string out;
        while (pos < text.size()) {
            size_t found = lower.find(plower, pos);
            if (found == std::string::npos) {
                out += text.substr(pos);
                break;
            }
            out += text.substr(pos, found - pos);
            out += "\033[7m" + text.substr(found, phrase.size()) + "\033[0m";
            pos = found + phrase.size();
        }
        fputs(out.c_str(), stdout);
        printf("\n");
    }
}

static void cmdEdit(const std::string& name, const std::string& editor,
                    const std::string& basePath) {
    std::string path = basePath + "/" + name;
    /* Create parent directory if needed */
    size_t slash = path.rfind('/');
    if (slash != std::string::npos) {
        std::string dir = path.substr(0, slash);
        /* mkdir -p */
        std::string cmd = "mkdir -p \"" + dir + "\"";
        system(cmd.c_str());
    }
    /* Launch editor */
    std::string cmd = editor + " \"" + path + "\"";
    system(cmd.c_str());
}

static void cmdTags(const std::vector<Sheet>& sheets) {
    std::unordered_map<std::string, int> tagCount;
    for (auto& s : sheets) {
        for (auto& t : s.tags) tagCount[t]++;
    }
    std::vector<std::pair<std::string, int>> tags(tagCount.begin(), tagCount.end());
    std::sort(tags.begin(), tags.end());
    for (auto& [tag, count] : tags) {
        printf("%-20s %d\n", tag.c_str(), count);
    }
}

static void cmdDirs(const Config& conf) {
    for (auto& cp : conf.cheatpaths) {
        printf("%-20s %s\n", cp.name.c_str(), cp.path.c_str());
    }
}

static void cmdConf(const std::string& path) {
    if (path.empty()) {
        printf("No config file found.\n");
    } else {
        printf("%s\n", path.c_str());
    }
}

static void cmdRemove(const std::string& name, const std::string& basePath) {
    std::string path = basePath + "/" + name;
    if (unlink(path.c_str()) == 0) {
        printf("Removed '%s'.\n", name.c_str());
    } else {
        printf("Failed to remove '%s': %s\n", name.c_str(), strerror(errno));
    }
}

static void cmdInit() {
    printf("# cheat config file\n");
    printf("---\n");
    printf("editor: vim\n");
    printf("pager: less -R\n");
    printf("colorize: true\n");
    printf("cheatpaths:\n");
    printf("  - name: community\n");
    printf("    path: ~/.cheat/community\n");
    printf("    tags: [ community ]\n");
    printf("    readonly: true\n");
    printf("  - name: personal\n");
    printf("    path: ~/.cheat\n");
    printf("    tags: [ personal ]\n");
    printf("    readonly: false\n");
}

static void printUsage() {
    fprintf(stderr,
        "cheatcpp - Create and view interactive cheatsheets (C++ port of cheat)\n"
        "Usage: cheatcpp [flags] [cheatsheet]\n\n"
        "Flags:\n"
        "  <cheatsheet>       View the specified cheatsheet\n"
        "  -l, --list         List all available cheatsheets\n"
        "  -b, --brief        List cheatsheet titles only\n"
        "  -s, --search STR   Search cheatsheets for phrase\n"
        "  -r, --regex        Treat search phrase as regex\n"
        "  -t, --tag TAG      Filter by tag (comma-separated)\n"
        "  -T, --tags         List all tags in use\n"
        "  -e, --edit SHEET   Edit (or create) a cheatsheet\n"
        "  -p, --path NAME    Filter by cheatpath name\n"
        "  -d, --directories  List cheatsheet directories\n"
        "  --rm SHEET         Remove (delete) a cheatsheet\n"
        "  -c, --colorize     Apply syntax highlighting\n"
        "  --init             Print a default config file\n"
        "  --conf             Print the config file path\n"
        "  -v, --version      Print version\n"
        "  -h, --help         Show this help\n");
}

int main(int argc, char* argv[]) {
    bool flagList = false, flagBrief = false, flagTags = false, flagDirs = false;
    bool flagCol = false, flagRegex = false, flagInit = false, flagConf = false;
    bool flagVersion = false;
    std::string flagSearch, flagTag, flagEdit, flagPath, flagRemove;
    std::string cheatsheet; /* positional arg */

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { printUsage(); return 0; }
        else if (a == "-v" || a == "--version") flagVersion = true;
        else if (a == "-l" || a == "--list") flagList = true;
        else if (a == "-b" || a == "--brief") flagBrief = true;
        else if (a == "-T" || a == "--tags") flagTags = true;
        else if (a == "-d" || a == "--directories") flagDirs = true;
        else if (a == "-c" || a == "--colorize") flagCol = true;
        else if (a == "-r" || a == "--regex") flagRegex = true;
        else if (a == "--init") flagInit = true;
        else if (a == "--conf") flagConf = true;
        else if ((a == "-s" || a == "--search") && i + 1 < argc) flagSearch = argv[++i];
        else if ((a == "-t" || a == "--tag") && i + 1 < argc) flagTag = argv[++i];
        else if ((a == "-e" || a == "--edit") && i + 1 < argc) flagEdit = argv[++i];
        else if ((a == "-p" || a == "--path") && i + 1 < argc) flagPath = argv[++i];
        else if (a == "--rm" && i + 1 < argc) flagRemove = argv[++i];
        else if (a[0] != '-') cheatsheet = a;
        else { fprintf(stderr, "Unknown flag: %s\n", a.c_str()); return 1; }
    }

    if (flagVersion) { printf("cheatcpp %s\n", VERSION); return 0; }
    if (flagInit) { cmdInit(); return 0; }

    /* Load config */
    std::string confpath = findConfig();
    if (flagConf) { cmdConf(confpath); return 0; }

    Config conf = loadConfig(confpath);

    /* Filter by cheatpath name */
    if (!flagPath.empty()) {
        conf.cheatpaths.erase(
            std::remove_if(conf.cheatpaths.begin(), conf.cheatpaths.end(),
                [&](const Cheatpath& cp) { return cp.name != flagPath; }),
            conf.cheatpaths.end());
        if (conf.cheatpaths.empty()) {
            fprintf(stderr, "No cheatpath named '%s'.\n", flagPath.c_str());
            return 1;
        }
    }

    /* Load sheets from all cheatpaths */
    std::vector<std::vector<Sheet>> allSheets;
    for (auto& cp : conf.cheatpaths) {
        allSheets.push_back(loadSheetsFromDir(cp.path, cp.name, cp.readOnly, cp.tags));
    }

    /* Consolidate: later paths override earlier */
    std::unordered_map<std::string, Sheet> consolidated;
    for (auto& pathSheets : allSheets) {
        for (auto& s : pathSheets) {
            consolidated[s.title] = s;
        }
    }

    /* Build flat list sorted by title */
    std::vector<Sheet> sheets;
    for (auto& [title, s] : consolidated) sheets.push_back(s);
    std::sort(sheets.begin(), sheets.end(), [](const Sheet& a, const Sheet& b) {
        return a.title < b.title;
    });

    /* Apply tag filter if specified */
    if (!flagTag.empty()) {
        std::vector<std::string> tags;
        size_t pos = 0;
        while (pos < flagTag.size()) {
            size_t comma = flagTag.find(',', pos);
            std::string tag = (comma == std::string::npos) ? flagTag.substr(pos) : flagTag.substr(pos, comma - pos);
            size_t ts = 0; while (ts < tag.size() && tag[ts] == ' ') ts++;
            tag = tag.substr(ts);
            if (!tag.empty()) tags.push_back(tag);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        sheets = filterByTags(sheets, tags);
    }

    /* Dispatch command */
    if (flagDirs) {
        cmdDirs(conf);
    } else if (flagTags) {
        cmdTags(sheets);
    } else if (!flagEdit.empty()) {
        /* Edit: use the first writable cheatpath */
        std::string basePath;
        for (auto& cp : conf.cheatpaths) {
            if (!cp.readOnly) { basePath = cp.path; break; }
        }
        if (basePath.empty()) {
            fprintf(stderr, "No writable cheatpath configured.\n");
            return 1;
        }
        cmdEdit(flagEdit, conf.editor, basePath);
    } else if (!flagRemove.empty()) {
        std::string basePath;
        for (auto& cp : conf.cheatpaths) {
            if (!cp.readOnly) { basePath = cp.path; break; }
        }
        if (basePath.empty()) {
            fprintf(stderr, "No writable cheatpath configured.\n");
            return 1;
        }
        cmdRemove(flagRemove, basePath);
        /* Reload after removal */
        sheets.clear();
        for (auto& cp : conf.cheatpaths) {
            auto loaded = loadSheetsFromDir(cp.path, cp.name, cp.readOnly, cp.tags);
            for (auto& s : loaded) sheets.push_back(s);
        }
    } else if (!flagSearch.empty()) {
        cmdSearch(sheets, flagSearch, flagRegex, flagCol || conf.doColorize);
    } else if (flagList || flagBrief) {
        if (flagBrief) {
            for (auto& s : sheets) printf("%s\n", s.title.c_str());
        } else {
            cmdList(sheets);
        }
    } else if (!cheatsheet.empty()) {
        cmdView(sheets, cheatsheet, flagCol || conf.doColorize, conf.pager);
    } else {
        printUsage();
        return 0;
    }

    return 0;
}
