#include <doctest/doctest.h>

#include "Astral/Core/Handle.hpp"
#include "Astral/Core/JobSystem.hpp"
#include "Astral/Engine/Platform/Input.hpp"

#include <atomic>

using namespace Astral;

namespace {
using TestHandle = Handle<struct TestTag>;
}

TEST_CASE("HandlePool invalidates handles after destroy, even when the slot is reused") {
    HandlePool<TestTag, int> pool;
    TestHandle a = pool.Create(1);
    CHECK(pool.Contains(a));
    CHECK(pool.Get(a) == 1);

    CHECK(pool.Destroy(a));
    CHECK_FALSE(pool.Contains(a));
    CHECK_FALSE(pool.Destroy(a));

    TestHandle b = pool.Create(2);
    CHECK(b.index == a.index);           // slot reused
    CHECK(b.generation != a.generation); // but a new generation
    CHECK_FALSE(pool.Contains(a));
    CHECK(pool.TryGet(a) == nullptr);
    CHECK(pool.AliveCount() == 1);
}

TEST_CASE("Handle raw round trip and null handle") {
    TestHandle h{7, 3};
    CHECK(TestHandle::FromRaw(h.Raw()) == h);
    CHECK_FALSE(TestHandle{}.IsValid());
}

TEST_CASE("JobSystem runs every submitted job") {
    JobSystem jobs;
    jobs.Initialize(4);
    std::atomic<int> sum{0};
    jobs.ParallelFor(100, 7, [&](size_t i) { sum += int(i) + 1; });
    CHECK(sum == 5050);
    jobs.Shutdown();
}

TEST_CASE("Input edges and axes") {
    Input input;
    input.BindAxis("MoveX", Key::D, Key::A);
    input.BindAction("Jump", Key::Space);

    input.BeginFrame();
    input.SetKey(int(Key::D), true);
    input.SetKey(int(Key::Space), true);
    CHECK(input.GetAxis("MoveX") == doctest::Approx(1.0f));
    CHECK(input.WasActionPressed("Jump"));

    input.BeginFrame(); // next frame, still held
    CHECK(input.IsActionDown("Jump"));
    CHECK_FALSE(input.WasActionPressed("Jump"));
    input.SetKey(int(Key::A), true);
    CHECK(input.GetAxis("MoveX") == doctest::Approx(0.0f));

    input.BindAction("Fire", MouseButton::Left);
    CHECK_FALSE(input.IsActionDown("Fire"));
    input.SetMouseButton(int(MouseButton::Left), true);
    CHECK(input.WasActionPressed("Fire"));
}
