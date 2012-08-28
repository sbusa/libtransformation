#!/usr/bin/perl

use Getopt::Long;

$result = GetOptions ( "min=s" => \$min, "max=s" => \$max, "summary!" => \$summary );

$min ||= 0;
$max ||= 100;

%threads = ();

while (<>) {

	($time, $id, $xform, $call) = m!^(\S+) (\S+) (.*) (\S+)$!;

	my @items;
	@items = @{$threads{ $id }} if exists $threads{ $id };
	push(@items, { xform => $xform, time => $time, call => $call } );
	$threads{ $id } = [ @items ];
#	print STDOUT scalar(@items)."\n";
}

for my $k1 ( keys %threads ) {
	my $lastxform = "";
	my $stime, $etime;
	print STDOUT "$k1 - ".scalar(@{$threads{$k1}})." events\n" if $summary;
	foreach my $item (@{$threads{ $k1 }}) {
		if ($$item{'xform'} ne $lastxform) {
			my $duration = $$item{'time'} - $stime;
			print STDOUT "$k1 $lastxform $duration\n" if $lastxform and ($duration > $min) and ($duration < $max);
			$stime = $$item{'time'};
			$lastxform = $$item{'xform'};
		}
	}
}
