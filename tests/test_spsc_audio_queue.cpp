#include "../Source/SpscAudioBlockQueue.h"
#include <array>
#include <atomic>
#include <iostream>
#include <thread>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL: " << #x << " line " << __LINE__ << std::endl; return 1; } } while (false)

int main() {
    using Queue = DbStudioAudio::SpscAudioBlockQueue<2, 4, 2>;
    Queue queue;
    Queue::BlockView view{};
    CHECK(!queue.tryAcquire(view));

    std::array<float, 4> left{{1,2,3,4}}, right{{5,6,7,8}};
    const float* channels[] = {left.data(), right.data()};
    CHECK(queue.tryPush(channels, 4));
    CHECK(queue.tryPush(channels, 2));
    CHECK(!queue.tryPush(channels, 4));
    CHECK(queue.droppedBlocks() == 1);

    CHECK(queue.tryAcquire(view));
    CHECK(view.frames == 4 && view.sequence == 0);
    CHECK(view.samples[0] == 1 && view.samples[3] == 4);
    CHECK(view.samples[4] == 5 && view.samples[7] == 8);
    CHECK(!queue.tryAcquire(view)); // only one outstanding block per consumer
    queue.release();
    queue.release(); // duplicate release is harmless
    CHECK(queue.tryAcquire(view));
    CHECK(view.frames == 2 && view.sequence == 1);
    queue.release();
    CHECK(!queue.tryAcquire(view));

    CHECK(!queue.tryPush(channels, 0));
    CHECK(!queue.tryPush(channels, 5));
    CHECK(!queue.tryPush(nullptr, 2));
    const float* missing[] = {left.data(), nullptr};
    CHECK(!queue.tryPush(missing, 2));

    // Exercise the exact production dimensions: 32 input-derived stereo buses,
    // 8192-frame maximum block and three preallocated slots. Keep the large
    // fixtures off the Windows default thread stack.
    using ProductQueue = DbStudioAudio::SpscAudioBlockQueue<32, 8192, 3>;
    static ProductQueue productQueue;
    static std::array<std::array<float, 8192>, 32> productSource{};
    std::array<const float*, 32> productChannels{};
    for (std::size_t channel = 0; channel < productSource.size(); ++channel) {
        productChannels[channel] = productSource[channel].data();
        for (std::size_t frame = 0; frame < productSource[channel].size(); ++frame)
            productSource[channel][frame] = static_cast<float>(channel * 10000 + frame);
    }
    CHECK(productQueue.tryPush(productChannels.data(), 8192));
    ProductQueue::BlockView productView{};
    CHECK(productQueue.tryAcquire(productView));
    CHECK(productView.frames == 8192 && productView.sequence == 0);
    for (std::size_t channel = 0; channel < productSource.size(); ++channel) {
        const auto offset = channel * 8192;
        CHECK(productView.samples[offset] == static_cast<float>(channel * 10000));
        CHECK(productView.samples[offset + 4095] == static_cast<float>(channel * 10000 + 4095));
        CHECK(productView.samples[offset + 8191] == static_cast<float>(channel * 10000 + 8191));
    }
    productQueue.release();
    CHECK(!productQueue.tryAcquire(productView));

    // Stress the actual single-producer/single-consumer ownership contract.
    Queue concurrentQueue;
    constexpr std::uint64_t iterations = 30000;
    std::array<float, 4> producerLeft{}, producerRight{};
    const float* producerChannels[] = {producerLeft.data(), producerRight.data()};
    std::atomic<bool> producerFailed{false};

    std::thread producer([&] {
        for (std::uint64_t sequence = 0; sequence < iterations;) {
            producerLeft.fill(static_cast<float>(sequence));
            producerRight.fill(-static_cast<float>(sequence));
            if (concurrentQueue.tryPush(producerChannels, 4))
                ++sequence;
            else
                std::this_thread::yield();
        }
    });

    std::uint64_t consumed = 0;
    while (consumed < iterations) {
        if (!concurrentQueue.tryAcquire(view)) {
            std::this_thread::yield();
            continue;
        }
        const bool valid = view.frames == 4 && view.sequence == consumed &&
            view.samples[0] == static_cast<float>(consumed) &&
            view.samples[4] == -static_cast<float>(consumed);
        if (!valid) producerFailed.store(true, std::memory_order_relaxed);
        concurrentQueue.release();
        ++consumed;
    }
    producer.join();
    CHECK(!producerFailed.load(std::memory_order_relaxed));
    return 0;
}
