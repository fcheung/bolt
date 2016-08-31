require 'bundler/setup'
$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
require 'bolt'
require 'benchmark/ips'



LIST_OF_INTEGERS = "\x98\x00\x7F\xF0\xC8\x80\xC9\x80\x00\xC9\x7F\xFF\xCA\x80\x00\x00\x00\xCB\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
LONG_LIST_OF_INTEGERS_DATA = "\xD6\x00\x01\x00\x00" + "\x01" * 65536
LONG_LIST_OF_INTEGERS = (-65536..65536).to_a
MAP_OF_STRINGS = "\xD8\x1A\x81\x41\x01\x81\x42\x02\x81\x43\x03\x81\x44\x04\x81\x45\x05\x81\x46\x06\x81\x47\x07\x81\x48\x08\x81\x49\x09\x81\x4A\x0A\x81\x4B\x0B\x81\x4C\x0C\x81\x4D\x0D\x81\x4E\x0E\x81\x4F\x0F\x81\x50\x10\x81\x51\x11\x81\x52\x12\x81\x53\x13\x81\x54\x14\x81\x55\x15\x81\x56\x16\x81\x57\x17\x81\x58\x18\x81\x59\x19\x81\x5A\x1A"

BIG_MAP_OF_INTS_TO_STRINGS = (1..65536).zip(['1']*65536).to_h
BIG_MAP_OF_INTS_TO_STRINGS_DATA = Bolt::PackStream.pack(BIG_MAP_OF_INTS_TO_STRINGS)
SMALL_LIST = [1, true, 3.14, "fünf"]
FLOATS = [1.0]*65536
Benchmark.ips do |x|

  x.report("unpack list of integers") do
    Bolt::PackStream.unpack(LIST_OF_INTEGERS).next
  end

  x.report("unpack long list of integers") do
    Bolt::PackStream.unpack(LONG_LIST_OF_INTEGERS_DATA).next
  end

  x.report("unpack map of strings") do
    Bolt::PackStream.unpack(MAP_OF_STRINGS).next
  end

  x.report("unpack big map of strings") do
    Bolt::PackStream.unpack(BIG_MAP_OF_INTS_TO_STRINGS_DATA).next
  end

  x.report("pack big map of ints to strings") do
    Bolt::PackStream.pack(BIG_MAP_OF_INTS_TO_STRINGS)
  end

  x.report("pack small list") do
    Bolt::PackStream.pack(SMALL_LIST)
  end

  x.report("pack big list") do
    Bolt::PackStream.pack(LONG_LIST_OF_INTEGERS)
  end

  x.report("pack big list of floats") do
    Bolt::PackStream.pack(FLOATS)
  end

end