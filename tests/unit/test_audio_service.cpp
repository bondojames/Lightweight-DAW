// SPDX-License-Identifier: GPL-3.0-or-later
// Device-failure paths that real hardware can't be made to hit on demand, driven through
// the same AudioService the app uses, with a scripted backend in place of WASAPI.
#include "engine/device/AudioService.h"
#include "tests/unit/test_framework.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

using namespace ldaw::engine;

namespace {

struct FakeWorld {
    std::mutex mutex;
    std::vector<AudioDeviceInfo> devices;
    bool failNextStart = false;
    std::string startError = "Another app has exclusive control of this audio device.";
    AudioStatus status = AudioStatus::NotInitialized;
    std::string lastStartedId;
    int startCount = 0;
    bool devicesChanged = false;
};

class FakeBackend final : public IAudioBackend {
public:
    explicit FakeBackend(FakeWorld& world) : world_(world) {}

    const char* name() const override { return "fake"; }
    std::vector<AudioDeviceInfo> enumerateOutputs() override {
        const std::scoped_lock lock(world_.mutex);
        return world_.devices;
    }
    bool start(const AudioStreamConfig& config, IAudioProcessor& processor, AudioTelemetry&) override {
        const std::scoped_lock lock(world_.mutex);
        ++world_.startCount;
        if (world_.failNextStart) {
            world_.failNextStart = false;
            world_.status = AudioStatus::Failed;
            return false;
        }
        processor.prepare(48000.0, 512);
        world_.lastStartedId = config.outputDeviceId;
        world_.status = AudioStatus::Running;
        return true;
    }
    void stop() override {
        const std::scoped_lock lock(world_.mutex);
        if (world_.status == AudioStatus::Running) world_.status = AudioStatus::Stopped;
    }
    AudioStatus status() const override {
        const std::scoped_lock lock(world_.mutex);
        return world_.status;
    }
    AudioStreamInfo streamInfo() const override {
        AudioStreamInfo info;
        info.deviceName = "Fake";
        info.sampleRate = 48000.0;
        info.periodFrames = 256;
        return info;
    }
    std::string lastError() const override {
        const std::scoped_lock lock(world_.mutex);
        return world_.startError;
    }
    bool takeDevicesChanged() override {
        const std::scoped_lock lock(world_.mutex);
        const bool changed = world_.devicesChanged;
        world_.devicesChanged = false;
        return changed;
    }

private:
    FakeWorld& world_;
};

bool waitFor(const std::function<bool()>& condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return condition();
}

const AudioDeviceInfo kSpeakers{"speakers", "Speakers", true};
const AudioDeviceInfo kHeadset{"headset", "Headset", false};

} // namespace

TEST_CASE(no_audio_device_gives_a_clear_message_and_no_crash) {
    FakeWorld world;
    AudioService service(std::make_unique<FakeBackend>(world));
    service.open({});
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Failed; }));
    CHECK(service.state().message.find("offline test") != std::string::npos);
    CHECK(world.startCount == 0);
}

TEST_CASE(a_device_that_appears_later_is_picked_up) {
    FakeWorld world;
    AudioService service(std::make_unique<FakeBackend>(world));
    service.open({});
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Failed; }));
    {
        const std::scoped_lock lock(world.mutex);
        world.devices = {kSpeakers};
        world.status = AudioStatus::Failed;
        world.devicesChanged = true;
    }
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));
    CHECK(service.state().message.empty());
}

TEST_CASE(a_saved_device_that_is_gone_falls_back_and_says_so) {
    FakeWorld world;
    world.devices = {kSpeakers};
    AudioService service(std::make_unique<FakeBackend>(world));
    AudioStreamConfig config;
    config.outputDeviceId = "headset";
    service.open(config);
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));
    CHECK(service.state().message.find("isn't connected") != std::string::npos);
    const std::scoped_lock lock(world.mutex);
    CHECK(world.lastStartedId.empty());
}

TEST_CASE(a_failed_open_reports_the_reason) {
    FakeWorld world;
    world.devices = {kSpeakers};
    world.failNextStart = true;
    AudioService service(std::make_unique<FakeBackend>(world));
    service.open({});
    CHECK(waitFor([&] { return service.state().message.find("exclusive") != std::string::npos; }));
    // It retries on its own, and this time the device is free.
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));
    CHECK(service.state().message.empty());
}

TEST_CASE(an_unplugged_device_is_reported_then_reconnects) {
    FakeWorld world;
    world.devices = {kSpeakers};
    AudioService service(std::make_unique<FakeBackend>(world));
    service.open({});
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));
    {
        const std::scoped_lock lock(world.mutex);
        world.status = AudioStatus::DeviceLost;
        world.devices.clear();
        world.devicesChanged = true;
    }
    CHECK(waitFor([&] { return service.state().message.find("No audio output device") != std::string::npos ||
                               service.state().message.find("disconnected") != std::string::npos; }));
    {
        const std::scoped_lock lock(world.mutex);
        world.devices = {kSpeakers};
        world.devicesChanged = true;
    }
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));
}

TEST_CASE(a_new_windows_default_is_followed_only_when_using_the_default) {
    FakeWorld world;
    world.devices = {kSpeakers, kHeadset};
    AudioService service(std::make_unique<FakeBackend>(world));
    service.open({});
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));
    const auto swapDefault = [&] {
        const std::scoped_lock lock(world.mutex);
        world.devices = {{"speakers", "Speakers", false}, {"headset", "Headset", true}};
        world.devicesChanged = true;
    };
    int starts = 0;
    {
        const std::scoped_lock lock(world.mutex);
        starts = world.startCount;
    }
    swapDefault();
    CHECK(waitFor([&] {
        const std::scoped_lock lock(world.mutex);
        return world.startCount == starts + 1;
    }));
}

TEST_CASE(choosing_a_device_opens_that_device) {
    FakeWorld world;
    world.devices = {kSpeakers, kHeadset};
    AudioService service(std::make_unique<FakeBackend>(world));
    AudioStreamConfig config;
    config.outputDeviceId = "headset";
    service.open(config);
    CHECK(waitFor([&] {
        const std::scoped_lock lock(world.mutex);
        return world.lastStartedId == "headset";
    }));
}

TEST_CASE(offline_test_needs_no_device_and_reports_a_pass) {
    FakeWorld world; // No devices at all.
    AudioService service(std::make_unique<FakeBackend>(world));
    const auto path = std::filesystem::temp_directory_path() / L"ldaw_offline_test.wav";
    service.runOfflineTest(path);
    CHECK(waitFor([&] { return !service.state().offlineTestResult.empty(); }));
    CHECK(service.state().offlineTestResult.find("PASS") != std::string::npos);
    CHECK(std::filesystem::exists(path));
    StereoBuffer back;
    std::uint32_t rate = 0;
    CHECK(readWavFloat32(path, back, rate));
    CHECK(rate == 48000);
    CHECK(back.left.size() == 16u * 48000u);
    std::filesystem::remove(path);
}

TEST_CASE(a_project_reaches_the_audio_thread_after_import) {
    FakeWorld world;
    world.devices = {kSpeakers};
    const auto cache = std::filesystem::temp_directory_path() / L"ldaw_service_cache";
    AudioService service(std::make_unique<FakeBackend>(world), cache);
    service.open({});
    CHECK(waitFor([&] { return service.state().status == AudioStatus::Running; }));

    StereoBuffer tone;
    tone.left.assign(48000, 0.25f);
    tone.right.assign(48000, 0.25f);
    const auto path = std::filesystem::temp_directory_path() / L"ldaw_service_tone.wav";
    CHECK(writeWavFloat32(path, tone, 48000));

    service.media().requestImport(7, path);
    std::vector<ImportResult> results;
    CHECK(waitFor([&] {
        results = service.media().takeResults();
        return !results.empty();
    }));
    CHECK(results[0].token == 7);
    CHECK(results[0].ok);
    CHECK(results[0].framesAtSourceRate == 48000);
    CHECK(service.media().peaksFor(path) != nullptr);
    CHECK(service.media().runtimeFor(path) != nullptr);
    std::filesystem::remove(path);
}

TEST_CASE(a_bad_import_reports_its_error) {
    FakeWorld world;
    AudioService service(std::make_unique<FakeBackend>(world), std::filesystem::temp_directory_path() / L"ldaw_service_cache");
    service.media().requestImport(9, L"Z:/no/such/file.wav");
    std::vector<ImportResult> results;
    CHECK(waitFor([&] {
        results = service.media().takeResults();
        return !results.empty();
    }));
    CHECK(!results[0].ok);
    CHECK(!results[0].error.empty());
}
