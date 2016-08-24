RSpec::Matchers.define :match_hex do |y|
  match do |x|
    formatted = x.unpack('H*')[0].chars.each_slice(2).flat_map {|pair| pair.join.upcase}.join(':')
    expect(formatted).to eq(y)
  end
end
