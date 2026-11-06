// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace ldaw::engine {

// Marks first-party real-time code on the current thread. Test programs replace
// operator new and delete and count any call made while this is set, which is how
// "no heap allocation in the callback" is proven rather than assumed.
inline thread_local bool g_inRealtimeSection = false;

class RealtimeSection {
public:
    RealtimeSection() { g_inRealtimeSection = true; }
    ~RealtimeSection() { g_inRealtimeSection = false; }
    RealtimeSection(const RealtimeSection&) = delete;
    RealtimeSection& operator=(const RealtimeSection&) = delete;
};

} // namespace ldaw::engine
