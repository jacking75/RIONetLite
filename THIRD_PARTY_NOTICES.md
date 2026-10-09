# Third-party notices

The RIONetLite project license is in [LICENSE](LICENSE). Dependencies are restored by the root [vcpkg manifest](vcpkg.json); dependency binaries are not checked into this repository.

| Component | Use | License and notice location |
|---|---|---|
| GoogleTest | Unit/integration executables, not the default binary library | BSD-3-Clause; restored vcpkg `share/gtest/copyright` |
| mimalloc | Optional executable allocator; disabled in the default binary package | MIT; restored vcpkg `share/mimalloc/copyright` |
| Windows SDK and MSVC runtime | Platform APIs and `/MD` toolchain dependency | Supplied under their respective Microsoft installation terms |

The unused legacy `samples/thirdparty/flags.h` file was removed from the current source tree. No project included it, and its historical copy had no confirmed upstream identity or license notice. Removal does not establish a license or provenance for copies retained in Git history.

When distributing optional dependency binaries, include the actual restored dependency notices for the pinned version. The default package links neither GoogleTest nor mimalloc into `RIONetLite.lib`.
