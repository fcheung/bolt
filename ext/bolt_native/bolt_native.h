#ifndef BOLT_NATIVE_H
#define BOLT_NATIVE_H 1

#include "ruby.h"
VALUE rb_bolt_encode_integer(VALUE self, VALUE integer, VALUE buffer);
VALUE rb_bolt_encode_array(VALUE self, VALUE integer, VALUE buffer);

VALUE rb_bolt_pack_internal(VALUE self, VALUE buffer, VALUE item);

#endif /* BOLT_NATIVE_H */
