#!/usr/bin/perl
use strict;
use warnings;
use Data::Dumper;
use File::Basename;

my $res;
my $percent = 0.5;
if(!defined $ARGV[0] || $ARGV[0] eq "-h" || $ARGV[0] eq "--help") {
	print "Usage: $0 [--percent 0.x] <files>\n";
	print "\t--percent: percentage to ignore at the beginning and at the end of the run\n";
	exit;	
} elsif($ARGV[0] eq "--percent") {
	shift;
	$percent = shift;
}

my $dir = dirname $0;
my $app = $dir."/parser-sampling";
my $options = join(" ", @ARGV);

$res = `$app --ignore-samples $options 2>/dev/null`;
my @lines = split(/\n/, $res);
my @times;

for my $l (@lines) {
	#if($l =~m/#SAMPLES - Total duration of the bench (\d+) \((\d+) -> (\d+)\)/) {
	#	@times = ($1,$2,$3);
	#}
	if($l =~m/#RDT - Total duration of the bench (\d+) \((\d+) -> (\d+)\)/) {
		@times = ($1,$2,$3);
	}
}
my $min = $times[1] + ($times[0]*$percent);
my $max = $times[2];# - ($times[0]*$percent);
$res = `$app --min $min --max $max $options`;
print $res;

print "\n";
