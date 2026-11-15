#!/bin/sh
set -eu

cd "$(dirname "$0")"
GEOS_LIB_DIR="${GEOS_LIB_DIR:-../ccommon/geos}"
COMMON_LIB_DIR="${COMMON_LIB_DIR:-../ccommon}"
OSCAR64="${OSCAR64:-oscar64}"
PYTHON="${PYTHON:-python3}"
VERSION=1.0
VARIANT=cassette-sda-write
ALL_VARIANTS=0

usage() {
    printf 'Usage: %s [--variant NAME | --all-variants]\n' "$0" >&2
    printf 'Variants: userport-01, userport-23, cassette-scl-write, cassette-sda-write (default)\n' >&2
    exit 1
}

variant_selected=0
while [ "$#" -gt 0 ]; do
    case "$1" in
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
if [ ! -f geos64-uiec_orig.d81 ]; then
    printf 'Required GEOS base image not found: geos64-uiec_orig.d81\n' >&2
    exit 1
fi

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
    "$OSCAR64" -g -n -O1 -tm=c64 -tf=bin \
        -DI2C_SHARED_PORT -DI2C_NO_STDIO \
        "-DAPP_VERSION=\"$VERSION\"" \
        "-rt=$GEOS_LIB_DIR/geos.c" "-i=$GEOS_LIB_DIR" \
        "-i=$COMMON_LIB_DIR/i2c" "-i=$COMMON_LIB_DIR" "$@" \
        "-o=$output.bin" main.c
    "$PYTHON" "$GEOS_LIB_DIR/cvt.py" "$output.bin" "$output.cvt" FM-Radio radio-icon.raw \
        --version "$VERSION" --description "FM-Radio for RDA5807 chip. $wiring" \
        --modified "$BUILD_DATE"
    "$PYTHON" "$GEOS_LIB_DIR/disk.py" "$output.cvt" "$output.d64"
    "$PYTHON" "$GEOS_LIB_DIR/disk.py" "$output.cvt" "$image" geos64-uiec_orig.d81
    printf '\nBuilt %s: %s.cvt, %s.d64 and %s.\n' "$variant" "$output" "$output" "$image"
}

if [ "$ALL_VARIANTS" -eq 1 ]; then
    variants='userport-01 userport-23 cassette-scl-write cassette-sda-write'
else
    variants="$VARIANT"
fi
for variant in $variants; do
    if [ "$ALL_VARIANTS" -eq 1 ]; then
        output="build/fm-radio-$variant"
        image="build/geos64-uiec-$variant.d81"
    else
        output=build/fm-radio
        image=build/geos64-uiec.d81
    fi
    build_variant "$variant"
done
