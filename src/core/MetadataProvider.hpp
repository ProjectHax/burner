#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>

namespace Burner::Core {

/// Cover art data
struct CoverArt {
    std::vector<std::uint8_t> data;
    std::string mimeType;  ///< "image/jpeg" or "image/png"
    int width{0};
    int height{0};
};

/// Single track metadata from MusicBrainz
struct TrackMeta {
    std::string title;
    std::string artist;
    int trackNumber{0};
    int durationMs{0};
    std::string recordingId;  ///< MusicBrainz recording MBID
};

/// Album metadata from MusicBrainz
struct AlbumMetadata {
    std::string title;
    std::string artist;
    std::string albumArtist;
    int year{0};
    std::string genre;
    std::string releaseId;        ///< MusicBrainz release MBID
    std::string releaseGroupId;   ///< MusicBrainz release group MBID
    std::vector<TrackMeta> tracks;
    std::optional<CoverArt> coverArt;
};

/// MusicBrainz lookup result
struct MusicBrainzResult {
    std::string discId;
    std::vector<AlbumMetadata> matches;  ///< Multiple releases may match
    std::string errorMessage;
};

/// Provider for CD metadata using MusicBrainz.
class MetadataProvider {
public:
    MetadataProvider();
    ~MetadataProvider();

    // Non-copyable
    MetadataProvider(const MetadataProvider&) = delete;
    MetadataProvider& operator=(const MetadataProvider&) = delete;

    // Movable
    MetadataProvider(MetadataProvider&&) noexcept;
    MetadataProvider& operator=(MetadataProvider&&) noexcept;

    /// Set user agent for HTTP requests
    /// @param appName Application name
    /// @param version Version string
    /// @param contact Contact email
    void setUserAgent(const std::string& appName,
                      const std::string& version,
                      const std::string& contact);

    /// Look up album metadata by MusicBrainz disc ID
    /// @param discId The MusicBrainz disc ID
    /// @return MusicBrainzResult with matches
    [[nodiscard]] MusicBrainzResult lookupByDiscId(const std::string& discId);

    /// Fetch cover art for a release
    /// @param releaseId MusicBrainz release MBID
    /// @param size Size in pixels (250, 500, or 1200; default 500)
    /// @return CoverArt if available
    [[nodiscard]] std::optional<CoverArt> fetchCoverArt(const std::string& releaseId,
                                                         int size = 500);

    /// Get last error message
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::string m_lastError;
    std::string m_userAgent;
};

} // namespace Burner::Core
