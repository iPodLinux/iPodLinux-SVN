#!/usr/bin/perl -w
use strict;
use LWP::Simple;
use File::Copy;

sub extension($) {
    my($name) = shift;
    if ($name =~ /\.(tar\.[a-z0-9]+)$/) {
        return $1;
    }
    $name =~ s/^.*\.//;
    return $name;
}

sub basename($) {
    my($name) = shift;
    if ($name =~ /Special:Module/) {
        return "pzmodules";
    } elsif ($name =~ /^.*([a-zA-Z0-9_-]+).*?$/) {
        return $1;
    }
    return ($name =~ s/[^a-zA-Z0-9_-]/-/g);
}

sub mirror_file($$) {
    my ($remote, $local) = @_;
    if ($remote =~ m[^http://]) {
        print STDERR "Fetching ";
        if ($remote =~ /(YYYYMMDD|NNN)/) {
            my($dir, $file) = ($remote, $remote);
            $dir =~ s|(.*)/.*?$|$1/|;
            $file =~ s|^.*/||;
            print STDERR $dir;
            my($dirdata) = get ($dir);
            die " error getting directory listing\n" unless defined $dirdata;
            my(@dirlines) = split /\n/, $dirdata;
            my($cap) = undef;
            for (@dirlines) {
                my($pat) = $file;
                $pat =~ s/YYYYMMDD/(\\d\\d\\d\\d-?\\d\\d-?\\d\\d)/;
                $pat =~ s/NNN/(\\d\\d\\d+)/;
                if (m[<a href=".*?">$pat</a>]i) {
                    $cap = $1;
                }
            }
            if (defined $cap) {
                $file =~ s/YYYYMMDD/$cap/;
                $remote =~ s/YYYYMMDD/$cap/;
                $local =~ s/YYYYMMDD/$cap/;
                $file =~ s/NNN/$cap/;
                $remote =~ s/NNN/$cap/;
                $local =~ s/NNN/$cap/;
            }
            print STDERR $file;
        } else {
            print STDERR $remote;
        }
        print STDERR " -> $local... ";
        my($stat) = getstore ($remote, $local);
        is_success $stat or die "error $stat\n";
        print STDERR "ok\n";
    } else {
        print STDERR "Copying $remote -> $local... ";
        copy ($remote, $local) or die "$!\n";
        print STDERR "ok\n";
    }
}

# handle_pkglist ($pkglist_file, $local_dir, $remote_dir)
# returns local pkglist file name
sub handle_pkglist($$$);
sub handle_pkglist($$$) {
    my($pkglist, $localpath, $remotepath) = @_;
    my($fhin, $fhout);
    print STDERR ">>> Mirroring $pkglist -> $localpath...\n";
    -d $localpath || mkdir($localpath, 0755) || die "$localpath: $!\n";
    mirror_file ($pkglist, "$localpath/packages.ipl.in");
    open $fhin, "$localpath/packages.ipl.in" or die "$localpath/packages.ipl.in: $!\n";
    open $fhout, ">$localpath/packages.ipl" or die "$localpath/packages.ipl: $!\n";
    while (<$fhin>) {
        chomp;
        if (/^\s*<<\s*(\S+)\s*>>\s*$/) {
            my($basename) = basename($1);
            handle_pkglist ($1, "$localpath/$basename", "$remotepath/$basename");
            print $fhout "<< $remotepath/$basename/packages.ipl >>\n";
        } elsif (/^\s*\[(kernel|loader|file|archive)\]\s*([a-zA-Z0-9_-]+)\s+([0-9.YMDNCVS-]+[abc]?)\s*:\s*"(.*?)"\s*at\s+(\S+)(.*)/) {
            my($type, $name, $vers, $desc, $url, $rest) = ($1, $2, $3, $4, $5, $6);
            my($newname) = "$name-$vers.@{[extension $url]}";
            mirror_file ($url, "$localpath/$newname");
            print $fhout "[$type] $name $vers: \"$desc\" at $remotepath/$newname$rest\n";
        } else {
            print $fhout "$_\n";
        }
    }
    close $fhin;
    close $fhout;
    unlink "$localpath/packages.ipl.in";
    print STDERR "<<< Done mirroring $pkglist.\n";
}

if ($#ARGV != 2) {
    print STDERR ("Usage: $0 package-list-file local-path remote-path\n".
                  "  e.g. $0 http://ipodlinux.org/iPodLinux:Installation_sources \\\n".
                  "             ~/public_html/ipl_packages http://my.server/~me/ipl_packages\n".
                  "To make a mirror for networkless installation, use the same local-path\n".
                  "as remote-path.\n");
    exit 1;
}

our($PackageListFile) = $ARGV[0];
our($LocalPath) = $ARGV[1];
our($RemotePath) = $ARGV[2];

handle_pkglist $PackageListFile, $LocalPath, $RemotePath;
print "Done.\n";
exit 0;
