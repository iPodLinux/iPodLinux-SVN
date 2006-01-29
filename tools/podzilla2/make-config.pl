#!/usr/bin/perl -w

use File::Find   qw< find >;
use Data::Dumper qw< Dumper >;

open CONFIG_IN, ">Config.in" or die $!;

# Might look like:
#  { "." => "a bunch of lines of text",
#    "Games" => { "." => "a bunch of lines of text",
#                 "Strategy" => { "." => "a bunch of lines of text" },
#                 ...
#               },
#    ...
#  }
our %MenuStructure;

my($sec,$min,$hour,$mday,$mon,$year,$wday,$yday) = localtime time;
$year += 1900; $mon++;
print CONFIG_IN <<EOF;
#
# Automatically generated podzilla2 Config.in. Do not edit.
# Generated @{[ sprintf("%04d-%02d-%02d %02d:%02d:%02d", $year, $mon, $mday, $hour, $min, $sec) ]}
# Do not edit.
#
mainmenu "podzilla2 module configuration"

config MODULES
    bool
    default y

EOF

sub parse_menu_stuff {
    my($fh) = shift;
    my($root) = shift;

    while (<$fh>) {
        my($line) = $_;
        if ($line =~ /^\s*menu\s+"(.*?)"/) {
            my($name) = $1;
            $root->{$name} = {} unless defined $root->{$name};
            parse_menu_stuff($fh, $root->{$name});
        } elsif ($line =~ /^\s*endmenu/) {
            return;
        } else {
            $root->{'.'} = "" unless defined $root->{'.'};
            $root->{'.'} .= $line;
        }
    }
}

sub process_file {
    if (m[/Module$]) {
        my(%modfile);
        open MODFILE, $File::Find::name or die "$!";
        while (<MODFILE>) {
            chomp;
            /^#/ and next;
            /^\s*$/ and next;
            /^\s*([A-Z][A-Za-z-]+):\s*(.*)\s*$/ or do { warn "bad Module file line"; next; };
            my($prop,$val) = ($1,$2);
            $prop =~ s/-//;
            $modfile{$prop} = $val;
        }

        if (open KCF, "$File::Find::dir/Config") {
            parse_menu_stuff *KCF{IO}, \%MenuStructure;
            close KCF;
        } else {
            $modfile{Category} = "Miscellaneous" unless defined $modfile{Category};

            my($cat) = $modfile{Category};
            my(@tree) = split /\//, $cat;
            my($menuptr) = \%MenuStructure;

            while (scalar @tree) {
                my($nextent) = shift @tree;
                $menuptr->{$nextent} = {} unless defined $menuptr->{$nextent};
                $menuptr = $menuptr->{$nextent};
            }

            my($deps) = "";
            if (defined $modfile{Dependencies}) {
                my(@deps) = split /,/ => $modfile{Dependencies};
                for (@deps) {
                    $deps .= "\n    select MODULE_$_";
                }
            }

            $modfile{Displayname} = $modfile{Module} unless defined $modfile{Displayname};
            $modfile{Description} = $modfile{Displayname} unless defined $modfile{Description};

            $menuptr->{'.'} = "" unless defined $menuptr->{'.'};
            $menuptr->{'.'} .= <<EOF;
config MODULE_$modfile{Module}
    tristate "$modfile{Displayname}"
    default m$deps
    help
      $modfile{Description}
      
      This is a module with no configuration information; if you need more
      information about it, check http://ipodlinux.org/Special:Module/$modfile{Module}.

EOF
        }
    }
}

find { wanted => \&process_file, no_chdir => 1 }, 'modules';

sub write_menu {
    my($fh) = shift;
    my($root) = shift;

    print $fh $root->{'.'} if defined $root->{'.'};
    for (keys %$root) {
        /^\.$/ and next;
        print $fh "\nmenu \"$_\"\n";
        write_menu($fh, $root->{$_});
        print $fh "endmenu\n";
    }
}

write_menu *CONFIG_IN{IO}, \%MenuStructure;

print CONFIG_IN "\n# End:\n";
close CONFIG_IN;
exit 0;
