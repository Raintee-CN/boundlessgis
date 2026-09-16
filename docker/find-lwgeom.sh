#!/bin/sh
set -eu
find /usr -name 'liblwgeom.h' -print 2>/dev/null || true
find /usr -name 'liblwgeom*' -print 2>/dev/null | head
dpkg -L postgresql-18-postgis-3-scripts 2>/dev/null | head
dpkg -L postgresql-18-postgis-3 2>/dev/null | head
