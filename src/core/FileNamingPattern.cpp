#include "FileNamingPattern.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace Burner::Core {

FileNamingPattern::FileNamingPattern(const std::string& pattern)
    : m_pattern(pattern) {
    // Validate pattern has at least one token
    if (pattern.find('%') == std::string::npos) {
        m_valid = false;
        m_errorMessage = "Pattern must contain at least one token (e.g., %t for track title)";
    }
}

void FileNamingPattern::setPattern(const std::string& pattern) {
    m_pattern = pattern;
    m_valid = true;
    m_errorMessage.clear();

    if (pattern.find('%') == std::string::npos) {
        m_valid = false;
        m_errorMessage = "Pattern must contain at least one token";
    }
}

std::filesystem::path FileNamingPattern::generate(const TrackNamingInfo& info,
                                                   const std::string& extension) const {
    std::string result = replaceTokens(info);
    return std::filesystem::path(result + extension);
}

std::string FileNamingPattern::preview(const TrackNamingInfo& info,
                                        const std::string& extension) const {
    return generate(info, extension).string();
}

std::string FileNamingPattern::sanitize(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        // Replace characters that are invalid in filenames
        switch (c) {
            case '/':
            case '\\':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                result += '_';
                break;
            default:
                if (c >= 32) {  // Skip control characters
                    result += c;
                }
                break;
        }
    }

    // Trim leading/trailing whitespace
    size_t start = result.find_first_not_of(" \t");
    size_t end = result.find_last_not_of(" \t");

    if (start == std::string::npos) {
        return "Unknown";  // All whitespace
    }

    result = result.substr(start, end - start + 1);

    // Ensure not empty
    if (result.empty()) {
        return "Unknown";
    }

    return result;
}

std::string FileNamingPattern::replaceTokens(const TrackNamingInfo& info) const {
    std::string result;
    result.reserve(m_pattern.size() * 2);

    for (size_t i = 0; i < m_pattern.size(); ++i) {
        if (m_pattern[i] == '%' && i + 1 < m_pattern.size()) {
            char token = m_pattern[i + 1];
            std::string value;

            switch (token) {
                case 'a':  // Album artist
                    value = sanitize(info.albumArtist.empty() ? "Unknown Artist" : info.albumArtist);
                    break;

                case 'A':  // Track artist (falls back to album artist)
                    if (!info.trackArtist.empty()) {
                        value = sanitize(info.trackArtist);
                    } else if (!info.albumArtist.empty()) {
                        value = sanitize(info.albumArtist);
                    } else {
                        value = "Unknown Artist";
                    }
                    break;

                case 'b':  // Album title
                    value = sanitize(info.albumTitle.empty() ? "Unknown Album" : info.albumTitle);
                    break;

                case 't':  // Track title
                    value = sanitize(info.trackTitle.empty() ? "Unknown Track" : info.trackTitle);
                    break;

                case 'n': {  // Track number (zero-padded)
                    std::ostringstream ss;
                    int width = (info.totalTracks >= 100) ? 3 : 2;
                    ss << std::setw(width) << std::setfill('0') << info.trackNumber;
                    value = ss.str();
                    break;
                }

                case 'N':  // Track number (no padding)
                    value = std::to_string(info.trackNumber);
                    break;

                case 'd':  // Disc number
                    value = std::to_string(info.discNumber);
                    break;

                case 'y':  // Year
                    if (info.year > 0) {
                        value = std::to_string(info.year);
                    } else {
                        value = "0000";
                    }
                    break;

                case 'g':  // Genre
                    value = sanitize(info.genre.empty() ? "Unknown" : info.genre);
                    break;

                case '%':  // Literal %
                    value = "%";
                    break;

                default:
                    // Unknown token, keep as-is
                    value = "%";
                    value += token;
                    break;
            }

            result += value;
            ++i;  // Skip the token character
        } else {
            // Keep forward slashes for directory structure
            result += m_pattern[i];
        }
    }

    return result;
}

} // namespace Burner::Core
