#!/usr/bin/perl -w

use IO::Socket;
use Net::IRC;
use Data::Dumper;

use POSIX qw/getpid setsid/;

my($CHANNEL) = "#ipodlinux";
my($KEY)     = "";

sub makeTTK($) {
    my($rev) = shift;

    chdir "/home/oremanj/dev/ipl/management/.svnc/ttk";
    system "svn up >svnerr 2>&1" and do { $conn->privmsg("#ipodlinux", "svn up in ttk failed $?"); return; };
    system "make distclean all >builderr 2>&1" and do { $conn->privmsg("#ipodlinux", "Someone just broke the TTK build."); return; };
    return 1;
}

sub makeAP($) {
    my($rev) = shift;
    chdir "/home/oremanj/dev/ipl/management/.svnc/ttk";
    system "svn up >svnerr 2>&1" and do { $conn->privmsg("#ipodlinux", "svn up in ttk failed $?"); return; };
    system "zip -r /var/www/html/iplbuilds/appearance-$rev.zip fonts schemes -x ".'\*/.svn/\*'." >/dev/null";
    system "tar czf /var/www/html/iplbuilds/appearance-$rev.tar.gz --exclude .svn fonts schemes >/dev/null";
}

sub makePZcore($) {
    my($rev) = shift;

    chdir "/home/oremanj/dev/ipl/management/.svnc/podzilla2";
    system "svn up >svnerr 2>&1" and do { $conn->privmsg("#ipodlinux", "svn up in pz2 failed $?"); return; };
    system "make mrproper defconfig all IPOD=1 TTKDIR=../ttk >builderr 2>&1" and do { $conn->privmsg("#ipodlinux", "Someone just broke the PZ2 build."); return; };
    system "cat podzilla | gzip -9 > /var/www/html/iplbuilds/podzilla2-$rev.gz";
    return 1;
}

sub makePZmods($) {
    my($rev) = shift;

    chdir "/home/oremanj/dev/ipl/management/.svnc/podzilla2";
    system "svn up >svnerr 2>&1" and do { $conn->privmsg("#ipodlinux", "svn up in pz2 failed $?"); return; };
    system "make defconfig all xpods IPOD=1 TTKDIR=../ttk >builderr 2>&1" and do { $conn->privmsg("#ipodlinux", "Someone just broke the PZ2 build (modules)."); return; };
    system "( cd xpods; zip -r /var/www/html/iplbuilds/pzmodules-$rev.zip * >/dev/null 2>&1 )";
    system "( cd xpods; tar czf /var/www/html/iplbuilds/pzmodules-$rev.tar.gz * >/dev/null 2>&1 )";
    return 1;
}

sub parse_data($;$) {
    my(@data) = split /\n/, $_[0];
    my($send) = $_[1];
    $send = 1 unless defined $send;
    my($headers) = 1;
    my($user,$date,@log,$tracurl);
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
		    push @log, $val;
		} elsif ($key eq "Trac view") {
		    ($tracurl = $val) =~ s#https://OpenSVN.csie.org/traccgi/courtc/trac.cgi/changeset#http://tinyurl.com/bukaa#;
		} elsif ($key eq "Changes") {
		    /\s\s([A-Z_]{1,2})\s*\+\d+\s*-\d+\s*(.*)/ or print "Couldn't figure out email\n";
		    my($flags,$file) = ($1,$2);
                    my($mflag,$pflag) = (substr($flags,0,1), substr($flags,1));
                    my($bits) = "";
                    $bits .= "+" if $mflag eq "A";
                    $bits .= "-" if $mflag eq "R" or $mflag eq "D";
                    $bits .= "*" if $pflag eq "U";
                    $bits .= "^" if $pflag eq "A";
                    $bits .= "v" if $pflag eq "R" or $pflag eq "D";
                    if ($bits ne "") {
                        push @files, $file."[$bits]";
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
    my($rev);
    ($rev = $tracurl) =~ s!.*/!!;

    map { s|/[^/]*$|| } @dirs;
    @dirs = keys %{{ map { $_ => 1 } @dirs }};

    if (grep { m[/ttk/(fonts|schemes)] } (@files)) {
        do { makeAP($rev); kill 9 => $$ } unless fork;
    }

    if (grep { m[/ttk/src] } (@files)) {
        do { makeTTK($rev) && makePZcore($rev) && makePZmods($rev); kill 9 => $$ } unless fork;
    } elsif (grep { m[/podzilla2/core] } (@files)) {
        do { makePZcore($rev) && makePZmods($rev); kill 9 => $$ } unless fork;
    } elsif (grep { m[/podzilla2/modules] } (@files)) {
        do { makePZmods($rev); kill 9 => $$ } unless fork;
    }

    if (scalar @files == 1) {
	$flist = $files[0];
        $flist =~ s#^((?:tools/)?[^/]+/)#10$1# if $send;
    } else {
	my(@fileparts) = map { [ split m|/| ] } @files;
	my($samelevels) = 0;
	
	while ((scalar grep { $_->[$samelevels] eq $fileparts[0]->[$samelevels] } @fileparts) == scalar @fileparts and $samelevels < 50) {
	    $samelevels++;
	}
        $samelevels = 0 if $samelevels >= 50;
	
	my($firstpart) = ($files[0] =~ m|^((?:[^/]+/){$samelevels})|);
	$firstpart = "" unless defined $firstpart;
	map { s|^$firstpart|| } @files;

        $firstpart = "10$firstpart" if $send;

	if (scalar @files > 4) {
	    if (scalar @dirs > 1) {
		$flist = $firstpart . " (". scalar(@files) ." files in ". scalar(@dirs) ." directories)";
	    } else {
		$flist = $firstpart . " (". scalar(@files) ." files)";
	    }
	} else {
	    $flist = $firstpart . " (@files)";
	}
    }

    my($log) = join " " => @log;
    if (length $log >= 256) {
	$log = substr $log, 0, 255;
    }

    $conn->privmsg ($CHANNEL, "03$user * $flist: $log ($tracurl)") if $send;
    return "$user * $flist: $log ($tracurl)";
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
	exec "./svnbot.pl";
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
    alarm 5;
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
        chdir "/home/oremanj/dev/ipl/management";
	-d ".svnc" or mkdir ".svnc";
	chdir ".svnc";
	umask 0;
	open FILE, ">".time.".".getpid.".svn" or die "Can't open >SOMEID.svn: $!\n";
	print FILE while <STDIN>;
	close FILE;
	exit 0;
    } elsif ($ARGV[0] eq "-t") {
        @newfiles = </home/oremanj/dev/ipl/management/.svnc/*.*.svn>;
        for (@newfiles) {
            local $/;
            open THISF, $_;
            my($data) = <THISF>;
            close THISF;
            print parse_data($data, 0), "\n";
        }
        exit 0;
    } elsif ($ARGV[0] eq "-f") {
	$bg = 0;
    }
}

if ($bg) {
    exit 0 if fork; setsid;
}

$SIG{'ALRM'} = sub {
    @newfiles = </home/oremanj/dev/ipl/management/.svnc/*.*.svn>;
    for (@newfiles) {
	local $/; # slurp mode
	open THISF, $_;
	my($data) = <THISF>;
	close THISF;
	unlink;

	parse_data $data;
    }
    alarm 5;
};
$SIG{'CHLD'} = sub { wait; };

$CHANNEL = shift @ARGV || "#ipodlinux";
$KEY = shift @ARGV || "";

open PIDFILE, ">.svnc/svnbot.pid" or die "Can't open PID file; do you need to mkdir .svnc?\n";
print PIDFILE getpid;
close PIDFILE;

$irc = new Net::IRC;
$conn = $irc->newconn( Server   => 'irc.freenode.net',
		       Port     => 6667,
		       Nick     => 'iPL-SVN',
		       Ircname  => 'SVN changes: http://tinyurl.com/cdcdj',
		       Username => 'iplsvn');

$conn->add_global_handler(376, \&on_connect);
$conn->add_handler("msg", \&on_msg);
$irc->start;

exit 0;
