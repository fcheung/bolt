module Bolt
  module PackStream
    def self.pack(*values)
      values.map do |value|
        case value
        when Integer then encode_integer(value)
        when Float then ["\xC1", value].pack('AG')
        when String then encode_string(value)
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
        ["\xCA", value].pack('Aq>')
      else
        raise ArgumentError, "integer #{value} is out of range"
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
        raise ArgumentError, "String is too long"
      end
      leader + encoded.force_encoding('BINARY')
    end
  end
end