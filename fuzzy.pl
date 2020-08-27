#! /usr/bin/perl

# $Header:$

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;

# Tests:
# acc
# acc.c
# bas.c
# Bas.c

#######################################################################
#   Command line switches.					      #
#######################################################################
my %opts;
my %noext = (
	o => 1,
	cm => 1,
	so => 1,
	gif => 1,
	png => 1,
	bmp => 1,
	);

sub main
{
	Getopt::Long::Configure('require_order');
	Getopt::Long::Configure('no_ignore_case');
	usage() unless GetOptions(\%opts,
		'debug',
		'dir=s',
		'f=s',
		'files',
		'help',
		'src',
		's=s',
		);

	usage(0) if $opts{help};

	my %f;
	if ($opts{dir}) {
		foreach my $d (split(/,/, $opts{dir})) {
			my $type = '';
			$type = " -type f" if $opts{files};
			my $fh = new FileHandle("find $d $type |");
			while (<$fh>) {
				chomp;
				$f{$_} = 1;
			}
		}
	} elsif ($opts{f}) {
		my $fh = new FileHandle($opts{f});
		die "Cannot read $opts{f} - $!" if !$fh;
		while (<$fh>) {
			chomp;
			$f{$_} = 1;
		}
	} elsif ($opts{s}) {
		$f{$opts{s}} = 1;
	} else {
		foreach my $p (split(":", $ENV{PATH})) {
			$f{$_} = 1 foreach glob("$p/*");
		}
	}

	foreach my $f (keys(%f)) {
		my $e = basename($f);
		next if $e !~ /^.*\.(.*)$/;
		delete $f{$f} if $noext{$1};
	}

	my $pat = shift @ARGV;
	usage() if !$pat;

	my %h;
	my %m;

	my $dpat = $pat;
	$dpat =~ s/(.)/$1*/g;
	$dpat =~ s/\*$//;
#	print "dpat=$dpat\n";

	foreach my $f (sort(keys(%f))) {
		my $b = basename($f);
		my $r;

		if ($b eq $pat) {
			$h{$f} = 10;
			$r = "X" x length($b);
		} elsif (lc($b) eq lc($pat)) {
			$h{$f} = 9;
			$r = "X" x length($b);
		} elsif ($r = re_match($b, $pat, bol => 1)) {
			$h{$f} = 8;
		} elsif ($r = re_match($b, $pat, bol => 1, ic => 1)) {
			$h{$f} = 7;
		} elsif ($r = re_match($b, $pat)) {
			$h{$f} = 6;
		} elsif ($r = re_match($b, $pat, ic => 1)) {
			$h{$f} = 5;
		} elsif ($r = re_match($b, $dpat, ic => 1)) {
			$h{$f} = 4;
		}
#print "r=$r\n";
		$m{$f} = $r;
	}

	my %seen;
	my @lst;
	foreach my $h (sort { $h{$b} <=> $h{$a} } keys(%h)) {
		push @lst, [$h{$h}, $h, $m{$h}];
	}

	display($_, $pat) foreach @lst;

}

sub display
{	my $parray = shift;
	my $pat = shift;

	my $mask = $parray->[2];

	my $where = $parray->[0];
	print "\033[37m$where - ";
	print " mask=$mask -" if $opts{v};
	my $p = $parray->[1];

	print dirname($p) . "/";
	my $b = basename($p);
	if (length($mask) != length($b)) {
		print "error mask mismatch:\n";
		print "  b=$b\n";
		print "  m=$mask\n";
		return;
	}

	for (my $i = 0; $i < length($b); $i++) {
		my $ch = substr($b, $i, 1);
		if ((substr($mask, $i, 1) || '') eq 'X') {
			print "\033[36m$ch\033[37m";
		} else {
			print $ch;
		}
	}
	print "\n";
	if ($opts{debug}) {
		print "  b=$b\n";
		print "  m=$mask\n";
	}

}

sub re_match
{	my $b = shift;
	my $pat = shift;
	my %opts = @_;
#print "$_\n" foreach keys (%opts);

	my @blst = split(/\./, $b);
#print "b=$b !", join("!", @blst), "!\n" if $b =~ /valgrind/;
	my @plst = split(/\./, $pat);
	my $i;
	my $j = 0;
#print "$b - $pat - ", join(",", @blst), " ", join(",", @plst), "\n";

	my $m = '';

	for ($i = 0; $i < @blst; $i++) {
		my $b1 = $blst[$i];
		if ($i >= @plst) {
			for (; $i < @blst; $i++) {
				$m .= ".";
				$m .= "." x length($blst[$i]);
			}
			return $m;
		}
		my $p1 = $plst[$i];

		my $mat = re_match2($b1, $p1, \%opts);
#print "  cmp $b1 $p1 = $mat\n" if $b =~ /^acc/;
		return "" if $mat eq '';
		$m .= "." if $m;
		$m .= $mat;
	}
#print "-> good\n";
	return $m;
}
sub re_match2
{	my $b = shift;
	my $pat = shift;
	my $opts = shift;

	my $str = '';
	my $i = 0;
	my $j = 0;
#print "match2: $b $pat\n";
	for ($j = 0; $i < length($b) && $j < length($pat); ) {
		my $b1 = substr($b, $i, 1);
		my $p1 = substr($pat, $j, 1);
		if ($opts->{ic}) {
			$b1 = lc($b1);
			$p1 = lc($p1);
		}
		if ($b1 eq $p1) {
			$str .= 'X';
			$i++, $j++;
			next;
		}
		if ($p1 eq '*') {
			$i++;
			my $found = 0;
			while ($i < length($b)) {
				my $b1 = substr($b, $i, 1);
				$b1 = lc($b1) if $opts->{ic};
				if ($b1 eq $p1) {
					$i++;
					$found = 1;
					last;
				}
				$i++;
				$str .= " ";
			}
			last if !$found;
			$j++;
			next;
		}
		$i++;
		$str .= "_";
	}
	if ($j < length($pat)) {
#print "not found\n";
		return '';
	}
	while ($i++ < length($b)) {
		$str .= "_";
	}
#print "str=$str\n";
	return '' if $opts->{bol} && substr($str, 0, 1) ne 'X';
	return $str;
}
#######################################################################
#   Print out command line usage.				      #
#######################################################################
sub usage
{	my $ret = shift;
	my $msg = shift;

	print $msg if $msg;

	print <<EOF;
fuzzy.pl -- fuzzy filename matching
Usage: fuzzy.pl [switches] <search-term>

  This is a simple tool to do fuzzy filename matching. It was designed
  as a learning experience/POC, to do fuzzy matching, similar to what
  many IDEs provide. "Cost based" ordering gets fiddly and tricky to debug.

  The initial approach for this was to try various algorithms for
  each item, and then order the results by "best". This code is designed
  to search for filenames - a different set of tests would be 
  appropriate if we were doing spelling corrections. Whilst
  this is useful for filenames, it can be used for code variable
  names, since they have the same layout.

  We need to provide list of candidate names to search from. This
  can be provided in a variety of forms:

  * If no switches are provided, then everything in \$PATH is chosen.
  * We can provide "-f <filename>" to give a generated or curated list
    of filenames
  * We can use "-s <name>" to add a single entry, for debug purposes.

  The results are listed in best-matching order, and color highlighting
  to show the match.

  We avoid native regular expressions - in reality, you do not
  use these to specify files, and it overly complicates things.

  Here is an example:

  \$ fuzzy.pl xt

  Without any switches, this would match, for instance:

  - /usr/bin/xterm
  - /usr/bin/xedit
  ...

  The preference is exact case matching, at the start of the filename.
  (Directory paths are ignored/stripped). "xterm" matches because
  it starts with "xt". "xedit" matches because of the "x" and "t".

  The search pattern is split on "." - so you could say:

  \$ fuzzy.pl a.j

  which might be used to match "argumentValidator.java". You can
  omit the extension, or use a prefix/substring to select from 
  similar files but different extensions.

Switches:

  -dir <path)
      Scan (find \$dir -print) to generate a list of filenames to match
      against.

  -f <filename>
      Specify a list of filenames to be matched against. E.g.

      \$ find . -type f > files.lst
      \$ fuzzy.pl -f files.lst abc

  -s <name>
      Add just <name> to the scan list - useful for debugging one-shot
      scenarios.

  -v  Verbose - some extra debug.

EOF

	exit(defined($ret) ? $ret : 1);
}

main();
0;

