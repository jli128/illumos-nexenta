#!/usr/bin/perl -w

my $pkg = $ARGV[0];
my $priority = $ARGV[1] ? $ARGV[1] : 'optional';
my $bldver = `cat RELEASE|awk -F= '/^BUILD/ {print \$2}'`; chomp $bldver;

if (!exists $ENV{GATEROOT}) {
	print "Need GATEROOT to be set\n";
	exit 1;
}

my $pkgdefsdir="$ENV{GATEROOT}/usr/src/pkgdefs";

if (!defined $pkg) {
	print "Need package name.\n";
	exit 1;
}

if (! -d $pkgdefsdir) {
	print "Directory '$pkgdefsdir' not found.\n";
	exit 1;
}

system("mkdir $pkg")
	if (! -d $pkg);

system("mkdir $pkg/debian")
	if (! -d "$pkg/debian");

system("cp sunwxge/debian/copyright $pkg/debian")
	if (! -f "$pkg/debian/copyright");

system("cp sunwxge/debian/rules $pkg/debian")
	if (! -f "$pkg/debian/rules");

if (! -f "$pkg/debian/changelog") {
	local *FD;
	open FD, ">$pkg/debian/changelog";
	print FD <<EOF;
$pkg (5.11.$bldver-1) nexenta; urgency=low

  * Initial release

 -- Nexenta Systems <support\@nexenta.com>  Wed, 12 Dec 2007 12:26:11 -0800
EOF
	close FD;
}

if (! -f "$pkg/debian/control") {
	local *FD;

	my $sunw_pkg = $pkg;
	$sunw_pkg =~ s/^(sunw)/\U$1/;
	$sunw_pkg .= ".i" if (! -d "$pkgdefsdir/$sunw_pkg" && -d "$pkgdefsdir/$sunw_pkg.i");

	my $short_desc = `cat $pkgdefsdir/$sunw_pkg/pkginfo.tmpl|awk -F= '/^NAME/ {print \$2}'`;
	chomp $short_desc;
	$short_desc =~ s/\"//g;

	my $long_desc = `cat $pkgdefsdir/$sunw_pkg/pkginfo.tmpl|awk -F= '/^DESC/ {print \$2}'`;
	chomp $long_desc;
	$long_desc =~ s/\"//g;

	my $depends = '';
	if (-f "$pkgdefsdir/$sunw_pkg/depend") {
		open FD, "$pkgdefsdir/$sunw_pkg/depend";
		my @lines = <FD>;
		close FD;
		for my $line (@lines) {
			next if ($line !~ /^P\s+(\w+)\s+/);
			my $dp = lc($1);
			my @existing_deps = ('sunwcakr', 'sunwkvm', 'sunwcsr', 'sunwckr',
					     'sunwcnetr', 'sunwcsu', 'sunwcsd', 'sunwcsl');
			if (! grep { /^$dp$/ } @existing_deps) {
				$depends .= ", $dp (>=5.11.$bldver-1)";
			}
		}
	}

	open FD, ">$pkg/debian/control";
	print FD <<EOF;
Source: $pkg
Section: base
Priority: $priority
Maintainer: Nexenta Systems <support\@nexenta.com>
Build-Depends: debhelper(>=4.9.5gnusol5)

Package: $pkg
Architecture: solaris-i386
Depends: \${shlibs:Depends}, sunwcakr (>=5.11.$bldver-1), sunwkvm (>=5.11.$bldver-1), sunwcsr (>=5.11.$bldver-1), sunwckr (>=5.11.$bldver-1), sunwcnetr (>=5.11.$bldver-1), sunwcsu (>=5.11.$bldver-1), sunwcsd (>=5.11.$bldver-1), sunwcsl (>=5.11.$bldver-1)$depends
Description: $short_desc
 $long_desc
 .
EOF
	close FD;
}

system("hg add $pkg");
