#include "Clock.h"
#include "Assert.h"
#include "Platform/Win32/WinPlatform.h"

namespace noc {

    static TimeSystem g_time;

    TimeSystem& GetTime()
    {
        return g_time;
    }

    // ----- HiResClock -----

    void HiResClock::Init()
    {
        freq_ = platform::QueryHiResFrequency();
        NOC_ASSERT(freq_ != 0);
    }

    uint64_t HiResClock::NowTicks() const
    {
        return platform::QueryHiResCounter();
    }

    double HiResClock::TicksToSeconds(uint64_t dtTicks) const
    {
        return static_cast<double>(dtTicks) / static_cast<double>(freq_);
    }

    // ----- TimeSystem -----

    void TimeSystem::Init()
    {
        clock_.Init();
        startTicks_ = clock_.NowTicks();
        frameBeginTicks_ = startTicks_;
        lastFrameEndTicks_ = startTicks_;

        dtSeconds_ = fallbackDtSeconds_;
    }

    void TimeSystem::BeginFrame()
    {
        frameBeginTicks_ = clock_.NowTicks();
    }

    void TimeSystem::EndFrame()
    {
        const uint64_t endTicks = clock_.NowTicks();
        const uint64_t dtTicks = endTicks - lastFrameEndTicks_;

        double dt = clock_.TicksToSeconds(dtTicks);

        // Clamp huge spikes (debugger breakpoints, stalls)
        if (dt > static_cast<double>(maxDtSeconds_))
            dt = static_cast<double>(fallbackDtSeconds_);

        dtSeconds_ = static_cast<float>(dt);
        lastFrameEndTicks_ = endTicks;
    }

    double TimeSystem::SecondsSinceStart() const
    {
        const uint64_t now = clock_.NowTicks();
        const uint64_t dtTicks = now - startTicks_;
        return clock_.TicksToSeconds(dtTicks);
    }

} // namespace noc
