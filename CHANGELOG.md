# Changelog

Biçim: [Keep a Changelog](https://keepachangelog.com/). Sürümler [SemVer](https://semver.org/).

## [Unreleased]

## [0.1.0] — 2026-11-21 — M0

### Added
- Modüler CMake sistemi: `astral_add_module`, `astral_add_tests`, preset'ler (debug/release/tsan).
- `Astral::Core`: `Logger` + sink'ler, `HandlePool`, `Clock`, `JobSystem`.
- Kurulum/export: `find_package(Astral CONFIG COMPONENTS Core)`, tüketici testi.
- `examples/hello_core`, GitHub Actions CI (GCC, Clang, MSVC, TSan).
- Dokümantasyon sistemi: mimari, yol haritası, süreç, modül dokümanları, ADR'ler.

### Fixed
- `JobSystem::wait()` başka grubu bekleyen bir job yüzünden kilitlenebiliyordu (ADR-0004).
