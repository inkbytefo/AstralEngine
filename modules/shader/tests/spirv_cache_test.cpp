#include "sha256.hpp"

#include <astral/core/jobs.hpp>
#include <astral/shader/compile.hpp>
#include <astral/shader/spirv_cache.hpp>

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace astral;
namespace fs = std::filesystem;

namespace {

/// A fresh, empty directory under the system temp folder, removed afterwards.
struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name) {
        static std::atomic<unsigned> counter{0};
        path = fs::temp_directory_path() / ("astral_spirv_cache_" + name + "_" +
                                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                                            std::to_string(counter++));
        fs::remove_all(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

constexpr const char* kFragment = "#version 460\n#extension GL_GOOGLE_include_directive : require\n"
                                  "#include <astral/k.glsl>\n"
                                  "layout(location = 0) out vec4 color;\n"
                                  "void main() { color = vec4(k_value()); }\n";

std::vector<GlslInclude> includes_with(const std::string& body) {
    return {{"astral/k.glsl", "float k_value() { return " + body + "; }\n"}, {"astral/unused.glsl", "// not included\n"}};
}

std::string hex_of(const detail::Sha256Digest& d) { return SpirvCacheKey{d}.hex(); }

std::size_t files_in(const fs::path& dir, const std::string& extension) {
    std::size_t n = 0;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) n += e.path().extension() == extension ? 1 : 0;
    return n;
}

std::vector<char> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_file(const fs::path& p, const std::vector<char>& bytes) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

} // namespace

TEST_CASE("sha256 matches the FIPS 180-4 known answers") {
    using detail::Sha256;
    CHECK(hex_of(Sha256().finish()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    Sha256 abc;
    abc.update("abc");
    CHECK(hex_of(abc.finish()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    Sha256 two_blocks; // 56 bytes: the padding spills into a second block
    two_blocks.update("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    CHECK(hex_of(two_blocks.finish()) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    Sha256 split; // feeding in odd pieces equals one call
    split.update("abcdbcdecdefdefgefghfghighijhijkijkl");
    split.update("jklmklmnlmnomnopnopq");
    CHECK(hex_of(split.finish()) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("a warm start answers from disk and runs glslang zero times") {
    TempDir dir("warm");
    const auto inc = includes_with("0.5");
    GlslCompile cold, warm;
    {
        SpirvCache cache(dir.path);
        cold = compile_glsl(kFragment, ShaderStage::Fragment, inc, "warm.frag", {.cache = &cache});
        REQUIRE_MESSAGE(cold.ok(), cold.log);
        CHECK_FALSE(cold.from_cache);
        CHECK(cache.stats().misses == 1);
        CHECK(cache.stats().stores == 1);
    }
    SpirvCache next_launch(dir.path); // a new process: nothing in memory, only the folder
    warm = compile_glsl(kFragment, ShaderStage::Fragment, inc, "warm.frag", {.cache = &next_launch});
    REQUIRE(warm.ok());
    CHECK(warm.from_cache);
    CHECK(next_launch.stats().hits == 1);
    CHECK(next_launch.stats().misses == 0);
    CHECK(next_launch.stats().stores == 0);
    CHECK(warm.spirv == cold.spirv);
    CHECK(warm.spirv == compile_glsl(kFragment, ShaderStage::Fragment, inc, "warm.frag").spirv); // = no cache at all
}

TEST_CASE("damaged cache entries recompile safely and are repaired") {
    TempDir dir("damage");
    SpirvCache cache(dir.path);
    const auto inc = includes_with("0.25");
    const GlslCompile reference = compile_glsl(kFragment, ShaderStage::Fragment, inc, "damage.frag", {.cache = &cache});
    REQUIRE(reference.ok());
    const fs::path file = [&] {
        for (const auto& e : fs::directory_iterator(dir.path)) {
            if (e.path().extension() == ".spv") return e.path();
        }
        return fs::path{};
    }();
    REQUIRE_FALSE(file.empty());
    const std::vector<char> good = read_file(file);
    REQUIRE(good.size() > 64);

    std::vector<std::vector<char>> damages;
    damages.emplace_back();                                                   // empty file
    damages.emplace_back(good.begin(), good.begin() + 10);                    // cut inside the header
    damages.emplace_back(good.begin(), good.end() - 7);                       // truncated payload
    damages.emplace_back(good);                                               // trailing garbage
    damages.back().push_back('x');
    damages.emplace_back(good);                                               // one flipped payload bit
    damages.back()[good.size() - 3] = static_cast<char>(damages.back()[good.size() - 3] ^ 0x10);
    damages.emplace_back(good);                                               // wrong format version
    damages.back()[4] = static_cast<char>(damages.back()[4] + 1);
    damages.emplace_back(std::vector<char>(good.size(), '\xAB'));             // garbage of the right size
    for (std::size_t i = 0; i < damages.size(); ++i) {
        CAPTURE(i);
        write_file(file, damages[i]);
        const std::uint64_t rejected_before = cache.stats().rejected;
        const GlslCompile r = compile_glsl(kFragment, ShaderStage::Fragment, inc, "damage.frag", {.cache = &cache});
        REQUIRE(r.ok());
        CHECK_FALSE(r.from_cache);
        CHECK(r.spirv == reference.spirv);
        CHECK(cache.stats().rejected == rejected_before + 1);
        const GlslCompile healed = compile_glsl(kFragment, ShaderStage::Fragment, inc, "damage.frag", {.cache = &cache});
        CHECK(healed.from_cache); // the bad file was overwritten
        CHECK(healed.spirv == reference.spirv);
    }
}

TEST_CASE("the cache key changes with an include, the source, the stage and the name") {
    TempDir dir("key");
    SpirvCache cache(dir.path);
    const auto a = includes_with("0.5");
    const auto b = includes_with("0.75");
    auto compile = [&](const char* src, ShaderStage stage, const std::vector<GlslInclude>& inc, const char* name) {
        return compile_glsl(src, stage, inc, name, {.cache = &cache});
    };
    CHECK_FALSE(compile(kFragment, ShaderStage::Fragment, a, "k").from_cache);
    CHECK(compile(kFragment, ShaderStage::Fragment, a, "k").from_cache);
    const GlslCompile changed_include = compile(kFragment, ShaderStage::Fragment, b, "k");
    REQUIRE(changed_include.ok());
    CHECK_FALSE(changed_include.from_cache); // a changed include never finds the stale entry...
    CHECK(compile(kFragment, ShaderStage::Fragment, a, "k").from_cache); // ...and the old entry is still there
    CHECK(compile(kFragment, ShaderStage::Fragment, b, "k").from_cache);
    CHECK_FALSE(compile(kFragment, ShaderStage::Fragment, a, "other-name").from_cache);
    const std::string edited = std::string(kFragment) + "// edited\n";
    CHECK_FALSE(compile(edited.c_str(), ShaderStage::Fragment, a, "k").from_cache);
    CHECK(files_in(dir.path, ".spv") == 4);
    // The same text as another stage is another entry (and here simply fails to compile, which is never cached).
    const GlslCompile as_vertex = compile(kFragment, ShaderStage::Vertex, a, "k");
    CHECK_FALSE(as_vertex.from_cache);
    CHECK(files_in(dir.path, ".spv") == (as_vertex.ok() ? 5u : 4u));
}

TEST_CASE("compile errors are never cached") {
    TempDir dir("errors");
    SpirvCache cache(dir.path);
    for (int i = 0; i < 2; ++i) {
        const GlslCompile r = compile_glsl("#version 460\nvoid main() { nope(); }\n", ShaderStage::Fragment, {}, "bad", {.cache = &cache});
        CHECK_FALSE(r.ok());
        CHECK_FALSE(r.from_cache);
        CHECK_FALSE(r.errors.empty());
    }
    CHECK(cache.stats().stores == 0);
    CHECK(files_in(dir.path, ".spv") == 0);
}

TEST_CASE("an unusable cache folder never fails a compile") {
    TempDir dir("blocked");
    fs::create_directories(dir.path);
    write_file(dir.path / "not_a_folder", {'x'});
    SpirvCache blocked(dir.path / "not_a_folder"); // a file where the folder should be
    const GlslCompile r = compile_glsl(kFragment, ShaderStage::Fragment, includes_with("1.0"), "blocked", {.cache = &blocked});
    CHECK(r.ok());
    CHECK(blocked.stats().stores == 0);
    SpirvCache disabled{fs::path{}}; // an empty path: never hits, never writes
    CHECK(compile_glsl(kFragment, ShaderStage::Fragment, includes_with("1.0"), "off", {.cache = &disabled}).ok());
    CHECK(disabled.stats().hits == 0);
}

TEST_CASE("parallel batch compiles equal serial ones, in request order") {
    const auto inc = includes_with("0.125");
    std::vector<GlslRequest> requests;
    for (int i = 0; i < 9; ++i) {
        std::string src = kFragment;
        src += "// variant " + std::to_string(i) + "\n";
        if (i == 4) src += "void broken() { nope(); }\n"; // one failure in the middle
        requests.push_back({src, ShaderStage::Fragment, "batch" + std::to_string(i)});
    }
    const std::vector<GlslCompile> serial = compile_glsl_batch(requests, inc);
    JobSystem jobs(4);
    const std::vector<GlslCompile> parallel = compile_glsl_batch(requests, inc, {.jobs = &jobs});
    REQUIRE(serial.size() == requests.size());
    REQUIRE(parallel.size() == requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
        CAPTURE(i);
        CHECK(parallel[i].spirv == serial[i].spirv);
        CHECK(parallel[i].ok() == serial[i].ok());
        CHECK(parallel[i].errors.size() == serial[i].errors.size());
        CHECK(serial[i].ok() == (i != 4));
        // And each equals a plain compile_glsl of the same request.
        CHECK(serial[i].spirv == compile_glsl(requests[i].source, requests[i].stage, inc, requests[i].name).spirv);
    }
}

TEST_CASE("a parallel cold batch writes the cache and a parallel warm batch compiles nothing") {
    TempDir dir("batch");
    const auto inc = includes_with("0.0625");
    std::vector<GlslRequest> requests;
    for (int i = 0; i < 12; ++i) requests.push_back({std::string(kFragment) + "// " + std::to_string(i) + "\n", ShaderStage::Fragment, "b"});
    JobSystem jobs(4);
    SpirvCache cold_cache(dir.path);
    const auto cold = compile_glsl_batch(requests, inc, {.cache = &cold_cache, .jobs = &jobs});
    CHECK(cold_cache.stats().stores == 12);
    SpirvCache warm_cache(dir.path);
    const auto warm = compile_glsl_batch(requests, inc, {.cache = &warm_cache, .jobs = &jobs});
    CHECK(warm_cache.stats().hits == 12);
    CHECK(warm_cache.stats().misses == 0);
    for (std::size_t i = 0; i < requests.size(); ++i) {
        CHECK(warm[i].from_cache);
        CHECK(warm[i].spirv == cold[i].spirv);
    }
    CHECK(files_in(dir.path, ".tmp") == 0); // atomic writes leave no temporaries behind
}

TEST_CASE("threads racing on one key all get the same SPIR-V") {
    TempDir dir("race");
    SpirvCache cache(dir.path);
    const auto inc = includes_with("0.03125");
    std::vector<std::vector<std::uint32_t>> results(8);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([&, i] { results[i] = compile_glsl(kFragment, ShaderStage::Fragment, inc, "race", {.cache = &cache}).spirv; });
    }
    for (std::thread& t : threads) t.join();
    for (const auto& r : results) {
        REQUIRE_FALSE(r.empty());
        CHECK(r == results[0]);
    }
    CHECK(files_in(dir.path, ".spv") == 1);
    CHECK(files_in(dir.path, ".tmp") == 0);
    CHECK(compile_glsl(kFragment, ShaderStage::Fragment, inc, "race", {.cache = &cache}).from_cache);
}

TEST_CASE("prune drops the least recently used entries and a hit counts as a use") {
    TempDir dir("prune");
    SpirvCache cache(dir.path);
    const auto inc = includes_with("0.5");
    auto source = [](int i) { return std::string(kFragment) + "// entry " + std::to_string(i) + "\n"; };
    for (int i = 0; i < 4; ++i) REQUIRE(compile_glsl(source(i), ShaderStage::Fragment, inc, "p", {.cache = &cache}).ok());
    const auto old = fs::file_time_type::clock::now() - std::chrono::hours(48);
    std::uintmax_t entry_size = 0;
    for (const auto& e : fs::directory_iterator(dir.path)) {
        fs::last_write_time(e.path(), old);
        entry_size = fs::file_size(e.path());
    }
    REQUIRE(entry_size > 0);
    REQUIRE(compile_glsl(source(2), ShaderStage::Fragment, inc, "p", {.cache = &cache}).from_cache); // entry 2 is in use again

    const std::uint64_t left = cache.prune(entry_size * 3 / 2); // room for one entry
    CHECK(left <= entry_size * 3 / 2);
    CHECK(files_in(dir.path, ".spv") == 1);
    CHECK(compile_glsl(source(2), ShaderStage::Fragment, inc, "p", {.cache = &cache}).from_cache);
    CHECK_FALSE(compile_glsl(source(0), ShaderStage::Fragment, inc, "p", {.cache = &cache}).from_cache);
    CHECK(cache.prune(1u << 30) > 0); // under the limit: nothing is removed
    CHECK(files_in(dir.path, ".spv") == 2); // entry 0 was written again by the miss above
}
