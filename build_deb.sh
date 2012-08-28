#!/bin/sh

# Duplicate 3 to 1 so we can use 3 for user messages.
exec 3>&1

# Silence all commands be default (this way we can do tests without spam).
#exec 1>&-
#exec 2>&-

# Helper function to display messages to the user.
message () {
	echo $* >&3
}

HEAD=$(head -n 1 debian/changelog)
VERSION=${HEAD##*\(}
VERSION=${VERSION%%-*}
PACKAGE=${HEAD%% *}

message "PACKAGE - '$PACKAGE'"
message "  VERSION - '$VERSION'"

# Clean up any junk left behind.
#for dir in ../${PACKAGE}[_-]${VERSION}* ../lib*.deb ; do
#  message "Cleaning up '$dir'."
#  rm -rf "$dir"
#done

# Create a tarball used in building a source-package.
#message "Exporting as '../${PACKAGE}_${VERSION}.orig.tar.gz'."
#svn export . ../${PACKAGE}-${VERSION} && \
#cd .. && \
#tar -czvf ${PACKAGE}_${VERSION}.orig.tar.gz ${PACKAGE}-${VERSION}

# Now we need to start building in a new directory.
#cd ${PACKAGE}-${VERSION}
message "Initializing with bootstrap." ; ./bootstrap

message "Building debian package."
#DH_VERBOSE=1 debuild -rfakeroot
debuild -I.svn

message "Build complete."

