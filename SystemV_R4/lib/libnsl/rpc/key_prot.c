/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:key_prot.c	1.1.2.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
*          All rights reserved.
*/ 
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)key_prot.c 1.1 89/03/08 Copyr 1986 Sun Micro";
#endif

#include <rpc/rpc.h>
#include <rpc/key_prot.h>

/* key_prot.x  */

/* 
 * Compiled from key_prot.x using rpcgen.
 * DO NOT EDIT THIS FILE!
 * This is NOT source code!
 */


bool_t
xdr_keystatus(xdrs, objp)
	XDR *xdrs;
	keystatus *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

#ifndef KERNEL
bool_t
xdr_keybuf(xdrs, objp)
	XDR *xdrs;
	keybuf objp;
{
	if (!xdr_opaque(xdrs, objp, HEXKEYBYTES)) {
		return (FALSE);
	}
	return (TRUE);
}
#endif

bool_t
xdr_netnamestr(xdrs, objp)
	XDR *xdrs;
	netnamestr *objp;
{
	if (!xdr_string(xdrs, objp, MAXNETNAMELEN)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_cryptkeyarg(xdrs, objp)
	XDR *xdrs;
	cryptkeyarg *objp;
{
	if (!xdr_netnamestr(xdrs, &objp->remotename)) {
		return (FALSE);
	}
	if (!xdr_des_block(xdrs, &objp->deskey)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_cryptkeyres(xdrs, objp)
	XDR *xdrs;
	cryptkeyres *objp;
{
	if (!xdr_keystatus(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case KEY_SUCCESS:
		if (!xdr_des_block(xdrs, &objp->cryptkeyres_u.deskey)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}

bool_t
xdr_unixcred(xdrs, objp)
	XDR *xdrs;
	unixcred *objp;
{
	if (!xdr_int(xdrs, (int *)&objp->uid)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, (int *)&objp->gid)) {
		return (FALSE);
	}
	if (!xdr_array(xdrs, (char **)&objp->gids.gids_val, (u_int *)&objp->gids.gids_len, MAXGIDS, sizeof(int), xdr_int)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_getcredres(xdrs, objp)
	XDR *xdrs;
	getcredres *objp;
{
	if (!xdr_keystatus(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case KEY_SUCCESS:
		if (!xdr_unixcred(xdrs, &objp->getcredres_u.cred)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}
