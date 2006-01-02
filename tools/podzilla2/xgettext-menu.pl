while (<STDIN>) {
    chomp;
    my($file) = $_;
    my($lineno) = 0;
    open FILE, $file or die $!;
    while (<FILE>) {
        chomp;
        $lineno++;
        if (m[pz_menu_add_(?:action|setting|option|legacy|ttkh|stub|after|top)\s*\(\s*"(/[^"]+)",] ||
           m[PZ_SIMPLE_MOD\s*\(\s*"[^"]*"\s*,\s*[^,]*\s*,\s*"(/[^"]+)]) {
            my($path) = $1;
            my(@components) = split m#/#, $path;
            shift @components if $components[0] eq "";
            print STDOUT sprintf "%-30s/* $file:$lineno */\n", "N_(\"$_\")" for @components;
        }
    }
    close FILE;
}
