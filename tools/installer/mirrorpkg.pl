#!/usr/bin/perl -w
use strict;
use LWP::Simple;
use File::Copy;
use Cwd qw/realpath/;

our($PackagesDir) = "";
our($UsingPackagesDir) = 0;
our($NewPackages) = 0;

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
    $name =~ s/[^a-zA-Z0-9_-]/-/g;
    return $name;
}

sub filename($) {
    my($name) = shift;
    $name =~ s|.*/||;
    return $name;
}

sub mirror_file($$) {
    my($remote, $local) = @_;
    my($reallocal) = ($UsingPackagesDir && ($local !~ /packages.ipl.in/))? "$PackagesDir/@{[filename $local]}" : $local;
    my($rlsize) = 0;
    $rlsize = (stat $reallocal)[7] if -e $reallocal;

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
                $reallocal =~ s/YYYYMMDD/$cap/;
                $file =~ s/NNN/$cap/;
                $remote =~ s/NNN/$cap/;
                $local =~ s/NNN/$cap/;
                $reallocal =~ s/NNN/$cap/;
                $rlsize = (stat $reallocal)[7] if -e $reallocal;
            }
            print STDERR $file;
        } else {
            print STDERR $remote;
        }
        print STDERR " -> $local";
#        print STDERR " (link to $reallocal)" if $reallocal ne $local;
        print STDERR "... ";

        my($getit) = 1;
        if ($rlsize) {
            my($content_type, $document_length, $modified_time, $expires, $server) = head ($remote);
            $getit = 0 if defined($document_length) and $document_length == $rlsize;
        }
        if ($getit) {
            my($stat) = getstore ($remote, $reallocal);
            is_success $stat or die "error $stat\n";
            print STDERR "ok\n";
            $NewPackages++ unless $local =~ /packages.ipl.in/;
        } else {
            print STDERR "no need\n";
        }
        symlink $reallocal, $local unless $reallocal eq $local;
    } else {
        print STDERR "Copying $remote -> $local... ";
        copy ($remote, $reallocal) or die "$!\n";
        symlink $reallocal, $local unless $reallocal eq $local;
        print STDERR "ok\n";
        $NewPackages++ unless $local =~ /packages.ipl.in/;
    }
}

# handle_pkglist ($pkglist_file, $local_dir, $remote_dir)
# returns local pkglist file name
sub handle_pkglist($$$);
sub handle_pkglist($$$) {
    my($pkglist, $localpath, $remotepath) = @_;
    my($rp) = "$remotepath/";
    $rp = "" unless $rp =~ m[://];
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
            print $fhout "<< $rp$basename/packages.ipl >>\n";
        } elsif (/^\s*\[(kernel|loader|file|archive)\]\s*([a-zA-Z0-9_-]+)\s+([0-9.YMDNCVS-]+[abc]?)\s*:\s*"(.*?)"\s*at\s+(\S+)(.*)/) {
            my($type, $name, $vers, $desc, $url, $rest) = ($1, $2, $3, $4, $5, $6);
            my($newname) = "$name-$vers.@{[extension $url]}";
            mirror_file ($url, "$localpath/$newname");
            print $fhout "[$type] $name $vers: \"$desc\" at $rp$newname$rest\n";
        } else {
            print $fhout "$_\n";
        }
    }
    close $fhin;
    close $fhout;
    unlink "$localpath/packages.ipl.in";
    print STDERR "<<< Done mirroring $pkglist.\n";
}

if ($#ARGV < 2 || $#ARGV > 3) {
    print STDERR ("Usage: $0 package-list-file local-path remote-path [packages-dir]\n".
                  "  e.g. $0 http://ipodlinux.org/iPodLinux:Installation_sources \\\n".
                  "             ~/public_html/ipl_packages http://my.server/~me/ipl_packages\n".
                  "To make a mirror for networkless installation, use the same local-path\n".
                  "as remote-path.\n".
                  "If you specify packages-dir, packages will be downloaded into it and\n".
                  "symlinked.\n");
    exit 1;
}

our($PackageListFile) = $ARGV[0];
our($LocalPath) = $ARGV[1];
our($RemotePath) = $ARGV[2];
$PackagesDir = realpath($ARGV[3]) if $#ARGV == 3;
$UsingPackagesDir = 1 if $#ARGV == 3;
print "Packages dir: $PackagesDir\n" if $#ARGV == 3;

handle_pkglist $PackageListFile, $LocalPath, $RemotePath;
print "Done. $NewPackages packages downloaded.\n";
exit ($NewPackages == 0);
