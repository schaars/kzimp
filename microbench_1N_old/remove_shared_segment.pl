#!/usr/bin/env perl

# permissions
$permission=666;

# username?
chomp($username = `whoami`);

#print("Username: ".$username."\n");
#print("Permission: ".$permission."\n");

open(IPCS,"ipcs -m |") || die "Failed: $!\n";
while ( <IPCS> )
{
   #print($_);

   if ($_ =~ m/^0x[\da-fA-F]+\s+(\d+)\s+$username\s+$permission/) {
      print("Killing $1\n");
      system("ipcrm -m $1");
   }
}
close(IPCS);
