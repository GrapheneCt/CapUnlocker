# CapUnlocker

A taiHEN kernel plugin that lifts some limits the PS Vita kernel puts on game
apps, so homebrew can use them:

| Unlock | Setting | What it allows | Patched module | How |
|---|---|---|---|---|
| Virtual drives | `mount` | mounting virtual drives such as `cache0:` | SceAppMgr | function hook at `+0x15D54` |
| 4th core | `core` | putting game threads on the 4th CPU core | SceKernelThreadMgr | function hook at `+0x114C` |
| VM thread functions | `vm` | the ForVM / ForMono threadmgr functions | SceKernelThreadMgr | hook of its `strcmp` import |
| Thread attributes | `thread_attr` | every thread attribute | SceKernelThreadMgr | 2-byte branch patch, checked first |

Every unlock applies to every app you run, not only to the app that needs it.
Turn off the ones you do not use (see [Settings](#settings)).

## Install

1. Copy `CapUnlocker.skprx` to `ur0:tai/`.
2. Add its path under `*KERNEL` in your taiHEN config (`ur0:tai/config.txt`, or
   `ux0:tai/config.txt` if you use that one):

   ```
   *KERNEL
   ur0:tai/CapUnlocker.skprx
   ```

3. Reboot.

To check that it worked, open `ur0:data/capunlocker_log.txt` after the reboot.
It lists each unlock as `hooked` / `patched` or says why it was skipped.

## Settings

Without a settings file every unlock is on. To change that, copy
[`capunlocker.txt`](capunlocker.txt) to `ux0:tai/` or `ur0:tai/`, edit it, and
reboot. `ux0:tai/` is read first, the same order taiHEN uses for `config.txt`.
If your memory card is not mounted as `ux0:` when kernel plugins load (some
SD2Vita and StorageMgr setups), use `ur0:tai/`.

```
mount = 1
core = 1
vm = 1
thread_attr = 1
log = 1
```

Values can be `1`/`0`, `on`/`off`, `yes`/`no` or `true`/`false`. Text after `#`
is a comment. Unknown lines are ignored. Set `log = 0` to stop writing
`ur0:data/capunlocker_log.txt` at every boot.

## Supported firmware

The offsets come from the original author and are not documented per
firmware. Users have reported running the plugin on 3.65 with Enso. The
thread-attribute patch has one offset for 3.60–3.61 and another for later
firmware.

- **Thread attributes** is safe on an unknown firmware: before patching, the
  plugin checks that the expected conditional branch is at that offset. If it
  is not, the log says `unexpected bytes ... skipped` and nothing is written.
- **Virtual drives** and **4th core** hook a fixed offset without such a check.
  The log records the module NID and the first bytes at each offset so a
  problem on another firmware can be reported precisely.
- **VM thread functions** hooks an import by NID, which does not depend on the
  firmware layout.

## Troubleshooting

Version 1.3 had reports of a black screen and freeze after closing Adrenaline
or another ABM bubble ([#3](https://github.com/GrapheneCt/CapUnlocker/issues/3)),
and of a Vita that woke up at night and did not go back to sleep
([#4](https://github.com/GrapheneCt/CapUnlocker/issues/4)). If you see
something like this:

1. Set one unlock to `0` in `capunlocker.txt`, reboot, and test again. Repeat
   until the problem goes away; that unlock is the cause.
2. If the Vita no longer boots far enough to edit the file, remove the plugin
   line from the taiHEN config with your usual recovery method.
3. Report it with your firmware version, your `ur0:data/capunlocker_log.txt`,
   and which unlock caused the problem.

## Building

You need [VitaSDK](https://vitasdk.org), which includes the taiHEN headers and
stubs.

```sh
cmake -S . -B build
cmake --build build          # build/CapUnlocker.skprx
```

The settings parser and the branch check are plain C in `capu_logic.c` and have
host tests (built with AddressSanitizer and UBSan):

```sh
make -C tests
```

GitHub Actions runs the host tests and builds the plugin in the
`vitasdk/vitasdk` container on every push and pull request; the `.skprx` and
the sample `capunlocker.txt` are uploaded as build artifacts.

### Source layout

| File | Contents |
|---|---|
| `main.c` | module start/stop, hooks, logging |
| `capu_logic.c` / `.h` | settings parser and Thumb branch checks, no kernel calls |
| `exports.yml` | module name, version and entry points for `vita-elf-create` |
| `capunlocker.txt` | sample settings file |
| `tests/` | host unit tests |

## Changes in 1.5

- Builds with VitaSDK (CMake + `exports.yml`). The 1.4 build used Sony's SDK
  (VDS) and `lib.emd`; that build was removed because it was not maintained
  alongside the new code. It is still in the git history.
- `capunlocker.txt` turns each unlock on or off.
- Boot log in `ur0:data/capunlocker_log.txt` and on the kernel console, with
  module NIDs, firmware version and the bytes at each patch target.
- The thread-attribute patch checks for the expected instruction before
  writing and leaves unknown firmware untouched.
- Error handling: a missing module no longer leads to hooking with a stale
  module ID; only hooks that succeeded are released on stop; the plugin
  unloads itself if nothing was applied.
- Host tests and GitHub Actions CI.

## Credits

Written by [GrapheneCt](https://github.com/GrapheneCt). Uses
[taiHEN](https://github.com/yifanlu/taiHEN). MIT licence, see
[LICENSE](LICENSE).
