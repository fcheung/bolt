module Bolt
  module PackStream
    def self.pack(*values)
      values.map do |value|
        case value
        when Integer then encode_integer(value)
        when Float then ["\xC1", value].pack('AG')
        when String then encode_string(value)
        when Symbol then encode_string(value.to_s)
        when Array then encode_array(value)
        when Hash then encode_hash(value)
        when nil then "\xC0"
        when true then "\xC3"
        when false then "\xC2"
        else
          raise ArgumentError, "value #{value} cannot be packstreamed"
        end
      end.join.force_encoding('BINARY')
    end

    def self.encode_integer(value)
      if -0x10 <= value && value < 0x80
        [value].pack('c')
      elsif -0x80 <= value  && value < 0x80
        ["\xC8", value].pack('Ac')
      elsif  -0x8000 <= value && value < 0x8000
        ["\xC9", value].pack('As>')
      elsif  -0x8000_0000 <= value && value < 0x8000_0000
        ["\xCA", value].pack('Al>')
      elsif  -0x8000_0000_0000_0000 <= value && value < 0x8000_0000_0000_0000
        ["\xCB", value].pack('Aq>')
      else
        raise ArgumentError, "integer #{value} is out of range"
      end
    end

    def self.encode_array(array)
      length = array.length
      leader = case length
      when 0..15 then [0x90 + length].pack('C')
      when 16..255 then [0xD4, length].pack('CC')
      when 256..65535 then [0xD5, length].pack('CS>')
      when 65536...0x100000000 then [0xD6, length].pack('CL>')
      else 
        raise ArgumentError, "Array is too long #{length}"
      end
      array.inject(leader) {|buffer, item| buffer << pack(item)}
    end

    def self.encode_hash(hash)
      size = hash.size
      leader = case size
      when 0..15 then [0xA0 + size].pack('C')
      when 16..255 then [0xD8, size].pack('CC')
      when 256..65535 then [0xD9, size].pack('CS>')
      when 65536...0x100000000 then [0xDA, size].pack('CL>')
      else 
        raise ArgumentError, "Hash is too big #{size}"
      end
      hash.inject(leader) do |buffer, (key, value)| 
        buffer << pack(key)
        buffer << pack(value)
      end
    end

    def self.encode_string(string)
      encoded = string.encode('utf-8')
      bytesize = encoded.bytesize
      leader = case bytesize
      when 0..15 then [0x80 + bytesize].pack('C')
      when 16..255 then [0xD0, bytesize].pack('CC')
      when 256..65535 then [0xD1, bytesize].pack('CS>')
      when 65536...0x100000000 then [0xD2, bytesize].pack('CL>')
      else 
        raise ArgumentError, "String is too long (#{bytesize})"
      end
      leader + encoded.force_encoding('BINARY')
    end
  end
end