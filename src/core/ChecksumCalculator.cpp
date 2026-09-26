#include "ChecksumCalculator.hpp"

#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace Burner::Core {

namespace {

const EVP_MD* evpAlgorithm(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::MD5:    return EVP_md5();
        case HashAlgorithm::SHA1:   return EVP_sha1();
        case HashAlgorithm::SHA256: return EVP_sha256();
        case HashAlgorithm::SHA512: return EVP_sha512();
    }
    return EVP_sha256();
}

std::string toHex(const unsigned char* data, unsigned int len) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

} // anonymous namespace

std::string ChecksumCalculator::algorithmName(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::MD5:    return "MD5";
        case HashAlgorithm::SHA1:   return "SHA-1";
        case HashAlgorithm::SHA256: return "SHA-256";
        case HashAlgorithm::SHA512: return "SHA-512";
    }
    return "Unknown";
}

ChecksumResult ChecksumCalculator::calculate(const std::filesystem::path& filePath,
                                              HashAlgorithm algorithm,
                                              ProgressCallback progress) {
    auto results = calculateMultiple(filePath, {algorithm}, progress);
    if (!results.empty()) {
        return results[0];
    }
    return ChecksumResult{algorithm, "", 0, false, "No result"};
}

std::vector<ChecksumResult> ChecksumCalculator::calculateMultiple(
    const std::filesystem::path& filePath,
    const std::vector<HashAlgorithm>& algorithms,
    ProgressCallback progress) {

    std::vector<ChecksumResult> results;
    results.reserve(algorithms.size());
    for (auto algo : algorithms) {
        results.push_back(ChecksumResult{algo, "", 0, false, ""});
    }

    if (algorithms.empty()) {
        return results;
    }

    // Get file size
    std::error_code ec;
    auto fileSize = std::filesystem::file_size(filePath, ec);
    if (ec) {
        for (auto& r : results) {
            r.error = "Could not get file size: " + ec.message();
        }
        return results;
    }

    // Open file
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        for (auto& r : results) {
            r.error = "Could not open file";
        }
        return results;
    }

    // Create EVP contexts for each algorithm
    struct CtxHolder {
        EVP_MD_CTX* ctx{nullptr};
        ~CtxHolder() { if (ctx) EVP_MD_CTX_free(ctx); }
    };

    std::vector<CtxHolder> contexts(algorithms.size());
    for (size_t i = 0; i < algorithms.size(); ++i) {
        contexts[i].ctx = EVP_MD_CTX_new();
        if (!contexts[i].ctx) {
            for (auto& r : results) {
                r.error = "Failed to create hash context";
            }
            return results;
        }
        if (EVP_DigestInit_ex(contexts[i].ctx, evpAlgorithm(algorithms[i]), nullptr) != 1) {
            for (auto& r : results) {
                r.error = "Failed to initialize hash";
            }
            return results;
        }
    }

    // Read file in 1MB chunks
    constexpr size_t bufferSize = 1024 * 1024;
    std::vector<char> buffer(bufferSize);
    std::uint64_t totalRead = 0;

    while (file && !m_cancelled.load()) {
        file.read(buffer.data(), static_cast<std::streamsize>(bufferSize));
        auto bytesRead = file.gcount();
        if (bytesRead <= 0) {
            break;
        }

        for (size_t i = 0; i < contexts.size(); ++i) {
            EVP_DigestUpdate(contexts[i].ctx,
                             reinterpret_cast<const unsigned char*>(buffer.data()),
                             static_cast<size_t>(bytesRead));
        }

        totalRead += static_cast<std::uint64_t>(bytesRead);

        if (progress) {
            progress(totalRead, fileSize);
        }
    }

    if (m_cancelled.load()) {
        for (auto& r : results) {
            r.error = "Cancelled";
            r.bytesRead = totalRead;
        }
        return results;
    }

    // Finalize each digest
    for (size_t i = 0; i < contexts.size(); ++i) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLen = 0;

        if (EVP_DigestFinal_ex(contexts[i].ctx, digest, &digestLen) == 1) {
            results[i].hexDigest = toHex(digest, digestLen);
            results[i].success = true;
        } else {
            results[i].error = "Failed to finalize hash";
        }
        results[i].bytesRead = totalRead;
    }

    return results;
}

} // namespace Burner::Core
