#
# Copyright 1999, 2000, 2001 Patrik Stridvall
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

package setup;

use strict;

BEGIN {
    use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
    require Exporter;

    @ISA = qw(Exporter);
    @EXPORT = qw();
    @EXPORT_OK = qw($current_dir $wine_dir $winapi_dir $winapi_check_dir);

    use vars qw($current_dir $wine_dir $winapi_dir $winapi_check_dir);

    my $tool = $0;
    $tool =~ s%^(?:.*?/)?([^/]+)$%$1%;

    if(defined($current_dir) && defined($wine_dir) &&
       defined($winapi_dir) && defined($winapi_check_dir))
    {
	# Nothing
    } elsif($0 =~ m%^(.*?)/?tools/([^/]+)/[^/]+$%) {
	my $dir = $2;

	if(defined($1) && $1 ne "")
	{
	    $wine_dir = $1;
	} else {
	    $wine_dir = ".";

	}

	require Cwd;
	my $cwd = Cwd::cwd();

	if($wine_dir =~ /^\./) {
	    $current_dir = ".";
	    my $pwd; chomp($pwd = `pwd`);
	    foreach my $n (1..((length($wine_dir) + 1) / 3)) {
		$pwd =~ s/\/([^\/]*)$//;
		$current_dir = "$1/$current_dir";
	    }
	    $current_dir =~ s%/\.$%%;
	} elsif($wine_dir eq $cwd) {
	    $wine_dir = ".";
	    $current_dir = ".";
	} elsif($cwd =~ m%^$wine_dir/(.*?)?$%) {
	    $current_dir = $1;
	    $wine_dir = ".";
	    foreach my $dir (split(m%/%, $current_dir)) {
		$wine_dir = "../$wine_dir";
	    }
	    $wine_dir =~ s%/\.$%%;
	} else {
	    print STDERR "$tool: You must run this tool in the main Wine directory or a sub directory\n";
	    exit 1;
	}

	$winapi_dir = "$wine_dir/tools/winapi";
	$winapi_dir =~ s%^\./%%;

	$winapi_check_dir = "$wine_dir/tools/winapi_check";
	$winapi_check_dir =~ s%^\./%%;

	push @INC, ($winapi_dir, $winapi_check_dir);
    } else {
	print STDERR "$tool: You must run this tool in the main Wine directory or a sub directory\n";
	exit 1;
    }
}

1;
