#pragma once
#include <cstdint>

namespace noc {

    class HiResClock
    {
    public:
        void Init();
        uint64_t NowTicks() const;
        uint64_t Frequency() const { return freq_; }
        double   TicksToSeconds(uint64_t dtTicks) const;

    private:
        uint64_t freq_ = 0;
    };

    class TimeSystem
    {
    public:
        void Init();

        // Call once per frame
        void BeginFrame();
        void EndFrame();

        float  DeltaSeconds() const { return dtSeconds_; }
        double SecondsSinceStart() const;

        void SetMaxDeltaSeconds(float maxDt) { maxDtSeconds_ = maxDt; }
        void SetBreakpointFallbackDelta(float dt) { fallbackDtSeconds_ = dt; }

    private:
        HiResClock clock_;
        uint64_t startTicks_ = 0;
        uint64_t frameBeginTicks_ = 0;
        uint64_t lastFrameEndTicks_ = 0;

        float dtSeconds_ = 0.0f;

        // Gregory-style breakpoint protection:
        float maxDtSeconds_ = 1.0f;        // if dt > this, assume breakpoint pause
        float fallbackDtSeconds_ = 1.0f / 60.0f;
    };

    TimeSystem& GetTime();

} // namespace noc
