#!/usr/bin/env perl

#          Copyright (C) 2002, EMBL-EBI, author Anton Enright.                #
#                                                                             #
# This file is part of MCL.  You can redistribute and/or modify MCL under the #
# terms of the GNU General Public License; either version 2 of the License or #
# (at your option) any later version.  You should have received a copy of the #
# GPL along with MCL, in the file COPYING.                                    #

#
# $Id: tribe-families,v 1.3 2002/04/25 19:30:31 flux Exp $
#
# Interprets an MCL cluster matrix and outputs protein families
#
# Anton Enright - EMBL-EBI 2002

open (FILE,$ARGV[0]) or die "Error: Cannot open file $ARGV[0]";
$i=-1;
while (<FILE>)
	{

	if ($started)
                {
                chop($_);
                $cluster[$i]=join(" ",$cluster[$i],"$_");
                }

	if (/^\d+/)
		{
		$started=1;
		$i++;
		chop($_);
		$cluster[$i]=join(" ",$cluster[$i],"$_");
		}

	if (/\$$/)
		{
		$started=0;
		}
	}

open (FILE,$ARGV[1]);
while (<FILE>)
	{
	/^(\S+)\s+(\S+)/;
	$hash{$1}=$2;
	}

for ($j=0;$j<$i+1;$j++)
	{
	@array=split(" ",$cluster[$j]);

	for ($p=1;$p<$#array;$p++)
		{
		print "$array[0]\t$hash{$array[$p]}\n";
		}

	}
