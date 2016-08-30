require "bundler/gem_tasks"
require "rspec/core/rake_task"

RSpec::Core::RakeTask.new(:spec)

require "rake/extensiontask"

task :build => :compile

Rake::ExtensionTask.new("bolt_native") do |ext|
  ext.lib_dir = "lib/bolt"
end

task :default => [:clobber, :compile, :spec]
