RSpec::Matchers.define :match_hex do |expected|
  match do |actual|
    actual == [expected.gsub(':', '')].pack('H*')
  end

  failure_message do |actual|
    "expected that #{format_bytes(actual)} would be equal to #{expected}"
  end

  def format_bytes bytes
    bytes.unpack('H*')[0].chars.each_slice(2).flat_map {|pair| pair.join.upcase}.join(':')
  end
end
