#!/usr/bin/env perl
#
# $ARGV[0]: the file where you can find the list of directories to work on

use strict;
use warnings;

die("Usage: $0 <file_where_to_find_list>\n") if (!defined($ARGV[0]));

open(FILE, "$ARGV[0]") or die "can't open $ARGV[0] for reading\n";
my @lines = <FILE>;
close(FILE);

for my $dir (@lines) {
   if ($dir =~ m/(\d+)consumers/) {
      $nbconsumers=$1
      chomp($nblines = `wc -l $dir`);
      system("./extract_latencies.py $dir $nbconsumers $nblines");     
   }
}

