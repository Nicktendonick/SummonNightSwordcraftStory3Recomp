// Structural and memory-safety tests only: no image, framebuffer, or output
// colour comparisons. Guest memory is not involved in presentation filters.
#include "runtime/presentation_filters.h"

#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "presentation filter test failed: %s\n", message);
        std::exit(1);
    }
}

// Page protections prove that the scaler accepts immutable input and never
// reads before/after the buffer, without comparing any pixel values. Test both
// alignments because buffers need not occupy a whole number of memory pages.
class ReadOnlyInput {
public:
    ReadOnlyInput(std::size_t byte_count, bool align_end) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const auto page = static_cast<std::size_t>(info.dwPageSize);
#else
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
        const auto usable = ((byte_count + page - 1) / page) * page;
        allocation_size_ = usable + 2 * page;
#ifdef _WIN32
        allocation_ = static_cast<std::uint8_t*>(VirtualAlloc(
            nullptr, allocation_size_, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        require(allocation_ != nullptr, "allocate guarded input");
        DWORD previous;
        require(VirtualProtect(allocation_ + page, usable, PAGE_READWRITE, &previous) != 0,
                "make input writable for initialization");
#else
        void* mapping = mmap(nullptr, allocation_size_, PROT_NONE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        require(mapping != MAP_FAILED, "allocate guarded input");
        allocation_ = static_cast<std::uint8_t*>(mapping);
        require(mprotect(allocation_ + page, usable, PROT_READ | PROT_WRITE) == 0,
                "make input writable for initialization");
#endif
        data_ = allocation_ + page + (align_end ? usable - byte_count : 0);
        for (std::size_t i = 0; i < byte_count; ++i)
            data_[i] = static_cast<std::uint8_t>((i * 37 + (i / 3) * 17) & 255);
#ifdef _WIN32
        require(VirtualProtect(allocation_ + page, usable, PAGE_READONLY, &previous) != 0,
                "make source immutable");
#else
        require(mprotect(allocation_ + page, usable, PROT_READ) == 0,
                "make source immutable");
#endif
    }

    ~ReadOnlyInput() {
#ifdef _WIN32
        VirtualFree(allocation_, 0, MEM_RELEASE);
#else
        munmap(allocation_, allocation_size_);
#endif
    }
    const std::uint8_t* data() const { return data_; }

private:
    std::uint8_t* allocation_ = nullptr;
    std::uint8_t* data_ = nullptr;
    std::size_t allocation_size_ = 0;
};

} // namespace

int main() {
    using namespace gbarecomp::runtime;
    require(valid_presentation_size(240, 160), "native extent supported");
    require(valid_presentation_size(384, 160), "widescreen extent supported");
    require(!valid_presentation_size(0, 160), "zero width rejected");
    require(!valid_presentation_size(-1, 160), "negative width rejected");
    require(!valid_presentation_size(384, INT_MAX), "integer overflow rejected");
    require(!valid_presentation_size(4096, 4096), "allocation limit enforced");

    SmoothScaler scaler;
    for (const auto& dimensions : {std::pair{1, 1}, std::pair{1, 7}, std::pair{7, 1},
                                 std::pair{240, 160}, std::pair{384, 160}}) {
        const int width = dimensions.first;
        const int height = dimensions.second;
        for (const bool align_end : {false, true}) {
            ReadOnlyInput source(static_cast<std::size_t>(width) * height * 3, align_end);
            require(scaler.scale2x(source.data(), width, height), "guarded source accepted");
            require(scaler.width() == width * 2 && scaler.height() == height * 2,
                    "reported doubled dimensions");
            require(scaler.output().size() == static_cast<std::size_t>(width) * height * 4,
                    "exact output allocation extent");
            const auto* storage = scaler.output().data();
            const auto capacity = scaler.output().capacity();
            require(scaler.scale2x(source.data(), width, height), "second frame succeeds");
            require(scaler.output().data() == storage && scaler.output().capacity() == capacity,
                    "same-size frames reuse backing allocation");
        }
    }

    const auto* overlapping = reinterpret_cast<const std::uint8_t*>(scaler.output().data());
    require(!scaler.scale2x(overlapping, 384, 160), "aliased source rejected");
    require(scaler.output().empty() && scaler.width() == 0 && scaler.height() == 0,
            "failed scale clears logical output");
    require(!scaler.scale2x(nullptr, 240, 160), "null source rejected");
    std::uint8_t unused[3]{};
    require(!scaler.scale2x(unused, INT_MAX, INT_MAX), "invalid huge source rejected before access");

    EffectMask mask;
    for (const int effect : {1, 2}) {
        require(build_screen_effect_mask(384, 160, effect, 60, mask), "effect builds");
        require(mask.width == 1152 && mask.height == 480, "mask dimensions are source-aligned");
        require(mask.rgba.size() == 1152u * 480u * 4u, "mask allocation covers dimensions");
        const auto* storage = mask.rgba.data();
        const auto capacity = mask.rgba.capacity();
        require(build_screen_effect_mask(384, 160, effect, 1000, mask), "strength clamps high");
        require(mask.rgba.data() == storage && mask.rgba.capacity() == capacity,
                "mask rebuild reuses allocation");
        require(build_screen_effect_mask(384, 160, effect, -1, mask), "strength clamps low");
        require(mask.rgba.empty() && !mask.width && !mask.height, "zero-strength mask disabled");
    }
    require(build_screen_effect_mask(240, 160, 0, 100, mask), "none is valid");
    require(mask.rgba.empty(), "none has no overlay");
    require(!build_screen_effect_mask(240, 160, -1, 50, mask), "invalid effect rejected");
    require(!build_screen_effect_mask(240, 160, 3, 50, mask), "future effect rejected safely");
    require(!build_screen_effect_mask(INT_MAX, INT_MAX, 1, 50, mask), "mask overflow rejected");
    require(mask.rgba.empty() && !mask.width && !mask.height, "invalid mask cannot be presented");

    // Measurement only, not a timing assertion or a game-frame-rate guarantee.
    ReadOnlyInput source(384u * 160u * 3u, true);
    constexpr int frames = 100;
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < frames; ++frame)
        require(scaler.scale2x(source.data(), 384, 160), "measurement frame succeeds");
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("presentation filter structural/memory tests passed; Smooth 2x 384x160: %.3f ms/frame (%d synthetic frames)\n",
                elapsed / frames, frames);
    return 0;
}
