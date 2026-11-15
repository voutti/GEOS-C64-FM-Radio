#!/bin/sh
set -eu

cd "$(dirname "$0")"
GEOS_LIB_DIR="${GEOS_LIB_DIR:-../ccommon/geos}"
COMMON_LIB_DIR="${COMMON_LIB_DIR:-../ccommon}"
OSCAR64="${OSCAR64:-oscar64}"
PYTHON="${PYTHON:-python3}"
RASP_ZERO_W_IP="${RASP_ZERO_W_IP:-192.168.1.226}"
DEPLOY=1
VARIANT=cassette-sda-write
ALL_VARIANTS=0

usage() {
    printf 'Usage: %s [--no-deploy] [--variant NAME | --all-variants]\n' "$0" >&2
    printf 'Variants: userport-01, userport-23, cassette-scl-write, cassette-sda-write (default)\n' >&2
    exit 1
}

variant_selected=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --no-deploy) DEPLOY=0; shift ;;
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

for tool in "$OSCAR64" "$PYTHON"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required tool not found: %s\n' "$tool" >&2
        exit 1
    fi
done
if [ "$DEPLOY" -eq 1 ]; then
    for tool in ssh scp; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            printf 'Required deployment tool not found: %s\n' "$tool" >&2
            exit 1
        fi
    done
    if [ -n "${SSHPASS:-}" ] && ! command -v sshpass >/dev/null 2>&1; then
        printf 'SSHPASS is set but sshpass is not installed.\n' >&2
        exit 1
    fi
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
if [ ! -f geos64-uiec_orig.d81 ]; then
    printf 'Required GEOS base image not found: geos64-uiec_orig.d81\n' >&2
    exit 1
fi

mkdir -p build
build_variant() {
    case "$1" in
        userport-01) set -- -DUSE_HARDCODED_I2C_PINS -DSDA_PIN=0 -DSCL_PIN=1 ;;
        userport-23) set -- -DUSE_HARDCODED_I2C_PINS -DSDA_PIN=2 -DSCL_PIN=3 ;;
        cassette-scl-write) set -- -DI2C_IN_CASSETTE_PORT -DI2C_CAS_SCL_WRITE ;;
        cassette-sda-write) set -- -DI2C_IN_CASSETTE_PORT -DI2C_CAS_SDA_WRITE ;;
    esac
    "$OSCAR64" -g -n -O1 -tm=c64 -tf=bin \
        -DI2C_SHARED_PORT -DI2C_NO_STDIO \
        "-rt=$GEOS_LIB_DIR/geos.c" "-i=$GEOS_LIB_DIR" \
        "-i=$COMMON_LIB_DIR/i2c" "-i=$COMMON_LIB_DIR" "$@" \
        "-o=$output.bin" main.c
    "$PYTHON" "$GEOS_LIB_DIR/cvt.py" "$output.bin" "$output.cvt" FM-Radio radio-icon.raw
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

if [ "$DEPLOY" -eq 1 ]; then
    remote="timo@$RASP_ZERO_W_IP"
    usb_unmounted=0

    authenticated() {
        if [ -n "${SSHPASS:-}" ]; then
            sshpass -e "$@"
        else
            "$@"
        fi
    }

    remount_usb() {
        status=$?
        trap - 0
        if [ "$usb_unmounted" -eq 1 ]; then
            if ! authenticated ssh "$remote" /home/timo/mountusb.sh; then
                printf 'Failed to remount Raspberry Pi USB storage; run /home/timo/mountusb.sh on the Pi.\n' >&2
                status=1
            fi
        fi
        exit "$status"
    }

    trap remount_usb 0
    trap 'exit 130' INT
    trap 'exit 143' TERM

    if ! authenticated ssh "$remote" /home/timo/umountusb.sh; then
        printf 'Failed to unmount Raspberry Pi USB storage; transfer cancelled.\n' >&2
        exit 1
    fi
    usb_unmounted=1
    for variant in $variants; do
        if [ "$ALL_VARIANTS" -eq 1 ]; then
            image="build/geos64-uiec-$variant.d81"
        else
            image=build/geos64-uiec.d81
        fi
        if ! authenticated scp "$image" "$remote:/mnt/usb_share/"; then
            printf 'Failed to copy %s to Raspberry Pi USB storage.\n' "$image" >&2
            exit 1
        fi
    done
    if ! authenticated ssh "$remote" sync; then
        printf 'Failed to sync Raspberry Pi USB storage.\n' >&2
        exit 1
    fi
    if ! authenticated ssh "$remote" /home/timo/mountusb.sh; then
        printf 'Failed to remount Raspberry Pi USB storage; cleanup will retry.\n' >&2
        exit 1
    fi
    usb_unmounted=0
    printf 'Copied D81 image(s) to %s:/mnt/usb_share/ and remounted USB storage.\n' "$remote"
fi
