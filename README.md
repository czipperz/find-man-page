# find-man-page

The purpose of this package is to make `man` fast!  Using `man` is super slow on my computer, both
for using auto-complete in the shell and actually viewing man pages.  So I made replacements.

There are three separate programs provided: `find-man-page`, `autocomplete-man-page`, and
`load-man-page`.

`find-man-page` finds the paths of the given man page.  All results will be shown, so you can
choose between which section to open.  `find-man-page` will dereference shallow man pages
so the file names may not match the given man page name.

`autocomplete-man-page` looks for all man pages with the name as a prefix.  This is intended to be
used for your shell's auto-complete functionality.

`load-man-page` finds the first instance of the given man page and prints the decompressed
contents.  This is an optimized version of running `find-man-page`, taking the first line and
decompressing it.

The script `man` is provided which emulates `man`'s functionality by using `load-man-page`.

## Compiling

Run `./build-release.sh`.
