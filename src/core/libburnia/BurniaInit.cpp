#include "BurniaInit.hpp"

#include <cstdint>
#include <libburn/libburn.h>
#include <libisofs/libisofs.h>

using namespace burn;

namespace Burner::Core {

BurniaInit::BurniaInit() {
    // Initialize libburn first
    if (!burn_initialize()) {
        throw BurniaInitError("Failed to initialize libburn");
    }
    m_libburnInit = true;

    // Set up signal handling for graceful interrupts
    burn_set_signal_handling(const_cast<char*>("burner : "), nullptr, 0x0);

    // Initialize libisofs
    int ret = iso_init();
    if (ret < 0) {
        burn_finish();
        m_libburnInit = false;
        throw BurniaInitError("Failed to initialize libisofs: error code " + std::to_string(ret));
    }
    m_libisofsInit = true;

    m_initialized = true;
}

BurniaInit::~BurniaInit() {
    if (m_libisofsInit) {
        iso_finish();
        m_libisofsInit = false;
    }
    if (m_libburnInit) {
        burn_finish();
        m_libburnInit = false;
    }
    m_initialized = false;
}

BurniaInit::BurniaInit(BurniaInit&& other) noexcept
    : m_initialized(other.m_initialized)
    , m_libburnInit(other.m_libburnInit)
    , m_libisofsInit(other.m_libisofsInit) {
    other.m_initialized = false;
    other.m_libburnInit = false;
    other.m_libisofsInit = false;
}

BurniaInit& BurniaInit::operator=(BurniaInit&& other) noexcept {
    if (this != &other) {
        // Clean up current state
        if (m_libisofsInit) {
            iso_finish();
        }
        if (m_libburnInit) {
            burn_finish();
        }

        // Move from other
        m_initialized = other.m_initialized;
        m_libburnInit = other.m_libburnInit;
        m_libisofsInit = other.m_libisofsInit;

        other.m_initialized = false;
        other.m_libburnInit = false;
        other.m_libisofsInit = false;
    }
    return *this;
}

std::string BurniaInit::libburnVersion() {
    int major, minor, micro;
    burn_version(&major, &minor, &micro);
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(micro);
}

std::string BurniaInit::libisofsVersion() {
    int major, minor, micro;
    iso_lib_version(&major, &minor, &micro);
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(micro);
}

} // namespace Burner::Core
