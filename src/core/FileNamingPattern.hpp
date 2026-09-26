#pragma once

#include <string>
#include <filesystem>

namespace Burner::Core {

/// Track information for file naming
struct TrackNamingInfo {
    std::string albumArtist;
    std::string trackArtist;
    std::string albumTitle;
    std::string trackTitle;
    int trackNumber{0};
    int totalTracks{0};
    int discNumber{1};
    int year{0};
    std::string genre;
};

/// File naming pattern parser and generator.
/// Supports tokens like %a (artist), %b (album), %n (track number), etc.
class FileNamingPattern {
public:
    /// Default pattern: "Artist - Album/NN - Title"
    static constexpr const char* DEFAULT_PATTERN = "%a - %b/%n - %t";

    /// Construct with a pattern
    /// @param pattern Pattern string with tokens
    explicit FileNamingPattern(const std::string& pattern = DEFAULT_PATTERN);

    /// Set the pattern
    void setPattern(const std::string& pattern);

    /// Get the current pattern
    [[nodiscard]] const std::string& pattern() const { return m_pattern; }

    /// Generate a filename from track info
    /// @param info Track information
    /// @param extension File extension (e.g., ".flac")
    /// @return Generated path (relative)
    [[nodiscard]] std::filesystem::path generate(const TrackNamingInfo& info,
                                                  const std::string& extension) const;

    /// Generate a preview filename
    /// @param info Track information
    /// @param extension File extension
    /// @return Preview string showing full path
    [[nodiscard]] std::string preview(const TrackNamingInfo& info,
                                       const std::string& extension) const;

    /// Check if the pattern is valid
    [[nodiscard]] bool isValid() const { return m_valid; }

    /// Get validation error message
    [[nodiscard]] const std::string& errorMessage() const { return m_errorMessage; }

    /// Common preset patterns
    static constexpr const char* PRESET_ARTIST_ALBUM = "%a - %b/%n - %t";      // Artist - Album/01 - Song
    static constexpr const char* PRESET_FOLDER_STRUCT = "%a/%b/%n %t";         // Artist/Album/01 Song
    static constexpr const char* PRESET_TRACK_FIRST = "%n - %a - %t";          // 01 - Artist - Song
    static constexpr const char* PRESET_FLAT = "%a - %t";                      // Artist - Song (no folders)
    static constexpr const char* PRESET_FULL = "%a - %b (%y)/%n - %t";         // Artist - Album (Year)/01 - Song

    /// Token documentation:
    /// %a - Album artist
    /// %A - Track artist (falls back to album artist)
    /// %b - Album title
    /// %t - Track title
    /// %n - Track number (zero-padded: 01, 02)
    /// %N - Track number (no padding: 1, 2)
    /// %d - Disc number
    /// %y - Year
    /// %g - Genre

private:
    std::string m_pattern;
    bool m_valid{true};
    std::string m_errorMessage;

    /// Sanitize a string for use in filenames
    [[nodiscard]] static std::string sanitize(const std::string& str);

    /// Replace a token with a value
    [[nodiscard]] std::string replaceTokens(const TrackNamingInfo& info) const;
};

} // namespace Burner::Core
