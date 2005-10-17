#!/usr/bin/perl -w

use IO::Socket;
use Net::IRC;
use Data::Dumper;
use POSIX qw/getpid setsid/;

$PORT = 25;

$irc = new Net::IRC;
$conn = $irc->newconn( Server   => 'irc.freenode.net',
		       Port     => 6667,
		       Nick     => 'iPL-SVN',
		       Ircname  => 'iPodLinux SVN-tracking bot',
		       Username => 'iplsvn');

sub do_smtp_stuff {
    $server = IO::Socket::INET->new( Proto     => 'tcp',
				     LocalPort => $PORT,
				     Listen    => SOMAXCONN,
				     Address   => '0.0.0.0',
				     Reuse     => 1);

    die "can't setup server" unless $server;
    print "[Server $0 accepting clients on port $PORT]\n";

    my($client);
    while ($client = $server->accept()) {
	my $pid = fork();
	die "Cannot fork" unless defined $pid;
	do { close $client; next; } if $pid; # parent
	
	my($address) = "";
	my($data) = "";
	my($datamode) = 0;
	
	$client->autoflush(1);
	print $client "220 this is svnbot.pl at your service\n";
	while (<$client>) {
	    s/\n//g; s/\r//g;

	    if ($datamode) {
		if (/^.$/) {
		    $datamode = 0;
		    print $client "250 ok, queued\n";
		    parse_data($data);
		} else {
		    $data .= $_ ."\n";
		}
	    } else {
		next unless /\S/;       # blank line
		if (/^quit/i) {
		    print $client "221 bye\n";
		    last;
		} elsif (/^helo/i or /^ehlo/i) {
		    print $client "250 hello to you too\n";
		} elsif (/^mail/i) {
		    print $client "250 ok, ignored\n";
		} elsif (/^rcpt to:\s*<?([-A-Za-z_.+0-9]*)@([-A-Za-z_.0-9]*)>?/i) {
		    $address = $1;
		    $domain = $2;
		    if ($domain eq "get-linux.org") {
			print $client "250 ok, sending to $1\n";
		    } else {
			print $client "551 buzz off, I'm not an open relay\n";
			print $client "221 goodbye, spammer\n";
			last;
		    }
		} elsif (/^data/i) {
		    $datamode = 1;
		    print $client "354 go ahead\n";
		} else {
		    print $client "502 go read rfc821\n";
		}
	    }
	}
	close $client;
    }
}


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

    $conn->privmsg ("#ipodlinux.dev", "03$user * $flist: $log ($tracurl)");
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
    $self->join("#ipodlinux.dev", "rs232");
    $self->privmsg("nickserv", "identify ipsofatso");

    do_smtp_stuff unless fork;
}

my($bg) = 1;

if (defined($ARGV[0])) {
    if ($ARGV[0] eq "-k") {
	open PIDFILE, "svnbot.pid" or die "svnbot not running\n";
	$pid = <PIDFILE>; chomp $pid; close PIDFILE;
	unlink "svnbot.pid";
	kill -15, $pid or exit 1;
	exit 0;
    } elsif ($ARGV[0] eq "-f") {
	$bg = 0;
    }
}

if ($bg) {
    exit 0 if fork; setsid;
}
	
open PIDFILE, ">svnbot.pid" and do {
    print PIDFILE getpid;
    close PIDFILE;
};

$conn->add_global_handler(376, \&on_connect);
$conn->add_handler("msg", \&on_msg);
$irc->start;

exit 0;
