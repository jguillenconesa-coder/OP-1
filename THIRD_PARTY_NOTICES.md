# Third-party notices / avisos de terceros

Pocket One's original source code is distributed under **AGPL-3.0-only**. See [LICENSE](LICENSE). Copyright (C) 2026 Pocket One contributors. Third-party components retain their original copyrights and licences.

## JUCE 8.0.6

Copyright Raw Material Software Limited and contributors. Pinned source: `51a8a6d7aeae7326956d747737ccf1575e61e209`.

This public open-source distribution uses JUCE's **AGPLv3 option**. It does not claim a commercial JUCE licence. JUCE's complete licence overview, dependency notices and upstream licence links are preserved in [licenses/JUCE-LICENSE.md](licenses/JUCE-LICENSE.md). The corresponding-source archive includes the complete pinned JUCE source tree, including the original per-file notices and dependency licence files.

Upstream source: https://github.com/juce-framework/JUCE/tree/51a8a6d7aeae7326956d747737ccf1575e61e209

JUCE embeds third-party code such as audio codecs, image/font rendering libraries and the VST3 SDK. Their licence files remain in the bundled JUCE source. Do not remove those notices when redistributing it.

## Steinberg VST3 SDK

Copyright Steinberg Media Technologies GmbH and contributors. The SDK version bundled by the pinned JUCE revision offers GPLv3 or a proprietary licence. This open-source build uses the **GPLv3 option**, combined with AGPLv3 as permitted by GPLv3 section 13. No proprietary Steinberg licence or endorsement is claimed.

See [licenses/VST3-SDK-LICENSE.txt](licenses/VST3-SDK-LICENSE.txt), [licenses/GPL-3.0.txt](licenses/GPL-3.0.txt) and the SDK's per-file notices in the corresponding-source archive. VST is a trademark of Steinberg Media Technologies GmbH.

## NSIS installer

NSIS and its installation components retain their authors' copyrights and licence terms. The package's copyright/licence notices are preserved in [licenses/NSIS-COPYRIGHT.txt](licenses/NSIS-COPYRIGHT.txt). The original project is https://nsis.sourceforge.io/ .

## Compiler/runtime and test tools

Microsoft CRT and Windows SDK are used under Microsoft's applicable terms. They are not relicensed as Pocket One source code. Compiler, SDK and system-library source are not included in the source archive. The installer contains the instrument and its statically linked runtime, not a development-tool installer.

LLVM, xwin, CMake, Ninja, GCC, Wine and Xvfb are build/test tools; their executables and SDK download caches are not shipped in the application downloads.

## Design references

No code, samples, fonts, artwork or logos from Teenage Engineering, OTTO-1 or TOP-1 have been incorporated. Their use here is design reference only. See [CREDITS.md](CREDITS.md) and [docs/references.md](docs/references.md).

## Corresponding source and rebuilding

The `Pocket-One-0.2.0-corresponding-source.tar.gz` download contains this project's source, build/installer scripts, licence notices and the complete pinned JUCE source. Its root `BUILD-FROM-SOURCE.md` explains offline JUCE selection. Native Windows builds still require the normal Windows compiler/SDK installation. The archive does not contain credentials, machine-specific caches, private Git metadata or unrelated workspace files.

## Original factory samples

The 28 WAVs in `Assets/Factory` were procedurally generated for Pocket One and are dedicated under CC0-1.0. They contain no OP-1 factory recordings. The generator is project code under AGPL-3.0-only. See `Assets/Factory/README.md`.


## Additional analysis references (0.8.0)

op1-sdk (MIT), U-Boot Blackfin loader definitions and op1-decryptor (no license
declared) were consulted as research references only. No source from them is
copied or linked into the product. Revisions and scope are in CREDITS.md and
docs/dsp-research.md. Numeric control/range observations do not include firmware
instructions, and the native mode contains independently written code.


## GNU simulator patch and numeric observations (0.9.0)

The local Blackfin research patch retains GNU GDB 13.1's GPL-3.0-or-later terms
(FSF / Analog Devices). It is a tool-source patch, not a plugin dependency or
shipped simulator binary. Coefficient values and oracle traces are numeric
research observations; the implementation is independently written. Original
firmware images/instructions and decryption keys are not included. See CREDITS.md.
