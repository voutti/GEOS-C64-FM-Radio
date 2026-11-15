#!/bin/sh
set -eu

cd "$(dirname "$0")"
GEOS_LIB_DIR="${GEOS_LIB_DIR:-../ccommon/geos}"
COMMON_LIB_DIR="${COMMON_LIB_DIR:-../ccommon}"
OSCAR64="${OSCAR64:-oscar64}"
PYTHON="${PYTHON:-python3}"
VERSION=1.0
AUTHOR="Timo Voutilainen"
# Previous system-disk duplication settings, retained for possible reuse:
# C64_BASE_IMAGE="${C64_BASE_IMAGE:-GEOS64.D81}"
# C128_BASE_IMAGE="${C128_BASE_IMAGE:-GEOS128.D81}"
C64_OUTPUT_IMAGE="${C64_OUTPUT_IMAGE:-build/geos64.d81}"
C128_OUTPUT_IMAGE="${C128_OUTPUT_IMAGE:-build/geos128.d81}"
# NEW_C64_DISK_LABEL="${NEW_C64_DISK_LABEL-}"
# NEW_C128_DISK_LABEL="${NEW_C128_DISK_LABEL-WORK}"
VARIANT=cassette-sda-write
ALL_VARIANTS=0
TARGET=all
BLANK_FORMAT=
COPY_IMAGE=
LS_IMAGE=0
MAKE_RELEASE=0

usage() {
    printf 'Usage: %s [--target c64|c128-80 | --all-targets] [--variant NAME | --all-variants] [--to-blank-d64 | --to-blank-d81 | --copy-to-image IMAGE] [--ls-image] [--make-release]\n' "$0" >&2
    printf 'Both targets are built by default.\n' >&2
    printf 'Variants: userport-01, userport-23, cassette-scl-write, cassette-sda-write (default)\n' >&2
    exit 1
}

variant_selected=0
target_selected=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --target)
            [ "$target_selected" -eq 0 ] && [ "$#" -ge 2 ] || usage
            TARGET="$2"
            target_selected=1
            shift 2
            ;;
        --all-targets)
            [ "$target_selected" -eq 0 ] || usage
            TARGET=all
            target_selected=1
            shift
            ;;
        --all-variants)
            [ "$variant_selected" -eq 0 ] || usage
            ALL_VARIANTS=1
            variant_selected=1
            shift
            ;;
        --variant)
            [ "$variant_selected" -eq 0 ] && [ "$#" -ge 2 ] || usage
            VARIANT="$2"
            variant_selected=1
            shift 2
            ;;
        --to-blank-d64|--to-blank-d81)
            [ -z "$BLANK_FORMAT" ] || usage
            BLANK_FORMAT="${1#--to-blank-}"
            shift
            ;;
        --copy-to-image|--copy-to_image)
            [ -z "$COPY_IMAGE" ] && [ "$#" -ge 2 ] && [ -n "$2" ] || usage
            COPY_IMAGE="$2"
            shift 2
            ;;
        --ls-image)
            [ "$LS_IMAGE" -eq 0 ] || usage
            LS_IMAGE=1
            shift
            ;;
        --make-release)
            [ "$MAKE_RELEASE" -eq 0 ] || usage
            MAKE_RELEASE=1
            shift
            ;;
        *) usage ;;
    esac
done
case "$VARIANT" in
    userport-01|userport-23|cassette-scl-write|cassette-sda-write) ;;
    *) usage ;;
esac
case "$TARGET" in
    c64|c128-80|all) ;;
    *) usage ;;
esac

if [ "$MAKE_RELEASE" -eq 1 ]; then
    if [ -n "$COPY_IMAGE" ]; then
        printf '%s\n' '--make-release cannot be combined with --copy-to-image.' >&2
        exit 1
    fi
    TARGET=all
    ALL_VARIANTS=1
    BLANK_FORMAT=both
fi

if [ -n "$COPY_IMAGE" ]; then
    if [ "$TARGET" = all ] || [ "$ALL_VARIANTS" -eq 1 ]; then
        printf '%s\n' '--copy-to-image requires one app: select --target c64 or c128-80 and do not use --all-variants.' >&2
        exit 1
    fi
    if [ -n "$BLANK_FORMAT" ]; then
        printf '%s\n' '--copy-to-image cannot be combined with a blank-disk option.' >&2
        exit 1
    fi
    case "$COPY_IMAGE" in
        *.[dD]64|*.[dD]81) ;;
        *)
            printf 'Copy destination must be a D64 or D81 image: %s\n' "$COPY_IMAGE" >&2
            exit 1
            ;;
    esac
    if [ ! -f "$COPY_IMAGE" ] || [ ! -r "$COPY_IMAGE" ] || [ ! -w "$COPY_IMAGE" ]; then
        printf 'Copy destination must be an existing readable/writable GEOS disk image: %s\n' "$COPY_IMAGE" >&2
        exit 1
    fi
fi

for tool in "$OSCAR64" "$PYTHON" date; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required tool not found: %s\n' "$tool" >&2
        exit 1
    fi
done
if [ "$LS_IMAGE" -eq 1 ] && ! command -v c1541 >/dev/null 2>&1; then
    printf '%s\n' 'Required tool not found: c1541 (needed for --ls-image).' >&2
    exit 1
fi
if [ ! -f "$GEOS_LIB_DIR/geos.c" ] || [ ! -f "$GEOS_LIB_DIR/cvt.py" ] ||
    [ ! -f "$GEOS_LIB_DIR/disk.py" ]; then
    printf 'GEOS library not found at %s; set GEOS_LIB_DIR.\n' "$GEOS_LIB_DIR" >&2
    exit 1
fi
if [ ! -f "$COMMON_LIB_DIR/i2c/i2c.h" ] || [ ! -f "$COMMON_LIB_DIR/i2c/i2c.c" ]; then
    printf 'Shared I2C library not found at %s/i2c; set COMMON_LIB_DIR.\n' "$COMMON_LIB_DIR" >&2
    exit 1
fi
if [ "$TARGET" = all ]; then
    targets='c64 c128-80'
else
    targets="$TARGET"
fi
# Previous base-image checks:
# for target in $targets; do
#     case "$target" in
#         c64) base="$C64_BASE_IMAGE" ;;
#         c128-80) base="$C128_BASE_IMAGE" ;;
#     esac
#     if [ ! -f "$base" ]; then
#         printf 'Required GEOS base image not found: %s\n' "$base" >&2
#         exit 1
#     fi
# done

BUILD_DATE=$(date '+%Y-%m-%dT%H:%M')
mkdir -p build
build_variant() {
    case "$1" in
        userport-01)
            wiring='Userport: SDA=PB0 SCL=PB1.'
            wiring_label=UP01
            cvt_variant=UP01
            set -- -DUSE_HARDCODED_I2C_PINS -DSDA_PIN=0 -DSCL_PIN=1 ;;
        userport-23)
            wiring='Userport: SDA=PB2 SCL=PB3.'
            wiring_label=UP23
            cvt_variant=UP23
            set -- -DUSE_HARDCODED_I2C_PINS -DSDA_PIN=2 -DSCL_PIN=3 ;;
        cassette-scl-write)
            wiring='Cassette: SDA=SENSE(4) SCL=WRITE(3).'
            wiring_label=CAS-SCL
            cvt_variant=CSCL
            set -- -DI2C_IN_CASSETTE_PORT -DI2C_CAS_SCL_WRITE ;;
        cassette-sda-write)
            wiring='Cassette: SDA=WRITE(3) SCL=SENSE(4).'
            wiring_label=CAS-SDA
            cvt_variant=CSDA
            set -- -DI2C_IN_CASSETTE_PORT -DI2C_CAS_SDA_WRITE ;;
    esac
    case "$target" in
        c64) machine=c64; platform=''; target_label=C64; cvt_target=FM64 ;;
        c128-80)
            machine=c128
            platform=' GEOS 128 80-column.'
            target_label=C128-80
            cvt_target=FM12880
            set -- "$@" -DGEOS_C128_80
            ;;
    esac
    cvt_file="build/$cvt_target-$cvt_variant.cvt"
    "$OSCAR64" -g -n -O1 "-tm=$machine" -tf=bin \
        -DI2C_SHARED_PORT -DI2C_NO_STDIO \
        "-DAPP_VERSION=\"$VERSION\"" \
        "-rt=$GEOS_LIB_DIR/geos.c" "-i=$GEOS_LIB_DIR" \
        "-i=$COMMON_LIB_DIR/i2c" "-i=$COMMON_LIB_DIR" "$@" \
        "-o=$output.bin" main.c
    "$PYTHON" "$GEOS_LIB_DIR/cvt.py" "$output.bin" "$cvt_file" FM-Radio radio-icon.raw \
        --version "$VERSION" --description "FM-Radio for RDA5807 chip. $wiring$platform" \
        --modified "$BUILD_DATE" --target "$target" --author "$AUTHOR"
    if [ "$BLANK_FORMAT" = both ]; then
        formats='d64 d81'
    else
        formats="${BLANK_FORMAT:-d64}"
    fi
    # Previous system-disk duplication:
    # case "$target" in
    #     c64) base="$C64_BASE_IMAGE"; new_disk_name="$NEW_C64_DISK_LABEL" ;;
    #     c128-80) base="$C128_BASE_IMAGE"; new_disk_name="$NEW_C128_DISK_LABEL" ;;
    # esac
    # mkdir -p "$(dirname "$image")"
    # set -- "$cvt_file" "$image" "$base"
    # if [ -n "$new_disk_name" ]; then
    #     set -- "$@" --disk-name "$new_disk_name"
    # fi
    # "$PYTHON" "$GEOS_LIB_DIR/disk.py" "$@"
    for format in $formats; do
        if [ -n "$BLANK_FORMAT" ]; then
            disk_image="${image%.*}-$target-$variant.$format"
            mkdir -p "$(dirname "$disk_image")"
            set -- "$cvt_file" "$disk_image" --blank-format "$format" \
                --disk-name "$target_label-$wiring_label"
        elif [ -n "$COPY_IMAGE" ]; then
            disk_image="$COPY_IMAGE"
            set -- "$cvt_file" "$disk_image" --in-place
        else
            disk_image="$output.d64"
            set -- "$cvt_file" "$disk_image"
        fi
        "$PYTHON" "$GEOS_LIB_DIR/disk.py" "$@"
        if [ "$LS_IMAGE" -eq 1 ]; then
            printf '\nContents of %s:\n' "$disk_image"
            if ! c1541 -attach "$disk_image" -list; then
                printf 'Failed to list disk image: %s\n' "$disk_image" >&2
                exit 1
            fi
        fi
        if [ -n "$BLANK_FORMAT" ]; then
            printf '\nBuilt %s / %s: %s and %s (label: %s-%s).\n' \
                "$target" "$variant" "$cvt_file" "$disk_image" "$target_label" "$wiring_label"
        elif [ -n "$COPY_IMAGE" ]; then
            printf '\nBuilt %s / %s: %s; installed in %s.\n' "$target" "$variant" "$cvt_file" "$disk_image"
        else
            printf '\nBuilt %s / %s: %s and %s.\n' "$target" "$variant" "$cvt_file" "$disk_image"
        fi
    done
}

if [ "$ALL_VARIANTS" -eq 1 ]; then
    variants='userport-01 userport-23 cassette-scl-write cassette-sda-write'
else
    variants="$VARIANT"
fi
set --
for target in $targets; do
    for variant in $variants; do
        case "$target" in
            c64) output=build/fm-radio; image="$C64_OUTPUT_IMAGE" ;;
            c128-80) output=build/fm-radio-c128-80; image="$C128_OUTPUT_IMAGE" ;;
        esac
        if [ "$ALL_VARIANTS" -eq 1 ]; then
            output="$output-$variant"
        fi
        # Previous system-disk output naming:
        # if [ -z "$BLANK_FORMAT" ] && [ "$ALL_VARIANTS" -eq 1 ]; then
        #     image="${image%.*}-$variant.${image##*.}"
        # fi
        build_variant "$variant"
        if [ "$MAKE_RELEASE" -eq 1 ]; then
            set -- "$@" "$cvt_file" "${image%.*}-$target-$variant.d64" \
                "${image%.*}-$target-$variant.d81"
        fi
    done
done

if [ "$MAKE_RELEASE" -eq 1 ]; then
    archive="build/FM-Radio-$VERSION.zip"
    "$PYTHON" - "$archive" "$@" <<'PY'
import os
from pathlib import Path
import sys
import tempfile
import zipfile

archive = Path(sys.argv[1])
files = [Path(name) for name in sys.argv[2:]]
if len(files) != 24 or len({path.name for path in files}) != 24:
    raise ValueError("Release must contain 24 uniquely named files.")
if any(path.suffix == ".cvt" and len(path.name) > 16 for path in files):
    raise ValueError("Release CVT filenames must fit the 16-character GEOS limit.")
metadata = [path.read_bytes()[254:508] for path in files[::3]]
version = metadata[0][87:94].split(b"\0", 1)[0].decode("ascii")
author = metadata[0][95:158].split(b"\0", 1)[0].decode("ascii")
readme = [
    f"FM-Radio {version} release",
    f"Author: {author}",
    "",
    "FM-Radio controls an RDA5807 FM tuner under Commodore GEOS.",
    "",
    "File formats",
    "------------",
    "CVT: A sequential GEOS Convert file containing one application, its icon",
    "and file information. Use GEOS Convert to install it on a GEOS disk.",
    "D64/D81: GEOS-formatted disks with that same application already installed",
    "as FM-Radio. Each listed pair contains the same target/wiring build.",
    "D64 is a 35-track 1541 image; D81 is an 80-track 1581 image.",
    "These are application disks, NOT GEOS boot disks. Boot the matching GEOS",
    "system first, open the application disk and launch FM-Radio.",
    "",
    "Filename tags: FM64 = C64; FM12880 = C128, 80 columns.",
    "UP01/UP23 = user-port PB0/PB1 or PB2/PB3.",
    "CSCL/CSDA = cassette SCL/SDA connected to WRITE, respectively.",
    "Cassette (3)/(4) below are CPU-port bit numbers, not connector pin numbers.",
    "",
    "Included builds",
    "---------------",
]
for index, info in enumerate(metadata):
    cvt, d64, d81 = files[index * 3:index * 3 + 3]
    target = {
        0x40: "Commodore 64 / GEOS 64 / 40 columns",
        0xc0: "Commodore 128 / GEOS 128 / 80 columns only",
    }[info[94]]
    description = info[158:254].split(b"\0", 1)[0].decode("ascii")
    readme.extend([
        f"CVT: {cvt.name}",
        f"Disk images: {d64.name} / {d81.name}",
        f"Target: {target}",
        f"Contents: {description}",
        "",
    ])
with tempfile.TemporaryDirectory(prefix=".release-", dir=archive.parent) as temporary:
    output = Path(temporary) / archive.name
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for path in files:
            package.write(path, arcname=path.name)
        package.writestr("readme.txt", "\n".join(readme).encode("ascii"))
    os.replace(output, archive)
PY
    printf '\nRelease created: %s (8 CVTs, 8 D64s, 8 D81s and readme.txt).\n' "$archive"
fi
