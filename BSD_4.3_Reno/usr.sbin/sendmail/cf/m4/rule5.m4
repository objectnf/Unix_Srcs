divert(10)
#
# Copyright (c) 1983 Eric P. Allman
# Copyright (c) 1988 The Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)rule5.m4	1.5 (Berkeley) 2/15/89
#
divert(0)
############################################################
############################################################
#####
#####		Provide Backward Compatibility
#####
############################################################
############################################################

#####################################################
#  General code to convert back to old style names  #
#####################################################
S5

R$+<@$w>		$1				strip host
R$+<@$-.UUCP>		$2!$1				u@host.UUCP => host!u
