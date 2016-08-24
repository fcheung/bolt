module Bolt
  module PackStream
    def self.pack(*values)
      values.map do |value|
        case value
        when Integer then encode_integer(value)
        else
          raise ArgumentError, "value #{value} cannot be packstreamed"
        end
      end.join
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

  end
end