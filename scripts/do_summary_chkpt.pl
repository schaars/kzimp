#!/usr/bin/perl
#
# Make a summary of the experiments.
#  ARGV[0]: directory where to find the results
#  ARGV[1]: output file
#
# This script is mostly result filename agnostic.
# However, you cannot mix different communication mechanism: this is not taken into account

use strict;
use warnings;
use Data::Dumper;

# the hash map containing the results
my %summary = ();


# create the summary from the files in the directory given as the argument
sub create_summary {
   my ($comm_mech, $nb_nodes, $nb_iter, $chkpt_size, $msg_size, $chan_size, $limit_thr);
   my $DIR = shift || die "Missing argument: results_directory";

   foreach my $file (<$DIR/*.txt>) {
      # parse $file
      if (($comm_mech, $nb_nodes, $nb_iter, $chkpt_size, $msg_size, $chan_size) =
         $file =~ m/^$DIR\/(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B_(\d+)channelSize.txt/) {
         $limit_thr=0;
      } else {
         if (($comm_mech, $nb_nodes, $nb_iter, $chkpt_size, $msg_size, $limit_thr) =
            $file =~ m/^$DIR\/(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B_(\d+)snapPerSec.txt/) {
         } else {
            ($comm_mech, $nb_nodes, $nb_iter, $chkpt_size, $msg_size) =
            $file =~ m/^$DIR\/(.+)_(\d+)nodes_(\d+)iter_chkpt(\d+)_msg(\d+)B.txt/;
            $limit_thr=0;
         }
         $chan_size=0;
      }

      # get thr and compute latency
      my $last_line;
      open(FILE, "< $file") or die "Unable to open the file in read mode: $!\n";
      while(<FILE>) {
         $last_line = $_ if eof;
      }
      close(FILE);

      $last_line =~ m/thr= (\d+.\d+) snap\/s/;
      my $thr = $1;
      my $lat = 1.0 / $thr * 1000 * 1000;

      # add result to the hash map
      my $ptr = \%summary;
      $ptr->{$nb_nodes}->{$msg_size}->{$chkpt_size}->{$nb_iter}->{$chan_size}->{$limit_thr} = [$thr, $lat];
   }
}

# dump the summary to the file given as the argument
sub dump_summary {
   my $OUT = shift || die "Missing argument: output_file";

   #print Dumper(\%summary);

   open(FILE, "> $OUT") or die "Unable to open the file in write mode: $!\n";

   print FILE "#nb_nodes\tmsg_size\tchkpt_size\tnb_iter\tchannel_size\tlimit_thr\tthroughput\tlatency\n";

   for my $nb_nodes (sort{$a<=>$b} keys %summary) {
      my $sub1 = $summary{$nb_nodes};
      for my $msg_size (sort{$a<=>$b} keys %$sub1) {
         my $sub2 = $sub1->{$msg_size};
         for my $chkpt_size (sort{$a<=>$b} keys %$sub2) {
            my $sub3 = $sub2->{$chkpt_size};
            for my $nb_iter (sort{$a<=>$b} keys %$sub3) {
               my $sub4 = $sub3->{$nb_iter};
               for my $chan_size (sort{$a<=>$b} keys %$sub4) {
                  my $sub5 = $sub4->{$chan_size};
                  for my $limit_thr (sort{$a<=>$b} keys %$sub5) {
                     my @T = @{$sub5->{$limit_thr}};
                     print FILE "$nb_nodes\t$msg_size\t$chkpt_size\t$nb_iter\t$chan_size\t$limit_thr\t$T[0]\t$T[1]\n";
                  }
               }
            }
         }
      }
   }

   close(FILE);
}


# --- ENTRY POINT ---
# check arguments
if (@ARGV == 2) {
   create_summary($ARGV[0]);
   dump_summary($ARGV[1]);
} else {
   print "Usage: " . $0 . " <results_directory> <output_file>\n";
} 
