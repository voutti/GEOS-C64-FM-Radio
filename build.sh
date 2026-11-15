#!/bin/sh
set -eu

cd "$(dirname "$0")"
GEOS_LIB_DIR="${GEOS_LIB_DIR:-../ccommon/geos}"
COMMON_LIB_DIR="${COMMON_LIB_DIR:-../ccommon}"
OSCAR64="${OSCAR64:-oscar64}"
PYTHON="${PYTHON:-python3}"
VERSION=1.0
C64_BASE_IMAGE="${C64_BASE_IMAGE:-GEOS64.D81}"
C128_BASE_IMAGE="${C128_BASE_IMAGE:-GEOS128.D81}"
C64_OUTPUT_IMAGE="${C64_OUTPUT_IMAGE:-build/geos64.d81}"
C128_OUTPUT_IMAGE="${C128_OUTPUT_IMAGE:-build/geos128.d81}"
# Empty new names preserve the source disk labels.
NEW_C64_DISK_LABEL="${NEW_C64_DISK_LABEL-}"
NEW_C128_DISK_LABEL="${NEW_C128_DISK_LABEL-WORK}"
VARIANT=cassette-sda-write
ALL_VARIANTS=0
TARGET=all

usage() {
    printf 'Usage: %s [--target c64|c128-80 | --all-targets] [--variant NAME | --all-variants]\n' "$0" >&2
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

for tool in "$OSCAR64" "$PYTHON" date; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required tool not found: %s\n' "$tool" >&2
        exit 1
    fi
done
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
for target in $targets; do
    case "$target" in
        c64) base="$C64_BASE_IMAGE" ;;
        c128-80) base="$C128_BASE_IMAGE" ;;
    esac
    if [ ! -f "$base" ]; then
        printf 'Required GEOS base image not found: %s\n' "$base" >&2
        exit 1
    fi
done

BUILD_DATE=$(date '+%Y-%m-%dT%H:%M')
mkdir -p build
build_variant() {
    case "$1" in
        userport-01)
            wiring='Userport: SDA=PB0 SCL=PB1.'
            set -- -DUSE_HARDCODED_I2C_PINS -DSDA_PIN=0 -DSCL_PIN=1 ;;
        userport-23)
            wiring='Userport: SDA=PB2 SCL=PB3.'
            set -- -DUSE_HARDCODED_I2C_PINS -DSDA_PIN=2 -DSCL_PIN=3 ;;
        cassette-scl-write)
            wiring='Cassette: SDA=SENSE(4) SCL=WRITE(3).'
            set -- -DI2C_IN_CASSETTE_PORT -DI2C_CAS_SCL_WRITE ;;
        cassette-sda-write)
            wiring='Cassette: SDA=WRITE(3) SCL=SENSE(4).'
            set -- -DI2C_IN_CASSETTE_PORT -DI2C_CAS_SDA_WRITE ;;
    esac
    case "$target" in
        c64) machine=c64; platform=''; base="$C64_BASE_IMAGE" ;;
        c128-80)
            machine=c128
            platform=' GEOS 128 80-column.'
            base="$C128_BASE_IMAGE"
            set -- "$@" -DGEOS_C128_80
            ;;
    esac
    "$OSCAR64" -g -n -O1 "-tm=$machine" -tf=bin \
        -DI2C_SHARED_PORT -DI2C_NO_STDIO \
        "-DAPP_VERSION=\"$VERSION\"" \
        "-rt=$GEOS_LIB_DIR/geos.c" "-i=$GEOS_LIB_DIR" \
        "-i=$COMMON_LIB_DIR/i2c" "-i=$COMMON_LIB_DIR" "$@" \
        "-o=$output.bin" main.c
    "$PYTHON" "$GEOS_LIB_DIR/cvt.py" "$output.bin" "$output.cvt" FM-Radio radio-icon.raw \
        --version "$VERSION" --description "FM-Radio for RDA5807 chip. $wiring$platform" \
        --modified "$BUILD_DATE" --target "$target"
    "$PYTHON" "$GEOS_LIB_DIR/disk.py" "$output.cvt" "$output.d64"
    mkdir -p "$(dirname "$image")"
    case "$target" in
        c64) new_disk_name="$NEW_C64_DISK_LABEL" ;;
        c128-80) new_disk_name="$NEW_C128_DISK_LABEL" ;;
    esac
    set -- "$output.cvt" "$image" "$base"
    if [ -n "$new_disk_name" ]; then
        set -- "$@" --disk-name "$new_disk_name"
    fi
    "$PYTHON" "$GEOS_LIB_DIR/disk.py" "$@"
    printf '\nBuilt %s / %s: %s.cvt, %s.d64 and %s.\n' "$target" "$variant" "$output" "$output" "$image"
}

if [ "$ALL_VARIANTS" -eq 1 ]; then
    variants='userport-01 userport-23 cassette-scl-write cassette-sda-write'
else
    variants="$VARIANT"
fi
for target in $targets; do
    for variant in $variants; do
        case "$target" in
            c64) output=build/fm-radio; image="$C64_OUTPUT_IMAGE" ;;
            c128-80) output=build/fm-radio-c128-80; image="$C128_OUTPUT_IMAGE" ;;
        esac
        if [ "$ALL_VARIANTS" -eq 1 ]; then
            output="$output-$variant"
            image="${image%.*}-$variant.${image##*.}"
        fi
        build_variant "$variant"
    done
done
