#include "MetadataProvider.hpp"

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#include "third_party/json.hpp"

#include <cstring>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace {

#ifdef HAVE_LIBCURL
// CURL write callback
size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<std::string*>(userdata);
    size_t totalSize = size * nmemb;
    buffer->append(ptr, totalSize);
    return totalSize;
}

// Rate limiter for MusicBrainz API (1 request per second)
class RateLimiter {
public:
    void wait() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRequest);
        if (elapsed.count() < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 - elapsed.count()));
        }
        m_lastRequest = std::chrono::steady_clock::now();
    }

private:
    std::chrono::steady_clock::time_point m_lastRequest;
};

static RateLimiter g_rateLimiter;
#endif // HAVE_LIBCURL

} // anonymous namespace

namespace Burner::Core {

class MetadataProvider::Impl {
public:
#ifdef HAVE_LIBCURL
    CURL* curl{nullptr};

    Impl() {
        curl = curl_easy_init();
    }

    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
#else
    Impl() = default;
    ~Impl() = default;
#endif
};

MetadataProvider::MetadataProvider()
    : m_impl(std::make_unique<Impl>())
    , m_lastError()
    , m_userAgent("Burner/" BURNER_VERSION " (contact@example.com)") {
}

MetadataProvider::~MetadataProvider() = default;

MetadataProvider::MetadataProvider(MetadataProvider&&) noexcept = default;
MetadataProvider& MetadataProvider::operator=(MetadataProvider&&) noexcept = default;

void MetadataProvider::setUserAgent(const std::string& appName,
                                     const std::string& version,
                                     const std::string& contact) {
    m_userAgent = appName + "/" + version + " (" + contact + ")";
}

MusicBrainzResult MetadataProvider::lookupByDiscId(const std::string& discId) {
    MusicBrainzResult result;
    result.discId = discId;

#ifndef HAVE_LIBCURL
    result.errorMessage = "MusicBrainz lookup not available (libcurl not installed)";
    return result;
#else

    if (!m_impl->curl) {
        result.errorMessage = "CURL not initialized";
        return result;
    }

    if (discId.empty()) {
        result.errorMessage = "Empty disc ID";
        return result;
    }

    // Rate limit
    g_rateLimiter.wait();

    // Build URL
    std::string url = "https://musicbrainz.org/ws/2/discid/" + discId +
                      "?fmt=json&inc=recordings+artist-credits+release-groups";

    std::string response;
    curl_easy_reset(m_impl->curl);
    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, m_userAgent.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(m_impl->curl);
    if (res != CURLE_OK) {
        result.errorMessage = "HTTP request failed: " + std::string(curl_easy_strerror(res));
        return result;
    }

    long httpCode = 0;
    curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode == 404) {
        result.errorMessage = "Disc not found in MusicBrainz database";
        return result;
    }

    if (httpCode != 200) {
        result.errorMessage = "HTTP error: " + std::to_string(httpCode);
        return result;
    }

    // Parse JSON response using nlohmann/json
    try {
        json root = json::parse(response);

        if (!root.contains("releases") || !root["releases"].is_array()) {
            result.errorMessage = "Invalid response from MusicBrainz";
            return result;
        }

        for (const auto& release : root["releases"]) {
            AlbumMetadata album;

            // Extract basic release info
            if (release.contains("id") && release["id"].is_string()) {
                album.releaseId = release["id"].get<std::string>();
            }
            if (release.contains("title") && release["title"].is_string()) {
                album.title = release["title"].get<std::string>();
            }

            // Extract artist from artist-credit array
            if (release.contains("artist-credit") && release["artist-credit"].is_array() &&
                !release["artist-credit"].empty()) {
                const auto& credit = release["artist-credit"][0];
                if (credit.contains("name") && credit["name"].is_string()) {
                    album.artist = credit["name"].get<std::string>();
                }
            }

            // Extract year from date
            if (release.contains("date") && release["date"].is_string()) {
                std::string date = release["date"].get<std::string>();
                if (date.length() >= 4) {
                    try {
                        album.year = std::stoi(date.substr(0, 4));
                    } catch (...) {}
                }
            }

            // Extract release group ID for cover art
            if (release.contains("release-group") && release["release-group"].is_object()) {
                const auto& rg = release["release-group"];
                if (rg.contains("id") && rg["id"].is_string()) {
                    album.releaseGroupId = rg["id"].get<std::string>();
                }
            }

            // Extract tracks from media array
            if (release.contains("media") && release["media"].is_array()) {
                int trackNum = 1;
                for (const auto& medium : release["media"]) {
                    if (!medium.contains("tracks") || !medium["tracks"].is_array()) {
                        continue;
                    }

                    for (const auto& trackObj : medium["tracks"]) {
                        TrackMeta track;
                        track.trackNumber = trackNum++;

                        if (trackObj.contains("title") && trackObj["title"].is_string()) {
                            track.title = trackObj["title"].get<std::string>();
                        }
                        if (trackObj.contains("length") && trackObj["length"].is_number()) {
                            track.durationMs = trackObj["length"].get<int>();
                        }

                        // Get recording info (fallback for title)
                        if (trackObj.contains("recording") && trackObj["recording"].is_object()) {
                            const auto& rec = trackObj["recording"];
                            if (rec.contains("id") && rec["id"].is_string()) {
                                track.recordingId = rec["id"].get<std::string>();
                            }
                            if (track.title.empty() && rec.contains("title") && rec["title"].is_string()) {
                                track.title = rec["title"].get<std::string>();
                            }
                        }

                        // Get track artist
                        if (trackObj.contains("artist-credit") && trackObj["artist-credit"].is_array() &&
                            !trackObj["artist-credit"].empty()) {
                            const auto& credit = trackObj["artist-credit"][0];
                            if (credit.contains("name") && credit["name"].is_string()) {
                                track.artist = credit["name"].get<std::string>();
                            }
                        }
                        if (track.artist.empty()) {
                            track.artist = album.artist;
                        }

                        album.tracks.push_back(track);
                    }
                }
            }

            album.albumArtist = album.artist;
            result.matches.push_back(album);
        }

    } catch (const json::parse_error& e) {
        result.errorMessage = std::string("JSON parse error: ") + e.what();
        return result;
    } catch (const json::type_error& e) {
        result.errorMessage = std::string("JSON type error: ") + e.what();
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Error parsing response: ") + e.what();
        return result;
    }

    if (result.matches.empty()) {
        result.errorMessage = "No releases found for disc ID";
    }

    return result;
#endif // HAVE_LIBCURL
}

std::optional<CoverArt> MetadataProvider::fetchCoverArt(const std::string& releaseId,
                                                         int size) {
#ifndef HAVE_LIBCURL
    (void)releaseId;
    (void)size;
    m_lastError = "Cover art fetch not available (libcurl not installed)";
    return std::nullopt;
#else

    if (!m_impl->curl || releaseId.empty()) {
        return std::nullopt;
    }

    // Rate limit
    g_rateLimiter.wait();

    // Build URL for Cover Art Archive
    std::string sizeStr;
    if (size <= 250) sizeStr = "-250";
    else if (size <= 500) sizeStr = "-500";
    else sizeStr = "-1200";

    std::string url = "https://coverartarchive.org/release/" + releaseId + "/front" + sizeStr;

    std::string response;
    curl_easy_reset(m_impl->curl);
    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_USERAGENT, m_userAgent.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(m_impl->curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(m_impl->curl);
    if (res != CURLE_OK) {
        m_lastError = "Failed to fetch cover art: " + std::string(curl_easy_strerror(res));
        return std::nullopt;
    }

    long httpCode = 0;
    curl_easy_getinfo(m_impl->curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (httpCode != 200) {
        m_lastError = "Cover art not available (HTTP " + std::to_string(httpCode) + ")";
        return std::nullopt;
    }

    if (response.empty()) {
        return std::nullopt;
    }

    CoverArt art;
    art.data.assign(response.begin(), response.end());

    // Determine MIME type from first bytes
    if (response.size() >= 3 &&
        static_cast<unsigned char>(response[0]) == 0xFF &&
        static_cast<unsigned char>(response[1]) == 0xD8 &&
        static_cast<unsigned char>(response[2]) == 0xFF) {
        art.mimeType = "image/jpeg";
    } else if (response.size() >= 8 &&
               static_cast<unsigned char>(response[0]) == 0x89 &&
               response[1] == 'P' && response[2] == 'N' && response[3] == 'G') {
        art.mimeType = "image/png";
    } else {
        art.mimeType = "image/jpeg";  // Default assumption
    }

    return art;
#endif // HAVE_LIBCURL
}

} // namespace Burner::Core
