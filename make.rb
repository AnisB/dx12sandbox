#!/usr/bin/env ruby

# External requirements
require 'fileutils'
require 'optparse'

# option variables
$options = {}
$compiler = ["vc14", "vc15", "makefile", "xcode"]
$platform = ["win64", "osx", "linux"]
$build = ["debug", "release"]

# Build variables
$script_dir = File.expand_path(File.dirname(__FILE__))
$platform_name =  nil
$build_directory = nil
$output_directory = $script_dir + "/output/"

# Cmake exe to use
CMAKE_EXE = "cmake"

def parse_options(argv, options)
	opt_parser = OptionParser.new do|opts|
		opts.banner = "Usage: make.rb [options]"
		opts.on('-c',  '--compiler <compiler>', "Target compiler [#{$compiler.join(", ")}]") { |v| options[:compiler] = v }
		opts.on('-p',  '--platform <platform>', "The target platform [#{$platform.join(", ")}]" ) { |v| options[:platform] = v }
		opts.on('-a', '--applications', "Generate applications for the sdk" ) { |v| options[:applications] = v }
		opts.on('-b', '--build <build>', "Compiles the project in at given [#{$compiler.join(", ")}] ") { |v| options[:build] = v }
		opts.on('-h',  '--help', 'Displays Help') do
			puts opts
			exit 1
		end
	end

	# Parse options
	begin
		opt_parser.parse(argv)
	rescue OptionParser::ParseError => e
		puts e
		puts
		puts opt_parser
		exit 1
	end

	return opt_parser
end

def create_dir_if_does_not_exist (directory_name)
	Dir.mkdir(directory_name) unless File.exist?(directory_name)
end

# Generate the required directires
def generate_build_directory()
	# Generate the platform name
	$platform_name = $options[:compiler] + "_" + $options[:platform]
	# Generate the build directory name
	$build_directory = "build_" + $platform_name
end

# Functions that generates the generator string based on the compiler and the platform for cmake
def get_generator_name()
	generator = " -G"

	if $options[:compiler] == "vc15"
		generator += "\"Visual Studio 15 2017"
	elsif $options[:compiler] == "vc14"
		generator += "\"Visual Studio 14 2015"
	elsif $options[:compiler] == "vc16"
		generator += "\"Visual Studio 16 2019\" "
	elsif $options[:compiler] == "makefile"
		generator += "\"Unix Makefiles\""
	else
		puts "\nERROR: Compiler '#{$options[:compiler]}' not recognized."
		exit 1
	end

	# Only append arch if not vc16
	if not $options[:compiler] =="vc16"
		if $options[:platform] == "win64"
			generator += " Win64\" "
		end
	end

	return generator
end

# String that defines the platform name that we will be compiling for (unique according to the target compiler/linktype/platform)
def get_platform_name()
	return " -DPROJECT_PLATFORM_NAME=" + $platform_name
end

# String that defines output directory
def get_output_directory()
	return " -DPROJECT_OUTPUT_DIRECTORY=" + $output_directory
end

# For a given setup, generates projects and compiles the library
def generate_project()
	# Create the build folder
	create_dir_if_does_not_exist($build_directory)

	# Move into the build build_directory if possible
	Dir.chdir($build_directory) do
		# We need to build the cmake command
		command = CMAKE_EXE + " .."
		# Get the generator name
		command += get_generator_name()
		# Inject the platfomr name
		command += get_platform_name()
		# Inject the output directory
		command += get_output_directory()

		# Execute the cmake command
		if !system(command)
			puts("CMake generation failed")
			exit 1
		else
			puts("cmake project generated")
		end
	end
end

def compile_sdk()
	Dir.chdir($build_directory) do
		# Compile the project in release mode
		if $options[:build] == "release"
			if !system(CMAKE_EXE + " --build . --config Release")
				exit 1
			end
		end
		if $options[:build] == "debug"
			if !system(CMAKE_EXE + " --build . --config Debug")
				exit 1
			end
		end
	end
end

parse_options(ARGV, $options)

# Setting the default OptionParser
if $options[:compiler] == nil
	$options[:compiler] = "vc16"
end

if $options[:platform] == nil
	$options[:platform] = "win64"
end

if $options[:link_type] == nil
	$options[:link_type] = "mt"
end

if $options[:build] == nil
	$options[:build] = "release"
end

if $options[:applications] == nil
	$options[:applications] = true
end

# Generate the directory names
generate_build_directory()

# Generate the project
generate_project()

# Compile the project
compile_sdk()