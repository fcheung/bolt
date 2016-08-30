#ifndef BOLT_NATIVE_H
#define BOLT_NATIVE_H 1

#include "ruby.h"
VALUE rb_bolt_encode_integer(VALUE self, VALUE integer, VALUE buffer);
VALUE rb_bolt_encode_array(VALUE self, VALUE array, VALUE buffer);

VALUE rb_bolt_pack_internal(VALUE self, VALUE buffer, VALUE item);
VALUE rb_bolt_encode_hash(VALUE self, VALUE hash, VALUE buffer);
VALUE rb_bolt_encode_string(VALUE self, VALUE string, VALUE buffer);

void bolt_encode_array(VALUE array, VALUE buffer);
void bolt_encode_hash(VALUE array, VALUE buffer);
void bolt_encode_string(VALUE array, VALUE buffer);

#endif /* BOLT_NATIVE_H */
