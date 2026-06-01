#include "sheet.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <regex>
#include <dirent.h>
#include <sys/stat.h>

/* Parse YAML frontmatter: returns {tags, syntax, body}. Frontmatter starts/ends with --- */
static void parseFrontmatter(const std::string& raw, std::vector<std::string>& tags,
                              std::string& syntax, std::string& body) {
    /* Detect linebreak */
    const char* lb = "\n";
    if (raw.find("\r\n") != std::string::npos) lb = "\r\n";

    std::string delim = std::string("---") + lb;

    /* If no frontmatter, return raw as body */
    if (raw.compare(0, delim.size(), delim) != 0) {
        body = raw;
        return;
    }

    /* Split: delim | frontmatter | delim | body */
    size_t firstEnd = raw.find(delim, delim.size());
    if (firstEnd == std::string::npos) { body = raw; return; }

    std::string fm = raw.substr(delim.size(), firstEnd - delim.size());
    body = raw.substr(firstEnd + delim.size());

    /* Parse YAML frontmatter (simplified: only tags: [...] and syntax: ...) */
    std::istringstream ss(fm);
    std::string line;
    while (std::getline(ss, line)) {
        /* Trim */
        size_t s = 0; while (s < line.size() && (line[s] == ' ' || line[s] == '\t')) s++;
        line = line.substr(s);

        if (line.find("tags:") == 0) {
            /* tags: [tag1, tag2] */
            size_t br = line.find('[');
            size_t er = line.find(']');
            if (br != std::string::npos && er != std::string::npos && er > br) {
                std::string tstr = line.substr(br + 1, er - br - 1);
                size_t pos = 0;
                while (pos < tstr.size()) {
                    size_t comma = tstr.find(',', pos);
                    std::string tag = (comma == std::string::npos) ? tstr.substr(pos) : tstr.substr(pos, comma - pos);
                    /* trim tag */
                    size_t ts = 0; while (ts < tag.size() && (tag[ts] == ' ' || tag[ts] == '\t')) ts++;
                    size_t te = tag.size(); while (te > ts && (tag[te-1] == ' ' || tag[te-1] == '\t')) te--;
                    tag = tag.substr(ts, te - ts);
                    if (!tag.empty()) tags.push_back(tag);
                    if (comma == std::string::npos) break;
                    pos = comma + 1;
                }
            }
        } else if (line.find("syntax:") == 0) {
            size_t col = line.find(':');
            if (col != std::string::npos) {
                syntax = line.substr(col + 1);
                /* trim */
                size_t ts = 0; while (ts < syntax.size() && (syntax[ts] == ' ' || syntax[ts] == '\t')) ts++;
                syntax = syntax.substr(ts);
            }
        }
    }

    std::sort(tags.begin(), tags.end());
}

Sheet loadSheet(const std::string& path, const std::string& title,
                const std::string& cheatPathName, bool readOnly,
                const std::vector<std::string>& parentTags) {
    Sheet s;
    s.title = title;
    s.cheatPath = cheatPathName;
    s.path = path;
    s.readOnly = readOnly;

    std::ifstream f(path);
    if (!f) return s;

    std::stringstream buf;
    buf << f.rdbuf();
    std::string raw = buf.str();

    std::vector<std::string> fmTags;
    parseFrontmatter(raw, fmTags, s.syntax, s.text);

    /* Merge: parent tags + frontmatter tags */
    for (auto& t : parentTags) s.tags.push_back(t);
    for (auto& t : fmTags) s.tags.push_back(t);
    std::sort(s.tags.begin(), s.tags.end());
    /* Remove duplicates */
    s.tags.erase(std::unique(s.tags.begin(), s.tags.end()), s.tags.end());

    return s;
}

static void walkDir(const std::string& dirPath, const std::string& basePath,
                    const std::string& cheatPathName, bool readOnly,
                    const std::vector<std::string>& parentTags,
                    std::vector<Sheet>& out) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.') continue; /* skip hidden */

        std::string fullPath = dirPath + "/" + entry->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (strcmp(entry->d_name, ".git") == 0) continue;
            walkDir(fullPath, basePath, cheatPathName, readOnly, parentTags, out);
        } else if (S_ISREG(st.st_mode)) {
            /* Skip files with extensions */
            const char* dot = strrchr(entry->d_name, '.');
            if (dot && dot != entry->d_name) continue;

            /* Compute title: path relative to basePath, without leading / */
            std::string title = fullPath.substr(basePath.size());
            while (!title.empty() && title[0] == '/') title = title.substr(1);

            Sheet s = loadSheet(fullPath, title, cheatPathName, readOnly, parentTags);
            out.push_back(s);
        }
    }
    closedir(dir);
}

std::vector<Sheet> loadSheetsFromDir(const std::string& dirPath,
                                      const std::string& cheatPathName,
                                      bool readOnly,
                                      const std::vector<std::string>& parentTags) {
    std::vector<Sheet> sheets;
    walkDir(dirPath, dirPath, cheatPathName, readOnly, parentTags, sheets);
    return sheets;
}

/* Simple ANSI color highlighter based on syntax name */
std::string colorize(const std::string& text, const std::string& syntax) {
    if (syntax.empty()) return text;
    if (text.empty()) return text;

    std::string out;
    out.reserve(text.size() * 2);

    /* Simple keyword-based highlighting for common languages */
    const char* kwColor = "\033[33m";   /* yellow for keywords */
    const char* cmtColor = "\033[90m";  /* gray for comments */
    const char* strColor = "\033[32m";  /* green for strings */
    const char* rst = "\033[0m";

    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        /* Check for comments */
        if (syntax == "bash" || syntax == "sh" || syntax == "python" || syntax == "yaml") {
            size_t hash = line.find('#');
            if (hash != std::string::npos) {
                out += line.substr(0, hash) + cmtColor + line.substr(hash) + rst + "\n";
                continue;
            }
        }
        /* Just output plain for now */
        out += line + "\n";
    }
    return out;
}

std::vector<Sheet> searchSheets(const std::vector<Sheet>& sheets,
                                 const std::string& phrase, bool useRegex) {
    std::vector<Sheet> results;
    for (auto& s : sheets) {
        bool match = false;
        if (useRegex) {
            try {
                std::regex re(phrase, std::regex::icase);
                if (std::regex_search(s.text, re)) match = true;
            } catch (...) { match = (s.text.find(phrase) != std::string::npos); }
        } else {
            std::string lower = s.text;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::string plower = phrase;
            std::transform(plower.begin(), plower.end(), plower.begin(), ::tolower);
            match = (lower.find(plower) != std::string::npos);
        }
        if (match) results.push_back(s);
    }
    return results;
}

std::vector<Sheet> filterByTags(const std::vector<Sheet>& sheets,
                                 const std::vector<std::string>& tags) {
    if (tags.empty()) return sheets;
    std::vector<Sheet> results;
    for (auto& s : sheets) {
        bool hasAll = true;
        for (auto& tag : tags) {
            bool found = false;
            for (auto& st : s.tags)
                if (st == tag) { found = true; break; }
            if (!found) { hasAll = false; break; }
        }
        if (hasAll) results.push_back(s);
    }
    return results;
}
