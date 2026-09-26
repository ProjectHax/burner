#pragma once

#include <stdexcept>
#include <string>

namespace Burner::Core {

/// Exception thrown when libburnia initialization fails
class BurniaInitError : public std::runtime_error {
public:
    explicit BurniaInitError(const std::string& message)
        : std::runtime_error(message) {}
};

/// RAII guard for libburn and libisofs initialization.
/// Ensures proper initialization order and cleanup.
/// Only one instance should exist per application.
class BurniaInit {
public:
    /// Initialize libburn and libisofs libraries
    /// @throws BurniaInitError if initialization fails
    BurniaInit();

    /// Clean up libraries in reverse order
    ~BurniaInit();

    // Non-copyable
    BurniaInit(const BurniaInit&) = delete;
    BurniaInit& operator=(const BurniaInit&) = delete;

    // Movable
    BurniaInit(BurniaInit&& other) noexcept;
    BurniaInit& operator=(BurniaInit&& other) noexcept;

    /// Check if libraries are properly initialized
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    /// Get libburn version string
    [[nodiscard]] static std::string libburnVersion();

    /// Get libisofs version string
    [[nodiscard]] static std::string libisofsVersion();

private:
    bool m_initialized{false};
    bool m_libburnInit{false};
    bool m_libisofsInit{false};
};

} // namespace Burner::Core
