#!/bin/sh
# Launcher for the packaged app.
#
# The executable already carries a RUNPATH pointing at ../lib and Qt finds its
# plugins through qt.conf, so this normally changes nothing. It exists because
# an explicit LD_LIBRARY_PATH is the difference between working and a confusing
# "cannot open shared object file" on setups where RUNPATH is ignored, such as
# some security-hardened loaders and anything that runs the binary through a
# wrapper.

set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

exec "$here/bin/mainteckj-app" "$@"
