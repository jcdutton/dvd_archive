aclocal -I m4
libtoolize --force --copy
autoheader
automake --add-missing --copy
autoconf

autoreconf
