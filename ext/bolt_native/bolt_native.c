#include "bolt_native.h"
VALUE rb_mBolt;
VALUE rb_mBolt_packStream;

void
Init_bolt_native(void)
{
  rb_mBolt = rb_const_get(rb_cObject, rb_intern("Bolt"));
  rb_mBolt_packStream = rb_const_get(rb_mBolt, rb_intern("PackStream"));

  rb_define_singleton_method(rb_mBolt_packStream, "encode_integer", RUBY_METHOD_FUNC(rb_bolt_encode_integer),2);
}

VALUE rb_bolt_encode_integer(VALUE self, VALUE integer, VALUE buffer){
  size_t length=0;
  uint8_t cbuffer[9]; /*an int is at most 1 byte length, 8 bytes data*/
  long long value = NUM2LL(integer);
  unsigned long long unsigned_value = (unsigned long long) value;
  if(value >= -0x10 && value < 0x80){
    cbuffer[0] = (uint8_t)(unsigned_value & 0xFF);
    length = 1;
  }else if( -0x80 <= value && value < 0x80){
    cbuffer[0] = '\xC8';
    cbuffer[1] = (uint8_t)(unsigned_value & 0xFF);
    length = 2;
  }
  else if (-0x8000 <= value && value < 0x8000){
    cbuffer[0] = '\xC9';
    cbuffer[1] = (uint8_t)((unsigned_value & 0xFF00) >> 8);
    cbuffer[2] = (uint8_t)((unsigned_value & 0xFF));
    length = 3;
  }
  else if (-0x80000000L <= value && value < 0x80000000L){
    cbuffer[0] = '\xCA';
    cbuffer[1] = (uint8_t)((unsigned_value >> 24)  & 0xFF);
    cbuffer[2] = (uint8_t)((unsigned_value >> 16)  & 0xFF);
    cbuffer[3] = (uint8_t)((unsigned_value >> 8)  & 0xFF);
    cbuffer[4] = (uint8_t)((unsigned_value)  & 0xFF);
    length = 5;
  }else {
    cbuffer[0] = '\xCB';
    cbuffer[1] = (uint8_t)((unsigned_value >> 56)  & 0xFF);
    cbuffer[2] = (uint8_t)((unsigned_value >> 48)  & 0xFF);
    cbuffer[3] = (uint8_t)((unsigned_value >> 40)  & 0xFF);
    cbuffer[4] = (uint8_t)((unsigned_value >> 32)  & 0xFF);
    cbuffer[5] = (uint8_t)((unsigned_value >> 24)  & 0xFF);
    cbuffer[6] = (uint8_t)((unsigned_value >> 16)  & 0xFF);
    cbuffer[7] = (uint8_t)((unsigned_value >> 8)  & 0xFF);
    cbuffer[8] = (uint8_t)((unsigned_value)  & 0xFF);
    length = 9;
  }
  return rb_str_buf_cat(buffer, (const char*)cbuffer, length);

}