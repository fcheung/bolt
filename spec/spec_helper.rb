$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)
require 'bolt'
Dir[File.dirname(__FILE__)+'/support/*.rb'].each {|f| require f }

RSpec.configure do |config|
  config.example_status_persistence_file_path = "./spec/examples.txt"
  config.mock_with :rspec do |c|
    c.syntax = [:expect]
    c.verify_partial_doubles = true
  end
  config.expect_with :rspec do |c|
    c.syntax = [:expect]
  end
end