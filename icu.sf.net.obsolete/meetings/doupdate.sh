#!/bin/sh
set -x
rsync $* -avc . shell.sourceforge.net:/home/groups/i/ic/icu/htdocs/meetings/ --exclude='*CVS*' --exclude='*.sh*' 
