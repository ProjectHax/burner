#pragma once

#include <string>
#include <filesystem>
#include <functional>
#include <atomic>
#include <cstdint>
#include <vector>

namespace Burner::Core {

/// Supported hash algorithms
enum class HashAlgorithm {
    MD5,
    SHA1,
    SHA256,
    SHA512
};

/// Result of a checksum calculation
struct ChecksumResult {
    HashAlgorithm algorithm{HashAlgorithm::SHA256};
    std::string hexDigest;
    std::uint64_t bytesRead{0};
    bool success{false};
    std::string error;
};

/// Calculates file checksums using OpenSSL EVP API.
/// Pure C++20, no Qt dependencies.
class ChecksumCalculator {
public:
    /// Progress callback: (bytesProcessed, totalBytes)
    using ProgressCallback = std::function<void(std::uint64_t, std::uint64_t)>;

    ChecksumCalculator() = default;
    ~ChecksumCalculator() = default;

    ChecksumCalculator(const ChecksumCalculator&) = delete;
    ChecksumCalculator& operator=(const ChecksumCalculator&) = delete;

    /// Calculate a single checksum for a file
    ChecksumResult calculate(const std::filesystem::path& filePath,
                             HashAlgorithm algorithm,
                             ProgressCallback progress = nullptr);

    /// Calculate multiple checksums in a single pass
    std::vector<ChecksumResult> calculateMultiple(
        const std::filesystem::path& filePath,
        const std::vector<HashAlgorithm>& algorithms,
        ProgressCallback progress = nullptr);

    /// Request cancellation
    void cancel() { m_cancelled.store(true); }

    /// Check if cancelled
    bool isCancelled() const { return m_cancelled.load(); }

    /// Reset cancellation flag
    void reset() { m_cancelled.store(false); }

    /// Get algorithm name as string
    static std::string algorithmName(HashAlgorithm algo);

private:
    std::atomic<bool> m_cancelled{false};
};

} // namespace Burner::Core
