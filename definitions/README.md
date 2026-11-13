# Lua definitions

These LuaLS stubs are maintained manually for the C bindings. They are editor
metadata, not Lua modules to load at runtime. Availability of globals depends
on which modules the host binds. The directory layout retains the old C++
module names; it does not describe the current source tree.

`lua-defgen` extracts explicit `/*@` annotation blocks from C/C++ sources;
it does not infer Lua registrations or verify parity. The rewritten C sources
currently have no such blocks, so running it against `sources` reports an error
without changing these definitions. If annotations are added, it overwrites
corresponding output files but retains unrelated and stale files; review both
generated changes and stale definitions manually.

Transform position, scale and origin are live vector references with `x` and
`y` fields, not ordinary `Vec2` values: they do not implement Vec2 arithmetic
or `scale`. The stubs cannot fully express the different types accepted when
assigning those fields versus reading them. Component handles may also become
invalid after entity/component removal; static types cannot enforce lifetime.
