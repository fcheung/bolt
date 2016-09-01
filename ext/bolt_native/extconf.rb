require "mkmf"

$CFLAGS << ' -Werror -O2 -std=c99'
create_makefile("bolt_native/bolt_native")
