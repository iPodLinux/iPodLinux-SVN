#!/usr/bin/perl -w

use IO::Socket;
use Net::IRC;
use Data::Dumper;

use POSIX qw/getpid setsid/;

my($CHANNEL) = "#ipodlinux.bot";
my($KEY)     = "";

sub parse_data($) {
    my(@data) = split /\n/, $_[0];
    my($headers) = 1;
    my($user,$date,$log,$tracurl);
    my($key) = "";
    my(@files) = ();
    for (@data) {
	chomp;
	if (!$headers) {
	    next if /^$/;
	    if (/^([a-zA-Z0-9]+)\t(.*)/) { # username line
		($user,$date) = ($1,$2);
	    } elsif (/^\s\s([A-Z][a-z ]*):/) {
		$key = $1;
	    } elsif (/^\s\sFlag\sChanges\s\s\s\sPath/) {
		$key = "Changes";
	    } elsif (/^\s\s\s\s(.*)/ or $key eq "Changes") {
		$val = $1;
		if ($key eq "Log") {
		    $log = $val;
		} elsif ($key eq "Trac view") {
		    $tracurl = $val;
		} elsif ($key eq "Changes") {
		    /\s\s([A-Z])\s*\+\d+\s*-\d+\s*(.*)/ or print "Couldn't figure out email\n";
		    my($flag,$file) = ($1,$2);
		    if ($flag eq "A") {
			push @files, "$file"."[+]";
		    } elsif ($flag eq "R" or $flag eq "D") {
			push @files, "$file"."[-]";
		    } else {
			push @files, $file;
		    }
		}
	    }
	}
	if (/^$/) {
	    $headers = 0;
	}
    }

    my($flist) = "";
    my(@dirs) = @files;

    map { s|/[^/]*$|| } @dirs;
    @dirs = keys %{{ map { $_ => 1 } @dirs }};

    if (scalar @files == 1) {
	$flist = "@files";
    } else {
	my(@fileparts) = map { [ split m|/| ] } @files;
	my($samelevels) = 0;
	
	while ((scalar grep { $_->[$samelevels] eq $fileparts[0]->[$samelevels] } @fileparts) == scalar @fileparts) {
	    $samelevels++;
	}
	
	my($firstpart) = ($files[0] =~ m|^((?:[^/]+/){$samelevels})|);
	$firstpart = "" unless defined $firstpart;
	map { s|^$firstpart|| } @files;

	if (scalar @files > 4) {
	    if (scalar @dirs > 1) {
		$flist = "10".$firstpart . " (". scalar(@files) ." files in ". scalar(@dirs) ." directories)";
	    } else {
		$flist = "10".$firstpart . " (". scalar(@files) ." files)";
	    }
	} else {
	    $flist = "10".$firstpart . " (@files)";
	}
    }

    $conn->privmsg ($CHANNEL, "03$user * $flist: $log ($tracurl)");
}

sub on_msg {
    my($self,$event) = @_;
    my($nick) = $event->nick;
    my($arg) = $event->args;

    if ($nick ne "iplbot") {
	$self->privmsg ($nick, "Please use iplbot to talk to me. /msg iplbot tell iPL-SVN say quit.");
	return;
    }

    if ($arg =~ /quit/) {
	unlink "svnbot.pid";
	$SIG{'TERM'} = sub { };
	kill -15, getpid;
	exit 0;
    }
    if ($arg =~ /restart/) {
	$SIG{'TERM'} = sub { };
	kill -15, getpid;
	exec "svnbot.pl";
    }
    if ($arg =~ /join (#[a-zA-Z0-9.-]*) (.*)?/) {
	$self->join($1, $2);
    }
    if ($arg =~ /part (#[a-zA-Z0-9.-]*)/) {
	$self->part($1);
    }
}

sub on_connect {
    my $self = shift;
    $self->join($CHANNEL, $KEY);
    $self->privmsg("nickserv", "identify ipsofatso");
}

my($bg) = 1;

if (defined($ARGV[0])) {
    if ($ARGV[0] eq "-k") {
	open PIDFILE, ".svnc/svnbot.pid" or die "svnbot not running\n";
	$pid = <PIDFILE>; chomp $pid; close PIDFILE;
	unlink ".svnc/svnbot.pid";
	kill -15, $pid or exit 1;
	exit 0;
    } elsif ($ARGV[0] eq "-q") {
	# queue message
	-d ".svnc" or mkdir ".svnc";
	chdir ".svnc";
	open FILE, ">".time.".".getpid.".svn";
	print FILE while <STDIN>;
	close FILE;
	open PIDFILE, "svnbot.pid" or die "svnbot not running\n";
	$pid = <PIDFILE>; chomp $pid; close PIDFILE;
	kill 1, $pid or die "unable to signal svnbot\n";
	exit 0;
    } elsif ($ARGV[0] eq "-f") {
	$bg = 0;
    }
}

if ($bg) {
    exit 0 if fork; setsid;
}

$SIG{'HUP'} = sub {
    chdir ".svnc";
    opendir SVNC, "." or do {
	$conn->privmsg($CHANNEL, "I'm misconfigured. Bye.");
	exit 1;
    };
    @newfiles = grep { /^[0-9]*\.[0-9]*\.svn$/ } readdir SVNC;
    closedir SVNC;
    for (@newfiles) {
	local $/ = 0;
	open THISF, $_;
	my($data) = <THISF>;
	close THISF;

	print "== $_ ==\n";
        print <THISF>;
	print "<< End >>\n\n";

	parse_data $data;
    }
};

open PIDFILE, ">.svnc/svnbot.pid" or die "Can't open PID file; do you need to mkdir .svnc?\n";
print PIDFILE getpid;
close PIDFILE;

$irc = new Net::IRC;
$conn = $irc->newconn( Server   => 'irc.freenode.net',
		       Port     => 6667,
		       Nick     => 'iPL-SVN',
		       Ircname  => 'iPodLinux SVN-tracking bot',
		       Username => 'iplsvn');

$conn->add_global_handler(376, \&on_connect);
$conn->add_handler("msg", \&on_msg);
$irc->start;

exit 0;
