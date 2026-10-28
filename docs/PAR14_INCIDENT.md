# par14 incident — 2026-10-28

Status: **reported non-booting FM-1; root cause not confirmed**. The owner relayed a tester report. The exact par14 revision, installation path, prior firmware, USB enumeration and power-on symptoms have been requested. “Brick” is the tester's reported outcome, not a diagnosis of permanent bootloader damage.

## Containment

- Hardware installers reject every `0.9.9-par14-*` revision, even if its edition metadata is missing or incorrectly labels it public. Withdrawn package files are not copied to the generated firmware directory.
- Public default is the existing ADDITIVE par10 (`30f05b3`); SAMPLE par13b (`3641c02`) remains unchanged as an alternative.
- Beta default is the already published ADDITIVE par10d-beta2 (`42947e9`), still explicitly beta. This is not evidence that it cannot share a defect with par14: REC and whole-sequence clipboard code are shared.
- SAMPLE development is paused. The browser emulator remains separately pinned to `4d3156d` through `web/EMULATOR`; that pin is not a hardware recommendation.

## Checks completed on the published par14-beta4 package

Package SHA-256: `01883f0f4b16a1d182a0a00de816001893b8cf35b0c22208268bcfd093675208`.

- Published package matched the successful target-CI artifact byte for byte.
- UFW header, entry list, flash header, flash payload and decoded application CRC checks passed.
- Product identity is `FM-1_909`; `flash.bin` is `0x93000` bytes; encrypted content ends at `0x92ffe`, inside that image.
- Application slot is 584336 bytes. The linked application is 584308 bytes: **28 bytes free**. This is very little room, but no overflow was observed. It does not establish runtime safety.
- Target `.data`/`.bss` span is 91788 / 98304 bytes; pool is 305972 bytes, with 4288 bytes spare after core-1 stacks according to the build check. No linker-region overlap was observed. Stack usage at runtime has not been measured on the reported unit.
- OTA loader is 7124 bytes, SHA-256 `ce8721ed1624b36c1b08f160b9d89a591ae42c7f23f9ce82ab963faeefa6a056`, identical to published SAMPLE par13b. The first `0x120` flash bytes also match par13b. Identical loader bytes do not rule out an interrupted update or a runtime/flash failure.
- Sample bank storage is separate, at `0xda000..0xdbfff`. Beta4 added automatic provisioning of an empty bank after firmware installation. Whether the tester reached that stage is unknown; this path has not been confirmed on the reported physical unit.
- Host sequence/undo/bounds tests, the target linker/build budgets, and Chromium audio/screen-refresh tests passed. A timing test retains the same one pre-existing failure as its exact baseline; the comparison is not a claim that all timing tests pass.

## Open investigation

The endpoint overread fixed in beta4 was a confirmed sampler defect, **not a proven explanation of this report**. The screen-only freeze reported earlier is also not yet linked to the non-booting device.

Next evidence: exact ABOUT/build label or installer selection, prior firmware, whether installation completed and stock samples were sent, screen/photo after power-on, and the USB device name/identity. Preserve projects and flash contents. Do not prescribe flash erasure, boot pins, or a blind recovery image without identifying the device's actual state.

Depending on that evidence, review boot initialization/persisted project compatibility, runtime stack guards and crash records, both-core flash/cache synchronization, the post-install sample transfer, and interrupted-update recovery. No specific cause is established by the package checks above.
