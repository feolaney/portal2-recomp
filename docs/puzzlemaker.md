# Puzzle Maker on macOS

Puzzle Maker needs both a mounted SDK content tree and native map compilers.
Creating a writable SDK directory alone does not fix VMF export: the Linux
filesystem rejects absolute writes outside its mounted search paths before
calling fopen. `make_app.py` and `run_game.py` add an `SDKCONTENT` search path
to the user-data copy of `portal2/gameinfo.txt`, preserving the Steam original
and any existing user overrides. The custom path ID keeps GAME asset priority
unchanged. The usual overlay redirects exported VMF/BSP files to user data.

Build with your own Linux depot and game files:

```sh
python3 tools/gameprep.py
python3 tools/build_game.py --release
python3 tools/build_puzzlemaker.py
python3 tools/make_app.py
```

`setup.sh` also builds the compilers. Packaging includes
`Contents/MacOS/portal2-map-compiler` and its matching images under
`Contents/Resources/map-compiler/prep`. Keep these private, like the game binary.
The game intercepts child launches of `vbsp_linux`, `vvis_linux`, and `vrad_linux`
and executes the native host with the existing arguments, pipes, and user-data
root. VBSP has a separate layout because its ET_EXEC address range overlaps the
game's libraries. VVIS and VRAD use their `launchable_dll_1` interfaces in that
host. ELF COPY relocations are applied after loading all images and imported
objects, before constructors run. Static compiler BSS stays below the mmap area.

For a repository diagnostic run, the host is found next to `portal2-native`,
with images in `build/tools/vbsp/prep`. `P2_MAP_COMPILER` and
`P2_MAP_COMPILER_IMAGES` override these paths. `P2_TRACE_FILES=preview.vmf`
traces actual file opens; it cannot show a write that the guest filesystem
rejects before fopen. Compiler child launches produce `[map-compiler]` lines.

Chambers and saves remain in their existing directories. These changes do not
migrate or delete them.

Compiler completion also needs SIGCHLD delivery. The runtime defers the host signal to a guest call boundary, invokes the registered guest handler, and restores CPU registers. The game handler reaps the compiler and records its exit status; accepting registration without delivering the signal leaves the editor waiting on geometry. Process completion also invalidates parent overlay lookups after external writes.
