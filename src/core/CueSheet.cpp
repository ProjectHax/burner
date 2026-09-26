#include "CueSheet.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Burner::Core {

bool CueSheet::parse(const std::filesystem::path& cuePath) {
    m_tracks.clear();
    m_title.clear();
    m_performer.clear();
    m_lastError.clear();
    m_file = {};

    if (!std::filesystem::exists(cuePath)) {
        m_lastError = "CUE file not found: " + cuePath.string();
        return false;
    }

    std::ifstream file(cuePath);
    if (!file.is_open()) {
        m_lastError = "Failed to open CUE file: " + cuePath.string();
        return false;
    }

    std::filesystem::path cueDir = cuePath.parent_path();
    CueTrack* currentTrack = nullptr;
    bool hasFile = false;
    std::string line;

    while (std::getline(file, line)) {
        // Strip BOM if present
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }

        line = trim(line);
        if (line.empty()) continue;

        // Parse the first keyword
        std::string keyword;
        {
            std::istringstream iss(line);
            iss >> keyword;
        }
        keyword = toUpper(keyword);

        if (keyword == "REM") {
            // Comment - ignore
            continue;
        }

        if (keyword == "FILE") {
            if (hasFile) {
                m_lastError = "Multi-file CUE sheets are not supported. "
                              "Please use a single BIN file.";
                return false;
            }

            // Parse: FILE "filename" TYPE
            std::string filename = parseQuotedString(line, 4);
            if (filename.empty()) {
                // Try unquoted
                std::istringstream iss(line);
                std::string dummy;
                iss >> dummy >> filename;
            }

            // Get file type (last word on the line)
            std::string fileType;
            {
                auto lastSpace = line.rfind(' ');
                if (lastSpace != std::string::npos) {
                    fileType = toUpper(trim(line.substr(lastSpace + 1)));
                }
            }

            // Resolve path relative to CUE directory
            std::filesystem::path binPath(filename);
            if (binPath.is_relative()) {
                binPath = cueDir / binPath;
            }

            m_file.path = binPath;
            m_file.fileType = fileType;
            hasFile = true;

            // Set bin sector size based on file type
            if (fileType == "BINARY" || fileType == "MOTOROLA") {
                m_binSectorSize = 2352;
            } else {
                // WAVE, AIFF, MP3 - audio formats, sector concept doesn't apply the same way
                m_binSectorSize = 2352;
            }

            continue;
        }

        if (keyword == "TRACK") {
            // Parse: TRACK nn TYPE
            std::istringstream iss(line);
            std::string dummy, numStr, typeStr;
            iss >> dummy >> numStr >> typeStr;

            int trackNum = 0;
            try {
                trackNum = std::stoi(numStr);
            } catch (...) {
                m_lastError = "Invalid track number: " + numStr;
                return false;
            }

            CueTrack track;
            track.number = trackNum;
            track.type = parseTrackType(toUpper(typeStr));
            track.sectorSize = sectorSizeForType(track.type);

            if (track.type == CueTrackType::Unknown) {
                m_lastError = "Unknown track type: " + typeStr;
                return false;
            }

            m_tracks.push_back(track);
            currentTrack = &m_tracks.back();
            continue;
        }

        if (keyword == "INDEX") {
            if (!currentTrack) {
                m_lastError = "INDEX before TRACK";
                return false;
            }

            std::istringstream iss(line);
            std::string dummy, indexNumStr, msfStr;
            iss >> dummy >> indexNumStr >> msfStr;

            int indexNum = 0;
            try {
                indexNum = std::stoi(indexNumStr);
            } catch (...) {
                m_lastError = "Invalid index number: " + indexNumStr;
                return false;
            }

            MsfTime msf = parseMsf(msfStr);

            if (indexNum == 0) {
                currentTrack->index00 = msf;
                currentTrack->hasIndex00 = true;
            } else if (indexNum == 1) {
                currentTrack->index01 = msf;
                currentTrack->hasIndex01 = true;
            }
            // Other indices (2+) ignored

            continue;
        }

        if (keyword == "TITLE") {
            std::string value = parseQuotedString(line, 5);
            if (currentTrack) {
                currentTrack->title = value;
            } else {
                m_title = value;
            }
            continue;
        }

        if (keyword == "PERFORMER") {
            std::string value = parseQuotedString(line, 9);
            if (currentTrack) {
                currentTrack->performer = value;
            } else {
                m_performer = value;
            }
            continue;
        }

        if (keyword == "PREGAP") {
            if (!currentTrack) continue;
            std::istringstream iss(line);
            std::string dummy, msfStr;
            iss >> dummy >> msfStr;
            currentTrack->pregap = parseMsf(msfStr);
            currentTrack->hasPregap = true;
            continue;
        }

        // CATALOG, ISRC, FLAGS, POSTGAP, SONGWRITER, etc. - ignore
    }

    if (!hasFile) {
        m_lastError = "No FILE directive found in CUE sheet";
        return false;
    }

    if (m_tracks.empty()) {
        m_lastError = "No TRACK directives found in CUE sheet";
        return false;
    }

    // Verify all tracks have INDEX 01
    for (const auto& track : m_tracks) {
        if (!track.hasIndex01) {
            m_lastError = "Track " + std::to_string(track.number) + " missing INDEX 01";
            return false;
        }
    }

    // Calculate track byte offsets using actual BIN file size
    if (std::filesystem::exists(m_file.path)) {
        std::uint64_t binSize = std::filesystem::file_size(m_file.path);
        calculateTrackOffsets(binSize);
    }

    return true;
}

bool CueSheet::validate() const {
    if (m_tracks.empty()) {
        m_lastError = "No tracks parsed";
        return false;
    }

    if (!std::filesystem::exists(m_file.path)) {
        m_lastError = "BIN file not found: " + m_file.path.string();
        return false;
    }

    std::uint64_t binSize = std::filesystem::file_size(m_file.path);
    if (binSize == 0) {
        m_lastError = "BIN file is empty: " + m_file.path.string();
        return false;
    }

    // Check that the last track doesn't start beyond the file
    const auto& lastTrack = m_tracks.back();
    if (lastTrack.dataStartByte >= binSize) {
        m_lastError = "Track " + std::to_string(lastTrack.number) +
                      " start offset exceeds BIN file size";
        return false;
    }

    return true;
}

std::uint64_t CueSheet::totalDataSize() const {
    std::uint64_t total = 0;
    for (const auto& track : m_tracks) {
        total += track.dataLengthBytes;
    }
    // If we haven't calculated offsets yet, fall back to BIN file size
    if (total == 0 && std::filesystem::exists(m_file.path)) {
        return std::filesystem::file_size(m_file.path);
    }
    return total;
}

bool CueSheet::isSingleDataTrack() const {
    if (m_tracks.size() != 1) return false;
    const auto& t = m_tracks[0];
    return t.type == CueTrackType::Mode1_2048 ||
           t.type == CueTrackType::Mode1_2352 ||
           t.type == CueTrackType::Mode2_2336 ||
           t.type == CueTrackType::Mode2_2352;
}

bool CueSheet::isAudioDisc() const {
    return std::all_of(m_tracks.begin(), m_tracks.end(),
        [](const CueTrack& t) { return t.type == CueTrackType::Audio; });
}

bool CueSheet::isMixedMode() const {
    bool hasAudio = false;
    bool hasData = false;
    for (const auto& t : m_tracks) {
        if (t.type == CueTrackType::Audio) hasAudio = true;
        else hasData = true;
    }
    return hasAudio && hasData;
}

CueTrackType CueSheet::parseTrackType(const std::string& typeStr) {
    if (typeStr == "AUDIO") return CueTrackType::Audio;
    if (typeStr == "MODE1/2048") return CueTrackType::Mode1_2048;
    if (typeStr == "MODE1/2352") return CueTrackType::Mode1_2352;
    if (typeStr == "MODE2/2336") return CueTrackType::Mode2_2336;
    if (typeStr == "MODE2/2352") return CueTrackType::Mode2_2352;
    // CDG, CDI variants
    if (typeStr == "CDG") return CueTrackType::Audio;  // CD+G is audio-based
    if (typeStr.find("CDI") == 0) return CueTrackType::Mode2_2352;
    return CueTrackType::Unknown;
}

int CueSheet::sectorSizeForType(CueTrackType type) {
    switch (type) {
        case CueTrackType::Audio:      return 2352;
        case CueTrackType::Mode1_2048: return 2048;
        case CueTrackType::Mode1_2352: return 2352;
        case CueTrackType::Mode2_2336: return 2336;
        case CueTrackType::Mode2_2352: return 2352;
        case CueTrackType::Unknown:    return 2352;
    }
    return 2352;
}

MsfTime CueSheet::parseMsf(const std::string& msf) {
    MsfTime result;
    // Format: MM:SS:FF
    if (std::sscanf(msf.c_str(), "%d:%d:%d",
                    &result.minutes, &result.seconds, &result.frames) != 3) {
        result = {};
    }
    return result;
}

std::string CueSheet::parseQuotedString(const std::string& line, std::size_t startPos) {
    auto firstQuote = line.find('"', startPos);
    if (firstQuote == std::string::npos) return {};

    auto lastQuote = line.rfind('"');
    if (lastQuote == firstQuote) return {};

    return line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
}

std::string CueSheet::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string CueSheet::toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

bool CueSheet::generate(const std::filesystem::path& cuePath,
                        const std::string& binFilename,
                        const std::vector<CueTrack>& tracks) {
    std::ofstream file(cuePath);
    if (!file.is_open()) {
        return false;
    }

    file << "FILE \"" << binFilename << "\" BINARY\n";

    for (const auto& track : tracks) {
        file << "  TRACK "
             << (track.number < 10 ? "0" : "") << track.number
             << " " << trackTypeString(track.type) << "\n";

        if (track.hasIndex00) {
            file << "    INDEX 00 " << formatMsf(track.index00) << "\n";
        }
        if (track.hasIndex01) {
            file << "    INDEX 01 " << formatMsf(track.index01) << "\n";
        }
    }

    return file.good();
}

std::string CueSheet::trackTypeString(CueTrackType type) {
    switch (type) {
        case CueTrackType::Audio:      return "AUDIO";
        case CueTrackType::Mode1_2048: return "MODE1/2048";
        case CueTrackType::Mode1_2352: return "MODE1/2352";
        case CueTrackType::Mode2_2336: return "MODE2/2336";
        case CueTrackType::Mode2_2352: return "MODE2/2352";
        case CueTrackType::Unknown:    return "MODE1/2352";
    }
    return "MODE1/2352";
}

std::string CueSheet::formatMsf(const MsfTime& msf) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", msf.minutes, msf.seconds, msf.frames);
    return buf;
}

void CueSheet::calculateTrackOffsets(std::uint64_t binFileSize) {
    if (m_tracks.empty()) return;

    // For BINARY files, all sectors in the BIN are stored at m_binSectorSize (2352)
    // The INDEX positions are in MSF format, and each frame = one sector
    for (auto& track : m_tracks) {
        track.dataStartByte = track.index01.toByteOffset(m_binSectorSize);
    }

    // Calculate data lengths
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        if (i + 1 < m_tracks.size()) {
            // Next track's start (use INDEX 00 if present for pregap, else INDEX 01)
            std::uint64_t nextStart;
            if (m_tracks[i + 1].hasIndex00) {
                nextStart = m_tracks[i + 1].index00.toByteOffset(m_binSectorSize);
            } else {
                nextStart = m_tracks[i + 1].index01.toByteOffset(m_binSectorSize);
            }
            m_tracks[i].dataLengthBytes = nextStart - m_tracks[i].dataStartByte;
        } else {
            // Last track extends to end of file
            m_tracks[i].dataLengthBytes = binFileSize - m_tracks[i].dataStartByte;
        }
    }
}

} // namespace Burner::Core
