#ifndef CHEATCPP_SHEET_HPP
#define CHEATCPP_SHEET_HPP

#include <string>
#include <vector>

struct Sheet {
    std::string title;
    std::string cheatPath;   /* name of cheatpath */
    std::string path;        /* full filesystem path */
    std::string text;        /* content without frontmatter */
    std::vector<std::string> tags;
    std::string syntax;      /* for syntax highlighting */
    bool readOnly = false;
};

/* Parse a cheatsheet file: extracts YAML frontmatter tags/syntax and body text. */
Sheet loadSheet(const std::string& path, const std::string& title,
                const std::string& cheatPathName, bool readOnly,
                const std::vector<std::string>& parentTags = {});

/* Walk a directory, returning all sheets found (recursive). */
std::vector<Sheet> loadSheetsFromDir(const std::string& dirPath,
                                      const std::string& cheatPathName,
                                      bool readOnly,
                                      const std::vector<std::string>& parentTags = {});

/* Apply ANSI color highlighting to text based on syntax. */
std::string colorize(const std::string& text, const std::string& syntax);

/* Search sheets for a phrase, return matching sheets. */
std::vector<Sheet> searchSheets(const std::vector<Sheet>& sheets,
                                 const std::string& phrase, bool regex);

/* Filter sheets by tags. */
std::vector<Sheet> filterByTags(const std::vector<Sheet>& sheets,
                                 const std::vector<std::string>& tags);

#endif
