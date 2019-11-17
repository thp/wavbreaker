To regenerate the pot file from the sources:

    (cd po && ./update-potfiles.sh)
    meson build
    ninja -C build wavbreaker-pot
    ninja -C build wavbreaker-update-po
