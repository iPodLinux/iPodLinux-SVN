#!/usr/bin/perl -w

use Net::IRC;
use LWP::Simple;
use URI::Escape;
use IO::Handle;
use Data::Dumper;

### Init
my $irc = new Net::IRC;
my $conn = $irc->newconn(Server => 'irc.freenode.net',
			 Port => 6667,
			 Nick => ($ARGV[0] || 'iplbot'),
			 Ircname => 'iPodLinux bot, second incarnation',
			 Username => 'iplbot')
  or die "Can't connect to IRC server.\n";

open LOG, ">>iplbot.log";
LOG->autoflush(1);

### The body of the bot.
my %Factoids;
my @Todos; # [idx] = { id => ID, name => name of person doing it, priority => priority, desc => description }
my($TodoLastID) = 0;
my %Karma;
my %Countdowns;
my %CountdownHelpers;
my %NicksHere;
my(@GoodNicks) = (); # josh_ aegray BleuLlama courtc davidc__ leachbj Luke veteran macPod coob
my(%IdNicks);
my($ReverseMode) = 0;

sub handle_line($$$$;$);

sub excellent($) {
    my($nick) = shift or die;
    $Karma{$nick} = 0 unless exists $Karma{$nick};
    $Karma{$nick} += 3;
}
sub good($) {
    my($nick) = shift or die;
    $Karma{$nick} = 0 unless exists $Karma{$nick};
    $Karma{$nick}++;
}
sub bad($) {
    my($nick) = shift or die;
    $Karma{$nick} = 0 unless exists $Karma{$nick};
    $Karma{$nick}--;
}
sub terrible($) {
    my($nick) = shift or die;
    $Karma{$nick} = 0 unless exists $Karma{$nick};
    $Karma{$nick} -= 3;
}

sub trivial_get($$$)
{
   my($host, $port, $path) = @_;
   #print "HOST=$host, PORT=$port, PATH=$path\n";

   require IO::Socket;
   local($^W) = 0;
   my $sock = IO::Socket::INET->new(PeerAddr => $host,
                                    PeerPort => $port,
                                    Proto    => 'tcp',
                                    Timeout  => 60) || return undef;
   $sock->autoflush;
   my $netloc = $host;
   $netloc .= ":$port" if $port != 80;
   print $sock join("\015\012" =>
                    "GET $path HTTP/1.0",
                    "Host: $netloc",
                    "User-Agent: Mozilla/5.0 (X11; U; Linux x86_64; en-US; rv:1.7.6) Gecko/20050411 Firefox/1.0.2",
                    "", "");

   my $buf = "";
   my $n;
   1 while $n = sysread($sock, $buf, 8*1024, length($buf));
   return undef unless defined($n);

   if ($buf =~ m,^HTTP/\d+\.\d+\s+(\d+)[^\012]*\012,) {
       my $code = $1;
       if ($code =~ /^30[1237]/ && $buf =~ /\012Location:\s*(\S+)/i) {
           # redirect
#           my $url = $1;
           return undef;# if $loop_check{$url}++;
#           return trivial_get($url, $host, $port, $path);
       }
       if ($code !~ /^2/) {
	   print STDERR "Error $code\n";
       }
       return undef unless $code =~ /^2/;
       $buf =~ s/.+?\015?\012\015?\012//s;  # zap header
   }

   return $buf;
}

our($depth) = 0;

our($np) = qr{
\[
(?:
   (?> [^][]+ )
 |
   (??{ $np })
)*
\]
             }x;


sub send_line($$$$;$) {
    my($self,$line,$nick,$dest) = @_;
    unless ($IdNicks{$nick} > 0) { return; }

    $self->privmsg($dest, &handle_line||"");
}
##                                 ##
# Arg 1: $self (object)             #
# Arg 2: the incoming line (string) #
# Arg 3: nick who sent it (string)  #
# Arg 4: where to send response     #
##                                 ##
sub handle_line($$$$;$) {
    my($self,$line,$nick,$dest,$prefix) = @_;
    my($wordy) = ($nick eq $dest);
    $prefix = "" unless defined($prefix);

    if ($line =~ /^\[.*\]$/) {
	$line =~ s/^\[//; $line =~ s/\]$//;
    }

    if ($depth >= 20) {
	return "... no, I'm not going to crash.";
    }

    if ($nick ne "iplbot") {
        $depth++;
        $line =~ s/[~`]($np)/handle_line($self,$1,$nick,$dest)||""/egx; #`]//;# to appease xemacs
        $depth--;
    }

    my(@words) = split/\s+/, $line;
    my($rest) = join " ", @words[1..$#words];
    my($nickok) = 0;
    if (grep { /^\Q$nick\E$/i } (@GoodNicks)) {
	$nickok = 1;
	unless (exists $IdNicks{$nick} and $IdNicks{$nick}>0) {
	    $IdNicks{$nick} = 0;
	    $self->privmsg("NickServ", "list $nick");
	    $self->privmsg($nick, "You must be identified to use the privileged parts of the bot.");
	    $self->schedule(2, \&send_line, $line, $nick, $dest);
	    $self->privmsg($nick, "If you are, wait a few seconds.");
	    return;
	}
    }
    if ($nick eq "iplbot") { $nickok = 1; }
    $words[0] = lc $words[0];

    if ($words[0] eq "literal") {
	if (exists $Factoids{$rest}) {
	    my($reply) = $Factoids{$rest};
	    $reply =~ s/^is\s+//;
	    return "$reply";
	}
	return "I literally don't know what you're talking about.";
    }

    if ($words[0] eq "setkarma") {
	if ($nick ne "iplbot") { bad $nick; return "Loophole closed. :-)"; }
	$Karma{$words[1]} = int($words[2]);
	return;
    }

    if ($words[0] eq "braindump") {
	$self->privmsg($nick, "I know about ".join(", ", keys(%Factoids)).".");
	return;
    }

    if ($words[0] eq "say") {
	return "$rest";
    }

    if ($words[0] eq "latest") {
	my($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time());
	$year += 1900;
	$month = sprintf "%02d", $mon+1;
	$day = sprintf "%02d", $mday;
	return "Kernel: http://www.ipodlinux.org/builds/$year-$month-$day-kernel.bin.gz  ||  Podzilla: http://www.ipodlinux.org/builds/$year-$month-$day-podzilla.gz  ||  Remember to gunzip the files. If podzilla looks corrupted, check if the first four letters are 'bFLT'; if so, it's not compressed.";
    }

    if ($words[0] eq "wiki" or $words[0] eq "wikisearch") { SEARCH: {
	if (!$rest) {
	    return "Use the WIKI. http://www.ipodlinux.org/";
	} else {
            if ($words[0] ne "wikisearch") {
                $content = get("http://www.ipodlinux.org/".uri_escape($rest));
                if (defined($content) && $content !~ /currently no text in this page/) {
                    $retstr = "$rest (http://www.ipodlinux.org/".uri_escape($rest).")  [".length($content)." bytes]";
		    $self->privmsg($dest, $retstr);
                    return "";
                }
            }
            $content = get("http://www.ipodlinux.org/Special:Search?search=".
                           uri_escape($rest)."&fulltext=Search");
	    if (!defined($content)) {
		return "Error fetching <http://www.ipodlinux.org/Special:Search?search=".
		  uri_escape($rest)."&fulltext=Search>";
	    }
	    if ($content =~ /unsuccessful search/) {
		return "No matches. (By the way, words <= 3 letters aren't indexed.)";
	    }

	    my($firstmatch) = 1;
	    my($infosent) = 0;
	    my($result) = 1;
	    my($results) = 5;
	    while ($content =~ m[<li><a href="(.*?)" title=".*?">(.*?)</a> \((\d+) bytes\)]g) {
		$retstr = "$2 (http://www.ipodlinux.org$1)  [$3 bytes]";
		if ($dest eq $nick and $words[0] eq "wikisearch") {
		    $retstr = "$result. ".$retstr;
		}
		if ($firstmatch) {
		    $self->privmsg($dest, $retstr);
		    $firstmatch = 0;
		} else {
		    if (!$infosent and $dest ne $nick) {
			$self->privmsg($dest, "I'll /msg you the next @{[$results-1]} results.");
			$infosent = 1;
		    }
		    $self->privmsg($nick, $retstr);
		}
		last if $result >= $results or $words[0] ne "wikisearch";
		$result++;
	    }
	    if ($firstmatch) { return "No match, apparently (bug in iplbot)." }
	    return "";
	}
    }}

    if ($words[0] eq "google") {
	$dst = "";
	$longlist = 0;
	$summfirst = 0;
	if ($nick eq $dest) { $dst = $nick; $longlist = 1; $summfirst = 0; }
	elsif ($rest =~ /for me/) { $rest =~ s/\s*for me//; $dst = $nick; $longlist = 1; $summfirst = 1; }
	elsif ($rest =~ /for (\S+)/) { $dst = $1; $rest =~ s/\s*for (\S+)//; $longlist = 1; $summfirst = 1; }
	else { $dst = $dest; $longlist = 0; $summfirst = 1; }

	$content = trivial_get("www.google.com", 80, "/search?hl=en&q=".uri_escape($rest)."&btnG=Google+Search");
	if (!defined($content)) {
	    return "Error fetching <http://www.google.com/search?hl=en&q=".
	      uri_escape($rest)."&btnG=Google+Search>";
	}
	if ($content =~ /did not match any documents/) {
	    return "No match.";
	}

	$content =~ s/&#(\d+);/chr($1)/eg;
	
	my($firstmatch) = 1;
	my($infosent) = 0;
	my($result) = 1;
	my($results) = 5;
	while ($content =~ m[<p class=g>.*?<a href="(.*?)".*?>(.*?)</a>.*?<font size=-1>(.*?)<br>]sg) {
	    $retstr = "$2 ($1): $3";
	    $retstr =~ s|</?b>||g;
	    if (length $retstr > 200) {
		$retstr = substr($retstr,0,197)."...";
	    }
	    if ($firstmatch and $summfirst) {
		$self->privmsg($dest, $retstr);
		$firstmatch = 0;
	    }
	    if ($longlist) {
		$retstr = "$result. ".$retstr;
	    }
	    if (!$infosent and $longlist and $summfirst) {
		$self->privmsg($dest, "I'll /msg you the next @{[$results-1]} results.");
		$infosent = 1;
	    }
	    if ($longlist) {
		$self->privmsg($dst, $retstr);
	    }
	    last if $result >= $results or !$longlist;
	    $result++;
	}
	if ($firstmatch and $summfirst) { return "No match, apparently (bug)." }
	return "";	
    }

#    if ($words[0] eq "countdown") {
#	my($thing) = $words[1];
#	if ($line =~ /.*in\s+(.+)/) {
#	    my($offset) = $1;
#	    $offset =~ s/(\d+)/+$1/g;
#	    $offset =~ s/days?/*86400/g;
#	    $offset =~ s/weeks?/*86400*7/g;
#	    $offset =~ s/months?/*86400*30/g;
#	    $offset =~ s/years?/*86400*365/g;
#	    $offset =~ s/(\s+|[a-zA-Z`$()]+)//g;
#	    $Countdowns{$words[1]} = time() + eval($offset);
#	    $CountdownHelpers{$words[1]} = [];
#	} elsif ($line =~ /.*at\s+(\d+)/) {
#	    $Countdowns{$words[1]} = $1;
#	    $CountdownHelpers{$words[1]} = [];
#	} elsif (!exists $Countdowns{$words[1]}) {
#	    return "I don't know anything about a countdown to $words[1].";
#	}
#	return "Tentative release date for $thing: ".localtime($Countdowns{$words[1]});
#    }
#    if ($words[0] eq "when") {
#	$Countdowns{$words[1]} += 86400*30;
#	my($asker) = $dest;
#	if ($asker =~ /^#/) {
#	    $asker = $words[2];
#	}
#	push @{$CountdownHelpers{$words[1]}}, $asker;
#	if (defined($prefix) and $prefix =~ /^#/) {
#	    $dest = $prefix;
#	}
#	$self->privmsg($dest, "Tentative release date for $words[1]: ".localtime($Countdowns{$words[1]}));
#	$self->privmsg($dest, "Some of those responsible for the delay: ".join(', ', @{$CountdownHelpers{$words[1]}}));
#	return "";
#    }
#    if ($words[0] eq "blame") {
#	push @{$CountdownHelpers{$words[3]}}, $words[1];
#	$self->privmsg($dest, "Tentative release date for $words[3]: ".localtime($Countdowns{$words[3]}));
#	$self->privmsg($dest, "Some of those responsible for the delay: ".join(', ', @{$CountdownHelpers{$words[3]}}));
#	return "";
#    }
#	
#    if ($words[0] eq "nocountdown") {
#	delete $Countdowns{$words[1]};
#	delete $CountdownHelpers{$words[1]};
#	return "Countdown $words[1] deleted.";
#    }
    if ($words[0] eq "karma") {
	if (exists $Karma{$words[1]}) {
	    if ($Karma{$words[1]} >= 1000000) {
		return "$words[1]'s karma is infinitely good.";
	    } elsif ($Karma{$words[1]} <= -1000000) {
		return "$words[1]'s karma is infinitely bad.";
	    }
	    return "$words[1]'s karma is $Karma{$words[1]}.";
	}
	return "$words[1] has no karma.";
    }
    if ($words[0] eq "extol") {
	if (!$nickok) { return; }
	$Karma{$words[1]} = 10000000;
	return;
    }
    if ($words[0] eq "abhor") {
	if (!$nickok) { return; }
	$Karma{$words[1]} = -10000000;
	return;
    }
    if ($words[0] eq "excellent") {
	if (!$nickok) { return; }
	if ($words[1] eq $nick) { $self->privmsg($nick, "You can't change your own karma."); return; }
	$Karma{$words[1]} = 0 unless exists $Karma{$words[1]};
	$Karma{$words[1]} += 3;
	return ($wordy?"$words[1]'s karma is now $Karma{$words[1]}.":"");
    }
    if ($words[0] =~ /\+\+$/) {
	$words[0] =~ s/\+\+$//;
	($words[1],$words[0]) = ($words[0],"good");
    }
    if ($words[0] =~ /--$/) {
	$words[0] =~ s/--$//;
	($words[1],$words[0]) = ($words[0],"bad");
    }
    if ($words[0] eq "good") {
	if ($words[1] eq $nick) { $self->privmsg($nick, "You can't change your own karma."); return; }
	$Karma{$words[1]} = 0 unless exists $Karma{$words[1]};
	$Karma{$words[1]}++;
	return ($wordy?"$words[1]'s karma is now $Karma{$words[1]}.":"");
    }
    if ($words[0] eq "bad") {
	if ($words[1] eq $nick) { $self->privmsg($nick, "You can't change your own karma."); return; }
	$Karma{$words[1]} = 0 unless exists $Karma{$words[1]};
	$Karma{$words[1]}--;
	return ($wordy?"$words[1]'s karma is now $Karma{$words[1]}.":"");
    }
    if ($words[0] eq "terrible") {
	if (!$nickok) { return; }
	if ($words[1] eq $nick) { $self->privmsg($nick, "You can't change your own karma."); return; }
	$Karma{$words[1]} = 0 unless exists $Karma{$words[1]};
	$Karma{$words[1]} -= 3;
	return ($wordy?"$words[1]'s karma is now $Karma{$words[1]}.":"");
    }
    if ($words[0] eq "voices") {
	return "Listening to: < @GoodNicks >";
    }
    if ($words[0] eq "listen") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	push @GoodNicks, $words[1]||"";
	return ($wordy?"Now listening to $words[1]. Listening to: < @GoodNicks >":"");
    }
    if ($words[0] eq "ignore") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	my($badnick) = $words[1]||"";
	@GoodNicks = grep { !m/^\Q$badnick\E$/ } (@GoodNicks);
	if (!scalar @GoodNicks) {
	    @GoodNicks = ( $nick );
	}
	return ($wordy?"Now ignoring $badnick. Listening to: < @GoodNicks >":"");
    }
    if ($words[0] eq "forget") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	if (exists $Factoids{$rest}) {
	    delete $Factoids{$rest};
	    return ($wordy?"I forgot about $rest, $nick.":"");
	}
	return ($wordy?"I didn't know anything about $rest, $nick.":"");
    }

    if ($words[0] eq "todo") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
        push @Todos, { id => ++$TodoLastID, name => $words[1], priority => $words[2], desc => join(" ", @words[3..$#words]) };
        my($ret) = $TodoLastID.". @".$words[2]." <".$words[1]."> ".$Todos[$#Todos]{desc};
        @Todos = sort { $b->{priority} <=> $a->{priority} } @Todos;
        return $ret;
    }
    if ($words[0] eq "todump") {
        my($dumpy) = Data::Dumper::Dumper(\@Todos);
        $dumpy =~ s/\n/ \\ /g;
        return $dumpy;
    }
    if ($words[0] eq "todone") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
        my($oldnum) = scalar @Todos;
        @Todos = grep { $_->{id} != $words[1] } @Todos;
        return ($oldnum - scalar @Todos
)." todos killed off.";
    }
    if ($words[0] eq "todos") {
        if ($rest =~ /^\s*$/ || $rest =~ /for (me|\w+)/) {
            my($target) = $1;
            $target = "" unless defined($target);
            $target = $nick if $target eq "me";
            if (scalar(@Todos) > 0) {
                my($ret);
                if ($target eq $nick) {
                    $ret = "Your issues";
                } elsif ($target) {
                    $ret = "${target}'s issues";
                } else {
                    $ret = "Most pressing issues";
                }
                my($maxnr) = 5;
                my($comma) = 0;
                foreach (@Todos) {
                    last if !$maxnr;
                    next if $target && $target !~ /$_->{name}_*/;
                    $ret .= ($comma++? ", " : ": ");
                    if (!$target) {
                        $ret .= "<03".$_->{name}."> ";
                    }
                    $ret .= "(#".$_->{id}.") ".$_->{desc};
                    $maxnr--;
                }
                $ret .= ", ..." unless $maxnr;
                if ($maxnr == 5) { return "Nothing todo for $target."; }
                return $ret;
            }
            return "Nothing todo.";
        } elsif ($words[1] =~ /#(\d+)/) {
            my($id) = $1;
            foreach (@Todos) {
                if ($_->{id} == $id) {
                    return "<03".$_->{name}."> ".$_->{desc};
                }
            }
            return "Couldn't find it.";
        }
        return "Huh?";
    }

    if ($words[0] eq "part") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$self->part ($words[1], $nick);
	return;
    }
    if ($words[0] eq "join") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$self->join ($words[1], $words[2]);
	return ($wordy?"Joined $words[1].":"");
    }

    if ($words[0] eq "write") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	open FACTOIDS, ">factoids.txt";
	my($nfac,$nkar,$nblame) = (0,0,0);
	foreach (keys %Factoids) {
	    print FACTOIDS "$_ $Factoids{$_}\n";
	    $nfac++;
	}
	foreach (keys %Karma) {
	    print FACTOIDS "setkarma $_ $Karma{$_}\n";
	    $nkar++;
	}
	foreach (@GoodNicks) {
	    print FACTOIDS "listen $_\n";
	}
        foreach (@Todos) {
            print FACTOIDS "todo ".$_->{name}." ".$_->{priority}." ".$_->{desc}."\n";
        }
#	foreach (keys %Countdowns) {
#	    print FACTOIDS "countdown $_ at $Countdowns{$_}\n";
#	}
#	foreach (keys %CountdownHelpers) {
#	    my($ctdn) = $_;
#	    for (@{$CountdownHelpers{$_}}) {
#		print FACTOIDS "blame $_ for $ctdn\n";
#		$nblame++;
#	    }
#	}
	close FACTOIDS;
	return "Wrote $nfac factoids".($nkar?" and $nkar karma data":"").".";
    }	
    if ($words[0] eq "mode") {
	if (!$nickok) { terrible $nick; return; }
	if (substr($words[1],0,1) eq "#") {
	    $dest = $words[1];
	    $rest = join " ", @words[2..$#words];
	}
	$self->mode ($dest, $rest);
	return;
    }
    if ($words[0] eq "kick") {
	my($whom);
	if (!$nickok) { terrible $nick; return; }
	if (substr($words[1],0,1) eq "#") {
	    $dest = $words[1];
	    $whom = $words[2];
	    $rest = join " ", @words[3..$#words];
	} else {
	    $whom = $words[1];
	    $rest = join " ", @words[2..$#words];
	}
	$self->kick ($dest, $whom, $rest);
	return;
    }
    if ($words[0] eq "leave") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$self->quit ($nick);
	sleep(2);
	open FACTOIDS, ">factoids.txt";
	foreach (keys %Factoids) {
	    print FACTOIDS "$_ $Factoids{$_}\n";
	}
	foreach (keys %Karma) {
	    print FACTOIDS "setkarma $_ $Karma{$_}\n";
	}
	foreach (@GoodNicks) {
	    print FACTOIDS "listen $_\n";
	}
        foreach (@Todos) {
            print FACTOIDS "todo ".$_->{name}." ".$_->{priority}." ".$_->{desc}."\n";
        }
#	foreach (keys %Countdowns) {
#	    print FACTOIDS "countdown $_ at $Countdowns{$_}\n";
#	}
#	foreach (keys %CountdownHelpers) {
#	    my($ctdn) = $_;
#	    for (@{$CountdownHelpers{$_}}) {
#		print FACTOIDS "blame $_ for $ctdn\n";
#	    }
#	}
	close FACTOIDS;
	close LOG;
	exit(0);
    }
    if ($words[0] eq "nick") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$self->nick ($words[1]);
	return;
    }
    if ($words[0] eq "qquit") {	
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$self->quit ("$nick");
	close LOG;
	exit 0;
    }
    if ($words[0] eq "qupdate") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$self->quit ("Restarted by $nick");
	close LOG;
	sleep 2;
	exec "./$0";
    }
    if ($words[0] eq "update") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	open FACTOIDS, ">factoids.txt";
	foreach (keys %Factoids) {
	    print FACTOIDS "$_ $Factoids{$_}\n";
	}
	foreach (keys %Karma) {
	    print FACTOIDS "setkarma $_ $Karma{$_}\n";
	}
	foreach (@GoodNicks) {
	    print FACTOIDS "listen $_\n";
	}
        foreach (@Todos) {
            print FACTOIDS "todo ".$_->{name}." ".$_->{priority}." ".$_->{desc}."\n";
        }
#	foreach (keys %Countdowns) {
#	    print FACTOIDS "countdown $_ at $Countdowns{$_}\n";
#	}
#	foreach (keys %CountdownHelpers) {
#	    my($ctdn) = $_;
#	    for (@{$CountdownHelpers{$_}}) {
#		print FACTOIDS "blame $_ for $ctdn\n";
#	    }
#	}
	close FACTOIDS;
	$self->quit ("Restarted by $nick");
	close LOG;
	exec "./$0";
    }
    if ($words[0] eq "tell") {
	if (!$nickok) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }

	if ($line =~ /tell\s+(\S+)\s+(?:about\s+)?(.*)/) {
	    $self->privmsg ($1, handle_line($self,$2,$nick,$1)||"");
	    return;
	}
    }
    if ($words[0] eq "inform") {
	if (!$nickok && $words[1] =~ /^#/) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }

	if ($line =~ /inform\s+(\S+)\s+(?:about\s+)?(.*)/) {
	    my($res) = (handle_line($self,$2,$nick,$1,$dest)||"");
	    if ($res ne "") {
		return "$1, ".$res;
	    }
	    return "";
	}
    }
    if ($line =~ /(.+?)\s+(is.+|means.+|are.+)/) {
	my($word,$factoid) = ($1,$2);
        $word =~ s/^fact\s+//;
	if (!$nickok and $nick ne "iplbot" and $word ne $nick) { $self->privmsg($nick, "You aren't allowed to do that."); bad $nick; return; }
	$Factoids{$word} = $factoid;
	return ($wordy?"OK, $nick.":"");
    }	

    keys %Factoids;   # resets `each' iterator
    # Iterate through the whole factoid list.
    while (($k, $v) = each %Factoids) {
	my($key) = $k;
	$key =~ s/([][.^\$+*?])/\\$1/g if $key =~ /[][.^\$+*?]/;
	$key =~ s/\\\$\d+/(.*?)/g; # fix arg specifier

	my($input) = $line;
	$input=~s/(^\s+|\s+$)//;      # strip whitespace
	
	if ($input =~ /^$key$/i) {
	    my($fact) = $v;
	    my($one,$two,$three,$four,$five) = ($1,$2,$3,$4,$5);
	    $fact =~ s/\$1/$one/g    if $one;
	    $fact =~ s/\$2/$two/g    if $two;
	    $fact =~ s/\$3/$three/g  if $three;
	    $fact =~ s/\$4/$four/g   if $four;
	    $fact =~ s/\$5/$five/g   if $five;
	    $fact =~ s/^\s+//;
	    $fact =~ s/\$who/$nick/gi;

	    my(@factoids) = split /\s \\ \s/x, $fact;
	    if (scalar @factoids > 1) {
		my($prefix) = "$key ";
		if ($fact =~ /<reply>/) {
		    $prefix = "";
		    $fact =~ s/<reply>\s*//; $fact =~ s/(is|means|are)\s+//;
		}
		@factoids = split /\s \\ \s/x, $fact;

		foreach (@factoids) {
		    my($factoid) = $_;
		    my($suffix) = (($factoid eq $factoids[$#factoids])? "  [from $nick]" : "");
		    sleep(1);
		    $self->privmsg($dest, $prefix.$factoid.($wordy? "" : $suffix));
		    $prefix = "";
		}
		return "";
	    } else {
		if ($fact =~ /<reply>/) {
		    $fact =~ s/<reply>\s*//; $fact =~ s/(is|means|are)\s+//;
		    return "$fact  ".($wordy?"":"[from $nick]");
		} elsif ($fact =~ /<action>/) {
		    $fact =~ s/<action>\s*//; $fact =~ s/(is|means|are)\s+//;
		    $self->ctcp('action',$dest,$fact);
		    return;
		} else {
		    return "$key $fact  ".($wordy?"":"[from $nick]");
		}
	    }
	}
    }

    my(@goodwords) = split/\s+/, $line;
    my($bestfact,$bestnum) = ("",0);
    my($num);

    keys %Factoids;
    while (($k, $v) = each %Factoids) {
	$num = 0;
	for (@goodwords) {
	    my($word) = $_;
	    my($kk) = $k;
	    $num += ($kk =~ s/\Q$word\E//g);
	}
	if ($num > $bestnum) {
	    $bestnum = $num;
	    $bestfact = $k;
	}
    }
    if ($bestnum) {
	return "I assume you meant $bestfact. ".handle_line($self,$bestfact,$nick,$dest)||"";
    }

    if ($wordy) {
	return "Sorry, $nick, I don't know anything about $line.";
    }
    return;
}


### The handlers
sub on_connect {
    my $self = shift;
    $self->join("#ipodlinux.bot");
    $self->join("#ipodlinux");
    $self->join("#ipodlinux.ttk");
    # Ha! You thought you'd actually get the password! :-)
    $self->privmsg("nickserv", "identify ________");
    print "Joined #ipodlinux and #ipodlinux.bot.\n";
    print "Loading factoids... ";
    if (-e "factoids.txt") {
	open FACTOIDS, "factoids.txt";
	while (<FACTOIDS>) {
	    chomp;
	    handle_line($self, $_, "iplbot", "nonexistent");
	}
	close FACTOIDS;
    }
    print "Ready.\n";
}
sub on_init {
    my ($self, $event) = @_;
    my (@args) = ($event->args);
    shift(@args);

    print "*** @args\n";
}
sub on_join {
    my ($self, $event) = @_;
    my (@args) = ($event->args);
    my (@to) = ($event->to);

    print LOG "-!- join ".$event->nick."\n";
    $NicksHere{$to[0]} = {} unless exists $NicksHere{$to[0]};
    $NicksHere{$to[0]}{$event->nick} = 1;
    my($nick) = $event->nick;
    if (grep { /\Q$nick\E/ } (@GoodNicks)) {
	$self->privmsg("NickServ", "list ".$event->nick);
	$IdNicks{$event->nick} = 0;
    }
}
sub on_nick {
    my ($self, $event) = @_;
    my (@args) = ($event->args);
    my (@to) = ($event->to);
    $NicksHere{$to[0]}{$event->nick} = 0;
    $NicksHere{$to[0]}{$args[0]} = 1;

    print LOG "-!- ".$event->nick." is now known as ".$args[0]."\n";
    $IdNicks{$event->nick} = 0;
}
sub on_part {
    my ($self, $event) = @_;
    my (@args) = ($event->args);
    my (@to) = ($event->to);
    $NicksHere{$to[0]}{$event->nick} = 0;

    print LOG "-!- part ".$event->nick." (@args)\n";
    $IdNicks{$event->nick} = 0;
}
sub on_notice {
    my($self,$event) = @_;
    my($nick) = $event->nick;
    my($arg) = $event->args;
    if (lc $nick eq "nickserv" and $arg =~ /^(\S+).*ONLINE/) {
	$IdNicks{$1} = 1;
	print LOG "-!- $1 is identified.\n";
    }
}
sub on_msg {
    my($self,$event) = @_;
    my ($nick) = ($event->nick);
    my ($arg) = ($event->args);

    return if $nick =~ /iplbot/;

    print LOG "[$nick] $arg\n";

    $arg =~ s/^[~`]+//;
    $self->privmsg($nick, handle_line($self, $arg, $nick, $nick)||"");
}
my(%Warned);
my($thisnick,$lastnick) = ('','');
sub on_public {
    my($self,$event) = @_;
    my @to = $event->to;
    my ($nick) = ($event->nick);
    my ($arg) = ($event->args);

    if ($nick ne $thisnick) {
	$lastnick = $thisnick;
	$thisnick = $nick;
    }

    return if $nick =~ /iplbot/;

    print LOG "<$nick> $arg\n";

    if ($arg =~ /(^|\W)(thx|thks|tks|tnx|thanks|thank\s*you)(\W|$)/) {
	my($found) = 0;
	for (keys %Karma) {
	    if ($arg =~ /(^|\W)($_)(\W|$)/) {
		good $2;
		$found = 1;
		last;
	    }
	}
	good $lastnick unless $found;
    }
    if ($arg =~ /(\S+)[:,] /) {
	my($x) = $1;
	if (exists $NicksHere{$to[0]} and exists $NicksHere{$to[0]}{$x} and ($NicksHere{$to[0]}{$x} == 0)) {
	    $self->privmsg($to[0], "I think he left.");
	    $NicksHere{$to[0]}{$x} = 1; # don't say it again
	}
    }
    if ($arg =~ /[Ii].*(working\s+on|writing)/) {
	excellent $nick;
    }
    if ($arg =~ /[Ii].*releas(e|ing)/) {
	if ($arg =~ /4g/) {
	    bad $nick;
	} else {
	    excellent $nick;
	}
    }
    if ($arg =~ /(when|how\s+close|how\+long).*(4[Gg]|fourth[- ]gen(eration)?|photo|ipod.*color|color.*ipod).*support(ed)?/i) {
	bad $nick;
    }

    $ReverseMode = 1 if $arg =~ /esrever\s*([~`]|[:,]toblpi)$/ or $arg =~ /^([~`]|iplbot[,:])\s*reverse/;
    $ReverseMode = 0 if $arg =~ /drawrof\s*([~`]|[:,]toblpi)$/ or $arg =~ /^([~`]|iplbot[,:])\s*forward/;

    $arg = reverse $arg if $ReverseMode;

    if ($arg =~ /^([~`]|iplbot[,:])/) {
	$arg =~ s/^([~`]+|iplbot[,:]\s*)//;

	my($dest) = $to[0];
	if (!grep { /^\Q$nick\E$/ } (@GoodNicks)) {
	    $dest = $nick;
	}

	if ($ReverseMode) {
	    $self->privmsg($dest, scalar reverse(handle_line($self, $arg, $nick, $dest)||""));
	} else {
	    $self->privmsg($dest, handle_line($self, $arg, $nick, $dest)||"");
	}
    } elsif ($arg =~ /~\[.*\]/) {
	if (!grep { /^\Q$nick\E$/ } (@GoodNicks)) {
	    return;
	}

	$max = 3;

	while ($arg =~ /~($np)/g) {
	    if (!$max) {
		$self->privmsg($nick, "Only up to 3 offhand substitutions per line, please.");
		last;
	    }
	    if ($ReverseMode) {		
		$self->privmsg($to[0], scalar reverse(handle_line($self, $1, $nick, $to[0])||""));
	    } else {
		$self->privmsg($to[0], handle_line($self, $1, $nick, $to[0])||"");
	    }
	    $max--;
	}
    }
}
sub on_ping {
    my($self,$event) = @_;
    $self->ctcp_reply($event->nick, join(' ', $event->args));
}


### Set up handlers
$conn->add_handler('cping', \&on_ping);
$conn->add_handler('msg',   \&on_msg);
$conn->add_handler('public',\&on_public);
$conn->add_handler('umode', \&on_umode);
$conn->add_handler('notice',\&on_notice);
$conn->add_handler('names', \&on_names);
$conn->add_handler('nick',  \&on_nick);
$conn->add_handler('join',  \&on_join);
$conn->add_handler('part',  \&on_part);

$conn->add_global_handler([ 251,252,253,254,302,255 ], \&on_init);
$conn->add_global_handler(376, \&on_connect);
$conn->add_global_handler(433, \&on_nick_taken);

print "Starting...\n";

$irc->start;
