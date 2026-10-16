# cpptoml provenance

The dependency is vendored unchanged in `include/cpptoml.h` and
`src/cpptoml.cpp`, matching the repository's existing dependency layout.

- Upstream: https://github.com/taqu/cpptoml
- Pinned revision: `6f064ef6f030713d5ba88eb58f537a3d5aa125f8`
- Header SHA256: `e79ec130e4e074c30e7d2103d96390db1566549c6321e00798b8342ea57e6aba`
- Source SHA256: `1f474d5942a84b23af547e7e60b8b2bc6ebd54113eed04c710339ffea70c455a`

CMake checks these hashes. Builds use the checked-in sources without network
access or a package manager. To reproduce, retrieve `cpptoml.h` and `cpptoml.cpp`
from the pinned revision and compare the SHA256 values. Upstream README and
LICENSE are included here; ZeroPad uses the MIT license option. The sources
also retain their original license notices. No upstream source changes.
