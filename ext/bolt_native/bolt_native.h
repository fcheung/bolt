#ifndef BOLT_NATIVE_H
#define BOLT_NATIVE_H 1

#include "ruby.h"
VALUE rb_bolt_encode_integer(VALUE self, VALUE integer, VALUE buffer);

VALUE rb_bolt_pack_internal(VALUE self, VALUE buffer, VALUE item);

VALUE pack_internal(VALUE buffer, VALUE item);
void bolt_encode_array(VALUE array, VALUE buffer);
void bolt_encode_hash(VALUE array, VALUE buffer);
void bolt_encode_string(VALUE array, VALUE buffer);
void bolt_encode_float(VALUE rbfloat, VALUE buffer);
void bolt_encode_structure(VALUE structure, VALUE buffer);

#endif /* BOLT_NATIVE_H */
