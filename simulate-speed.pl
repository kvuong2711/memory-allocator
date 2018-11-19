#!/usr/bin/perl

use strict;

$| = 1;

# Keep this in sync with TRACEDIR in config.h
my $traces_dir = "/web/classes/Fall-2018/csci2021-010/ha/4/traces/";
#my $traces_dir = "../traces";

# Keep this in sync with DEFAULT_TRACEFILES in config.h
my @traces =
  (
   "amptjp-bal.rep",
   "cccp-bal.rep",
   "cp-decl-bal.rep",
   "expr-bal.rep",
   "coalescing-bal.rep",
   "random-bal.rep",
   "random2-bal.rep",
#   "binary-bal.rep",
#   "binary2-bal.rep",
   "realloc-bal.rep",
   "realloc2-bal.rep"
  );

# Keep this in sync with THRU_POINTS in config.h
my $thru_points = 40;

die "Run this script in the same directory as mdriver\n"
  unless -e "mdriver";

my $valgrind = "/usr/bin/valgrind";

die "simulate-speed.pl depends on Valgrind\n"
  unless -e $valgrind;

my $cache_conf = "--I1=32768,8,64 --D1=32768,8,64 --LL=8388608,16,64";

my $valgrind_cmd = "$valgrind --tool=callgrind --cache-sim=yes $cache_conf";

my $compare_wall = 0;
#my $compare_freq = 2.20e9; # i7-4770HQ CPU @ 2.20GHz
#my $compare_freq = 3.30e9; # i7-3770 CPU @ 3.40GHz
my $compare_freq = 3.60e9; # i7-4790 CPU @ 3.60GHz

sub run_bench {
    my($use_libc) = @_;

    my $total_insns = 0;
    my $total_ops = 0;
    print "           Trace   Instructions  Simulated time   Sim. Kop/sec\n";

    for my $trace (@traces) {
	my $trace_name = $trace;
	$trace_name =~ s/\.rep$//;
	my $trace_loc = "$traces_dir/$trace";
	my $driver_opts = "-v -c -f $trace_loc";
	$driver_opts .= " -l" if $use_libc;
	my $pid = open(MSGS, "$valgrind_cmd ./mdriver $driver_opts 2>&1 |");
	my $num_ops;
	while (<MSGS>) {
	    #print $_ if /Collected/;
	    if (/^ 0\s+yes\s+\d{1,3}%\s+(\d+)\s+[0-9.]+\s+\d+/) {
		$num_ops = $1;
	    }
	}
	$total_ops += $num_ops;
	close MSGS;
	printf "%16s ", $trace_name;
	unlink "callgrind.out.$pid";
	my $outfile = "callgrind.out.$pid.1";
	open(OUT, "<", $outfile)
	  or die "Failed to open callgrind output $outfile: $!";
	my(@event_names, @counts);
	while (<OUT>) {
	    if (/^events: (.*)$/) {
		@event_names = split(" ", $1);
	    } elsif (/^summary: (.*)$/) {
		@counts = split(" ", $1);
	    }
	}
	close OUT;
	my %events;
	@events{@event_names} = @counts;
	unlink $outfile;
	my $accesses = $events{"Dr"} + $events{"Dw"};
	my $misses = $events{"I1mr"} + $events{"D1mr"} + $events{"D1mw"};
	#printf "%12d %12d", $events{"Ir"}, $misses;
	my $sim_time = $events{"Ir"} / $compare_freq;
	my $kops = 0.001*$num_ops/$sim_time;
	printf "%14d  %12.6f s %14.0f", $events{"Ir"}, $sim_time, $kops;
	$total_insns += $events{"Ir"};
	if ($compare_wall) {
	    my $wall_secs;
	    open(MSGS, "./mdriver -v -f $trace_loc 2>&1 |");
	    while (<MSGS>) {
		if (/^ 0       yes\s+\d{1,3}%\s+\d+\s+([0-9.]+)\s+\d+/) {
		    $wall_secs = $1;
		}
	    }
	    close MSGS;
	    my $ipc = $events{"Ir"}/($wall_secs * $compare_freq);
	    my $mem_ratio = $accesses/$events{"Ir"};
	    my $miss_ratio = $misses/$events{"Ir"};
	    printf "%10.6f %7.5f %7.5g", $wall_secs, $ipc, $miss_ratio;
	}
	print "\n";
    }
    my $total_time = $total_insns/$compare_freq;
    my $total_kops = 0.001*$total_ops/$total_time;
    printf "           Total %14d  %12.6f s %14.0f\n", $total_insns,
      $total_time, $total_kops;

    return $total_insns;
}

print "Libc throughput\n";
my $libc_cost = run_bench(1);
print "\n";

print "mm.c throughput\n";
my $mm_cost = run_bench(0);

print "\n";

my $target_cost = 14_376_377;

if ($libc_cost < 14_200_000 or $libc_cost > 14_400_000) {
    print "Warning: libc performance on this machine is unexpected.\n";
    print "For accurate results, make sure you test on Ubuntu 18.04.\n";
    print "\n";
}

printf "Throughput ratio vs.   libc: %f\n", $libc_cost/$mm_cost;
printf "Throughput ratio vs. target: %f\n", $target_cost/$mm_cost;

my $target_ratio = $target_cost / $mm_cost;

my $thru_score = int(0.5 + $target_ratio * $thru_points);
if ($thru_score > $thru_points) {
    $thru_score = $thru_points;
} elsif ($thru_score < 0) {
    $thru_score = 0;
}
printf "Throughput score:            %2d/40\n", $thru_score;
