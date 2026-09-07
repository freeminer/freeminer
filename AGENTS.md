This project is a fork of https://github.com/luanti-org/luanti.git.
Upstream changes are merged for every release. The current merge tag is 5.17.0.

## Rules

- Minimize the diff from the upstream merge tag.
- Put substantial changes in new files named `fm_*` whenever possible.
- Freeminer specific files named "fm_*" or located in src/mapgen/earth
- Including implementation files with `#include "fm_*.cpp"` is allowed for non Freeminer files.
- After changing any Freeminer files, format them with `clang-format`.
- In original non Freeminer files, mark each diff to upsteam release section with `// fm:` at the beginning and `// ===` at the end. Combine sections if possible if already exists.
- In changed code, for nodes, blocks, objects always use `pos_t`, `v3pos_t`, `v3bpos_t`, and `v3opos_t` instead of `v3s16`, `s16`, and `v3f`. s16 and v3f sometimes accepptable for textures/draw calls.
- Minimize compilation checks while working. Compile once at the end with `-j16`.
