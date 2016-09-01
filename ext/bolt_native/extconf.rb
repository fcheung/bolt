require "mkmf"

$CFLAGS << ' -Werror -O2'
create_makefile("bolt_native/bolt_native")
