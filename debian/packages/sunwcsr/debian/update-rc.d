#! /usr/bin/perl
#
# update-rc.d	Update the links in /etc/rc[0-9S].d/ and SMF manifest
#

$initd = "/etc/init.d";
$etcd  = "/etc/rc";
$notreally = 0;

# Print usage message and die.

sub usage {
	print STDERR "update-rc.d: error: @_\n" if ($#_ >= 0);
	print STDERR <<EOF;
usage: update-rc.d [-n] [-f] <basename> remove
       update-rc.d [-n] <basename> defaults|multiuser [NN | sNN kNN]
       update-rc.d [-n] <basename> start|stop NN runlvl [runlvl] [...] .
		-n: not really
		-f: force
EOF
	exit (1);
}

# Check out options.

while($#ARGV >= 0 && ($_ = $ARGV[0]) =~ /^-/) {
	shift @ARGV;
	if (/^-n$/) { $notreally++; next }
	if (/^-f$/) { $force++; next }
	if (/^-h|--help$/) { &usage; }
	&usage("unknown option");
}

# Action.

&usage() if ($#ARGV < 1);
$bn = shift @ARGV;

$manifest = "";
$manifest_state = "";
$manifest_fmri = "";
if ( ! -r "/etc/svc/volatile/repository_door" ) {
    $ENV{SVCCFG_REPOSITORY} = "/etc/svc/repository.db";
}
if (-x "/lib/svc/method/$bn") {
    open(FD,"/lib/svc/method/$bn manifest|");
    $manifest = <FD>; chop $manifest;
    close FD;
    if (! -f $manifest) {
        print STDERR "update-rc.d: $manifest: file does not exist\n";
        exit (1);
    }
} else {
    open(FD,"/usr/sbin/svccfg -s system/manifest-import listprop general/${bn}_manifest|");
    $manifest = <FD>; chop $manifest; $manifest =~ s/^\s*\S+\s+\S+\s+(\S+)/$1/;
    close FD;
    if ($manifest ne "" && ! -f $manifest && $ARGV[0] ne 'remove') {
	print STDERR "update-rc.d: $manifest: file does not exist\n";
	exit (1);
    }
}
if ($manifest ne "") {
    $manifest_fmri = "svc:/$1/$2:default" if ($manifest =~ /\/var\/svc\/manifest\/([\w\/]+)\/(\w+)\.xml/);
    if (system("/usr/bin/svcs -Ho sta $manifest_fmri 2>/dev/null 1>&2") == 0) {
        open(FD,"/usr/bin/svcs -Ho sta $manifest_fmri|");
        $manifest_state = <FD>; chop $manifest_state;
        close FD;
    } elsif (system("/usr/sbin/svccfg -s $manifest_fmri listprop general/enabled 2>/dev/null 1>&2") == 0) {
        open(FD,"/usr/sbin/svccfg -s $manifest_fmri listprop general/enabled|");
	$svc_en = <FD>; chop $svc_en; $svc =~ s/^\s*\S+\s+\S+\s+(\S+)/$1/;
	close FD;
	$manifest_state = "DIS";
	if ($svc_en eq "true" &&
	    system("/usr/bin/svccfg -s $manifest_fmri listprop restarter/state 2>/dev/null 1>&2") == 0) {
            open(FD,"/usr/bin/svccfg -s $manifest_fmri listprop restarter/state|");
            $rstate = <FD>; chop $rstate; $rstate =~ s/^\s*\S+\s+\S+\s+(\S+)/$1/;
	    if ($rstate ne "") {
	        $manifest_state = "OFF";
	        $manifest_state = "ON"  if ($rstate eq "online");
	        $manifest_state = "DGD" if ($rstate eq "degraded");
	        $manifest_state = "MNT" if ($rstate eq "maintenance");
	        $manifest_state = "UN"  if ($rstate eq "uninitialized");
            }
            close FD;
	}
    }
}
    

if ($ARGV[0] ne 'remove') {
    if (! -f "$initd/$bn") {
	print STDERR "update-rc.d: $initd/$bn: file does not exist\n";
	exit (1);
    }
} elsif (-f "$initd/$bn") {
    if (!$force) {
	printf STDERR "update-rc.d: $initd/$bn exists during rc.d purge (use -f to force)\n";
	exit (1);
    }
}

$_ = $ARGV[0];
if    (/^remove$/)       { &checklinks ("remove"); }
elsif (/^defaults$/)     { &defaults; &makelinks }
elsif (/^multiuser$/)    { &multiuser; &makelinks }
elsif (/^(start|stop)$/) { &startstop; &makelinks; }
else                     { &usage; }

exit (0);

# Check if there are links in /etc/rc[0-9S].d/ 
# Remove if the first argument is "remove" and the links 
# point to $bn.

sub is_link () {
    my ($op, $fn, $bn) = @_;
    if (! -l $fn) {
	print STDERR "update-rc.d: warning: $fn is not a symbolic link\n";
	return 0;
    } else {
	$linkdst = readlink ($fn);
	if (! defined $linkdst) {
	    die ("update-rc.d: error reading symbolic link: $!\n");
	}
	if (($linkdst ne "../init.d/$bn") && ($linkdst ne "$initd/$bn")) {
	    print STDERR "update-rc.d: warning: $fn is not a link to ../init.d/$bn or $initd/$bn\n";
	    return 0;
	}
    }
    return 1;
}

sub checklinks {
    my ($i, $found, $fn, $islnk);

    $found = 0;

    if ($_[0] eq 'remove') {
        if ($manifest ne "") {
            if ($manifest_state eq "ON") {
                print " Disabling SMF service for $manifest ...\n";
                system("/usr/sbin/svcadm disable $manifest_fmri");
	    }
            if ($manifest_state eq "") {
                print " SMF service $manifest_fmri already unregistered...\n";
            } else {
                print " Removing SMF service $manifest_fmri ...\n";
                if (system("/usr/sbin/svccfg delete $manifest_fmri") == 0) {
                    system("/usr/sbin/svccfg -s system/manifest-import delprop general/${bn}_manifest");
		    $manifest_fmri =~ s/^svc:(.*):default/$1/;
                    system("/usr/sbin/svccfg delete $manifest_fmri");
	        }
            }
        }

        print " Removing any SMF legacy services for $initd/$bn ...\n";
	if (open(FD, "svccfg -s smf/legacy_run listpg |")) {
            my @lines = <FD>;
            close FD;
            for my $line (@lines) {
                chomp $line;
                next if ($line !~ /^(rc.*S[0-9][0-9]$bn).*/);
		system("svccfg -s smf/legacy_run delpg $1 2>/dev/null 1>&2");
	    }
	}

        print " Removing any system startup links for $initd/$bn ...\n";
    } elsif ($manifest ne "") {
        return ($manifest_state ne "");
    }

    foreach $i (0..9, 'S') {
	unless (chdir ("$etcd$i.d")) {
	    next if ($i =~ m/^[789S]$/);
	    die("update-rc.d: chdir $etcd$i.d: $!\n");
	}
	opendir(DIR, ".");
	foreach $_ (readdir(DIR)) {
	    next unless (/^[SK]\d\d$bn$/);
	    $fn = "$etcd$i.d/$_";
	    $found = 1;
	    $islnk = &is_link ($_[0], $fn, $bn);
	    next if ($_[0] ne 'remove');
	    if (! $islnk) {
		print "   $fn is not a link to ../init.d/$bn; not removing\n"; 
		next;
	    }
	    print "   $etcd$i.d/$_\n";
	    next if ($notreally);
	    unlink ("$etcd$i.d/$_") ||
		die("update-rc.d: unlink: $!\n");
	}
	closedir(DIR);
    }
    $found;
}

# Process the arguments after the "defaults" keyword.

sub defaults {
    my ($start, $stop) = (20, 20);

    &usage ("defaults takes only one or two codenumbers") if ($#ARGV > 2);
    $start = $stop = $ARGV[1] if ($#ARGV >= 1);
    $stop  =         $ARGV[2] if ($#ARGV >= 2);
    &usage ("codenumber must be a number between 0 and 99")
	if ($start !~ /^\d\d?$/ || $stop  !~ /^\d\d?$/);

    $start = sprintf("%02d", $start);
    $stop  = sprintf("%02d", $stop);

    $stoplinks[0] = $stoplinks[1] = $stoplinks[6] = "K$stop";
    $startlinks[2] = $startlinks[3] =
	$startlinks[4] = $startlinks[5] = "S$start";

    1;
}

# Process the arguments after the "multiuser" keyword.

sub multiuser {
    my ($start, $stop) = (20, 20);

    &usage ("multiuser takes only one or two codenumbers") if ($#ARGV > 2);
    $start = $stop = $ARGV[1] if ($#ARGV >= 1);
    $stop  =         $ARGV[2] if ($#ARGV >= 2);
    &usage ("codenumber must be a number between 0 and 99")
	if ($start !~ /^\d\d?$/ || $stop  !~ /^\d\d?$/);

    $start = sprintf("%02d", $start);
    $stop  = sprintf("%02d", $stop);

    $stoplinks[1] = "K$stop";
    $startlinks[2] = $startlinks[3] =
	$startlinks[4] = $startlinks[5] = "S$start";

    1;
}

# Process the arguments after the start or stop keyword.

sub startstop {

    my($letter, $NN, $level);

    while ($#ARGV >= 0) {
	if    ($ARGV[0] eq 'start') { $letter = 'S'; }
	elsif ($ARGV[0] eq 'stop')  { $letter = 'K' }
	else {
	    &usage("expected start|stop");
	}

	if ($ARGV[1] !~ /^\d\d?$/) {
	    &usage("expected NN after $ARGV[0]");
	}
	$NN = sprintf("%02d", $ARGV[1]);

	shift @ARGV; shift @ARGV;
	$level = shift @ARGV;
	do {
	    if ($level !~ m/^[0-9S]$/) {
		&usage(
		       "expected runlevel [0-9S] (did you forget \".\" ?)");
	    }
	    if (! -d "$etcd$level.d") {
		print STDERR
		    "update-rc.d: $etcd$level.d: no such directory\n";
		exit(1);
	    }
	    $level = 99 if ($level eq 'S');
	    $startlinks[$level] = "$letter$NN" if ($letter eq 'S');
	    $stoplinks[$level]  = "$letter$NN" if ($letter eq 'K');
	} while (($level = shift @ARGV) ne '.');
	&usage("action with list of runlevels not terminated by \`.'")
	    if ($level ne '.');
    }
    1;
}

# Create the links.

sub makelinks {
    my($t, $i);
    my @links;

    if (&checklinks) {
        if ($manifest ne "") {
	    print " SMF Service for $manifest already registered.\n";
	} else {
	    print " System startup links for $initd/$bn already exist.\n";
        }
	exit (0);
    }
    if ($manifest ne "") {
        print " Adding SMF service for $manifest ...\n";
        if (system("/usr/sbin/svccfg import $manifest") == 0) {
            system("/usr/sbin/svccfg -s system/manifest-import delprop general/${bn}_manifest 2>/dev/null 1>&2");
            system("/usr/sbin/svccfg -s system/manifest-import addpropvalue general/${bn}_manifest astring: '$manifest'");
            system("/usr/sbin/svccfg -s $manifest_fmri setprop general/enabled=true");
	    return 1;
        }
	print "update-rc.d: service not added.\n";
	exit 0;
    }
    print " Adding system startup for $initd/$bn ...\n";

    # nice unreadable perl mess :)

    for($t = 0; $t < 2; $t++) {
	@links = $t ? @startlinks : @stoplinks;
	for($i = 0; $i <= $#links; $i++) {
	    $lvl = $i;
	    $lvl = 'S' if ($i == 99);
	    next if ($links[$i] eq '');
	    print "   $etcd$lvl.d/$links[$i]$bn -> ../init.d/$bn\n";
	    next if ($notreally);
	    symlink("../init.d/$bn", "$etcd$lvl.d/$links[$i]$bn")
		|| die("update-rc.d: symlink: $!\n");
	}
    }

    1;
}
