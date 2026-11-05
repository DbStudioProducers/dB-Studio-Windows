#include "../Source/SpscAudioBlockQueue.h"
#include <array>
#include <cmath>
#include <iostream>

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
    return 0;
}
