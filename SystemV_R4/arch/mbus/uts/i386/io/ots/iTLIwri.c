/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1988, 1989  Intel Corporation	*/
/*	All Rights Reserved	*/

/*	INTEL CORPORATION PROPRIETARY INFORMATION	*/

/*	This software is supplied to AT & T under the terms of a license   */ 
/*	agreement with Intel Corporation and may not be copied nor         */
/*	disclosed except in accordance with the terms of that agreement.   */	

#ident	"@(#)mbus:uts/i386/io/ots/iTLIwri.c	1.3"

/*
** ABSTRACT:
**
**	Put and Service routines for write queue side of SV-ots driver.
**
** NOTES regarding ep->tli_state:
**
**	This field is updated in this module:
**
**	1) after the request received from the user is verified
**	2) if there is an immediate error in processing the request
**	3) after processing T_unbind_req and T_optmgmt_req
*/

#include "sys/ots.h"

extern endpoint ots_endpoints[];
extern struct otscfg otscfg;

extern ushort ots_resetting;

/*
 * TLI state transition matrix, defined in the STREAMS system
 * module timod
 */
extern char ti_statetbl[TE_NOEVENTS][TS_NOSTATES];

extern ulong ots_stat[];

/*
 * ots debug level defined in ots.c
 */
extern int ots_debug;

/*
 * Put functions that process the TLI primitives generated by the
 * transport user.
 *
 * Each of these functions have the same three arguments:
 *
 *	(ep, mptr, tptr)
 *
 *		ep:	pointer to the endpoint data structure
 *		mptr:	pointer to the message to be processed, which
 *				will always be of type M_PROTO or
 *				M_PCPROTO or M_DATA
 *		tptr:	pointer to the TLI structure to be processed
 *
 * Each routine returns 0 or a TLI error code.
 *
 * If a TLI error code is returned, the state will be updated via 
 * TE_ERROR_ACK and an error_ack sent with the appropriate error code.
 *
 * On success, it is the responsibility of each function to:
 *   update the state of the endpoint, according to the state transition 
 *     table ti_statetbl[][].
 *   ensure that any messages which require it are ack'd appropriately
 *   enqueue any messages which need flow control
 */
int Tp_conn_req(), Tp_conn_res(), Tp_discon_req(), Tp_data_req(),
	Tp_exdata_req(), Tp_unitdata_req(),
	Tp_info_req(), Tp_bind_req(), Tp_unbind_req(),
	Tp_optmgmt_req(), Tp_ordrel_req();

/*
 * This function is indexed by PRIM_type field (T_xxx value), as given
 * in the sys/tihdr.h file.
 */
int (*ti_Tp_fcntbl[])() =
{
	Tp_conn_req, Tp_conn_res, Tp_discon_req, Tp_data_req,
	Tp_exdata_req, Tp_info_req, Tp_bind_req, Tp_unbind_req,
	Tp_unitdata_req, Tp_optmgmt_req, Tp_ordrel_req,
};

/*
 * Service functions that process the TLI primitives generated by the
 * transport user.
 *
 * Each of these functions have the same two arguments:
 *
 *	(ep, mptr)
 *
 *		ep:	pointer to the endpoint data structure
 *		mptr:	pointer to the message to be processed, which
 *				will always be of type M_PROTO or
 *				M_PCPROTO or M_DATA
 *
 * If the function returns a non-zero value, the service procedure quits
 * processing messages.  Non-zero returns indicate that an error was
 * generated, the connection has been broken or flow control conditions
 * prevent further processing.
 *
 * Regardless of the function return value, it is the responsibility of
 *	the function itself to update the state of the endpoint,
 *	according to the state transition table ti_statetbl[][], and
 *	send any required acknowledgments or errors upstream.
 */
bool Ts_data_req(), Ts_exdata_req(), Ts_unitdata_req(), Ts_ordrel_req(),
	Ts_unbind_req();

/*
 * This function is indexed by PRIM_type field (T_xxx value), as given
 * in the sys/tihdr.h file.
 */
bool (*ti_Ts_fcntbl[])() =
{
	NULL, NULL, NULL, Ts_data_req, Ts_exdata_req, NULL,
	NULL, Ts_unbind_req, Ts_unitdata_req, NULL, Ts_ordrel_req,
};

/* FUNCTION:		iTLIwput()
 *
 * ABSTRACT:	Streams write put procedure
 *
 *		TLI to OTS driver
 *
 *	This function:
 *
 *		1. Performs basic syntax checking, to weed out bad messages.
 *		2. Performs basic semantic checking, to weed out illegal
 *			messages.
 *		3. Carries out certain functions that are purely TLI or
 *			STREAMS related.
 */
iTLIwput(wr_q, mptr)
queue_t *wr_q;
register mblk_t *mptr;
{
	register endpoint *ep;

	DEBUGC('a');
	ep = (endpoint *)wr_q->q_ptr;
	switch(mptr->b_datap->db_type)
	{
	case M_FLUSH:
		/*
		 * Process these messages immediately.
		 */
		if (*mptr->b_rptr & FLUSHW)
		{
			flushq(wr_q, FLUSHDATA);
		}
		if (*mptr->b_rptr & FLUSHR)
		{
			flushq(RD(wr_q), FLUSHDATA);
			*mptr->b_rptr &= ~FLUSHW;
			qreply(wr_q, mptr);
		}
		else
			freemsg(mptr);
		break;
	case M_DATA:
	case M_PROTO:
	case M_PCPROTO:
		if (ep->str_state & C_ERROR)
		{
			/*
			 * A M_ERROR was previously issued for this
			 * stream. Ignore all messages.
			 */
			DEBUGP(DEB_ERROR,(CE_CONT, "iTLIwput: stream is M_ERROR'd\n"));
			freemsg(mptr);
			break;
		}
		switch(mptr->b_datap->db_type)
		{
		case M_DATA:
			/*
			 * There must be at least one byte of data in
			 * the message.
			 *
			 * If not, simply ignore the message.
			 */
			if (msgdsize(mptr) == 0)
				freemsg(mptr);
			else	/* Check (and possibly process) the message */
				iTLI_check_msg(ep, mptr, T_DATA_REQ);
			break;
		case M_PROTO:
		case M_PCPROTO:
			/*
			 * Check (and possibly process) the message.
			 */
			iTLI_check_msg(ep, mptr, (int)((union T_primitives *)mptr->b_rptr)->type);
			break;
		}
		break;
	case M_IOCTL:
		iTLI_ioctl_check(ep, mptr, wr_q);
		break;
	default:
		/*
		 * This is an unexpected STREAMS message:
		 * 	Throw it away.
		 */
		freemsg(mptr);
		break;
	}
}


/* FUNCTION:		iTLIwsrv()
 *
 * ABSTRACT:
 *				  (user)
 * Write service procedure:       ( vv )
 *				  (iTLI)
 *
 * Note that all messages read from this STREAM have already been
 * "filtered" through the the put procedure.  As a result, only four 
 * types of messages are ever seen here:
 *
 *	T_DATA_REQ	: User data, to be sent
 *	T_ORDREL_REQ	: Orderly release, to be sent
 *	T_UNBIND_REQ	: Unbind endpoint
 *	T_UNITDATA_REQ	: User datagrams, to be sent
 *
 * The lower level routines will return a non-zero value if servicing of
 * the queue should be halted (e.g. because of an error or flow control
 * condition).
 */
iTLIwsrv(wr_q)

queue_t *wr_q;
{
	mblk_t *mptr;
	endpoint *ep;
	int type;

	DEBUGC('b');
	DEBUGP(DEB_CALL,(CE_CONT, "iTLIwsrv()\n"));

	ep = (endpoint *)wr_q->q_ptr;
	while ((mptr = getq(wr_q)) != NULL)
	{
		if (mptr->b_datap->db_type == M_DATA)
			type = T_DATA_REQ;
		else
			type = ((union T_primitives *)mptr->b_rptr)->type;
		/*
		 * Process the message.
		 *
		 * This call can return in one of two ways:
		 *	1. Continue processing with the other
		 *		messages on this STREAM.
		 *	2. Stop all processing from the STREAM.
		 */
		if (ti_Ts_fcntbl[type])
		{
			if ((*ti_Ts_fcntbl[type])(ep, mptr))
				return;
			else
				DEBUGP(DEB_CALL,(CE_CONT, "iTLIwsrv => NULL\n"));
		}
		else
		{
			cmn_err(CE_PANIC,"SV-ots: bad wsrv jump reference\n");
		}
	}
}


/* FUNCTION:		iTLI_abort
 *
 * ABSTRACT:
 *
 *	Abort all connections associated with the endpoint.
 *	Do a flushing disconnect whether the user asked for it or not.
 *
 * INPUTS:	ptr to endpoint structure
 *
 * OUTPUTS:	streams messages flushed, VC's aborted
 *
 * RETURNS:	none
 *
 * CALLED FROM:	otsclose() and iTLI_pferr()
 */
void
iTLI_abort(ep)

endpoint *ep;
{
	DEBUGC('c');
	DEBUGP(DEB_CALL,(CE_CONT, "iTLI_abort()\n"));
	/*
	 *	1. Flush any output not yet processed.
	 *	2. Throw away any output being sent.
	 *	3. Make sure the write queue is available for
	 *		scheduling by STREAMS.
	 */
	flushq(WR(ep->rd_q), FLUSHALL);
	enableok(WR(ep->rd_q));
	qenable(WR(ep->rd_q));

	if (ep->tli_state >= TS_DATA_XFER)
		ots_stat[ST_CURC]--;
	/*
	 * Now, abort all VC's associated with the endpoint
	 */
	iMB2_abort(ep);
}


/* FUNCTION:		iTLI_check_msg()
 *
 * ABSTRACT:
 *
 *	This function is called from the wput function, to filter out
 *	bad messages, and dispatch to the appropriate put routine.
 */
void
iTLI_check_msg(ep, mptr, type)

endpoint *ep;
mblk_t *mptr;
int type;
{
	int event, next, error;

	DEBUGC('d');

	/*
	 * Step 1:  See if the message is legal in the current TLI state.
	 */
	switch(type)
	{
	case T_CONN_REQ:
		event = TE_CONN_REQ;		break;
	case T_CONN_RES:
		event = TE_CONN_RES;		break;
	case T_DISCON_REQ:
		event = TE_DISCON_REQ;		break;
	case T_DATA_REQ:
		event = TE_DATA_REQ;		break;
	case T_EXDATA_REQ:
		event = TE_EXDATA_REQ;		break;
	case T_INFO_REQ:
		Tp_info_req(ep, mptr);		return;
	case T_BIND_REQ:
		event = TE_BIND_REQ;		break;
	case T_UNBIND_REQ:
		event = TE_UNBIND_REQ;		break;
	case T_UNITDATA_REQ:
		event = TE_UNITDATA_REQ;	break;
	case T_OPTMGMT_REQ:
		event = TE_OPTMGMT_REQ;		break;
	case T_ORDREL_REQ:
		event = TE_ORDREL_REQ;		break;
	default:
		/*
		 * A message of unknown type.
		 */
		iTLI_pterr(ep, mptr, TSYSERR, EINVAL);
		return;
	}
	DEBUGP(DEB_FULL,(CE_CONT, "iTLI_check_msg(): state=%d, event=%d\n",
			ep->tli_state,event));
	if ((next = ti_statetbl[event][ep->tli_state]) == TS_INVALID)
	{
		if (  (event == TE_DATA_REQ)
		    ||(event == TE_EXDATA_REQ)
		    ||(event == TE_UNITDATA_REQ)
		   )
		{
			/*
			 * Out-of-state data transfer errors are either
			 * ignored or are fatal.
			 */
			DEBUGP(DEB_ERROR,(CE_CONT, "iTLI_check_msg: out-of-state M_DATA(%x, %x)\n",
					ep, mptr));
			DEBUGP(DEB_ERROR,(CE_CONT, " state=%x, event=%x\n",
					ep->tli_state, event));
			freemsg(mptr);
			if (ep->tli_state != TS_IDLE)
				iTLI_pferr(ep, EPROTO);
			return;
		}
		else
		{	/*
			 * We may have gotten (and passed to the user)
			 * a conn_ind, but he hasn't seen it yet (if he
			 * hasn't done a t_listen yet).  So optmgmt req's
			 * are really still legal
			 */
			if (  (event == TE_OPTMGMT_REQ)
			    &&(ep->tli_state == TS_WRES_CIND)
			   )
				next = TS_WRES_CIND;
			else
			{
				DEBUGP(DEB_ERROR,(CE_CONT, "iTLI_check_msg: out-of-state non-M_DATA(%x, %x)\n", ep, mptr));
				DEBUGP(DEB_ERROR,(CE_CONT, " state=%x, event=%x\n", ep->tli_state, event));
				switch(type)
				{
				case T_BIND_REQ:
				case T_UNBIND_REQ:
				case T_OPTMGMT_REQ:
					iTLI_pterr(ep, mptr, TOUTSTATE, 0);
					break;
				case T_CONN_REQ:
				case T_CONN_RES:
				case T_DISCON_REQ:
					iTLI_pterr(ep, mptr,
						   TOUTSTATE|HIGH_PRI, 0);
					break;
				}
				return;
			}
		}
	}
	ep->tli_state = next;
	/*
	 * Step 2: Call the appropriate put procedure
	 */
	if (error = (*ti_Tp_fcntbl[type])(ep, mptr, mptr->b_rptr))
	{
		ep->tli_state = ti_statetbl[TE_ERROR_ACK][ep->tli_state];
		switch(error&~HIGH_PRI)
		{
		case TSYSERR:
			iTLI_pterr(ep, mptr, error, ENOSR);
			break;
		case MB2ERR:
			iTLI_pterr(ep, mptr, error, EIO);
			break;
		default:
			iTLI_pterr(ep, mptr, error, 0);
			break;
		}
	}
}

/* FUNCTION:		iTLI_reset()
 *
 * ABSTRACT:	Reset all endpoints via iMB2_abort called from iTLI_pferr()
 *		This notifies users of existing endpoints.
 */
void
iTLI_reset()
{
	unsigned int i;

	ots_resetting = TRUE;		/* set to block opens */

	for (i = 1;  i < otscfg.n_endpoints; i++)
	{
		if (ots_endpoints[i].str_state != C_IDLE)
			iTLI_pferr(&ots_endpoints[i], EPROTO);
	}
	/*
	 * Clean up any remaining lower-level resources after giving
	 * abort time to complete (two seconds).  iMB2_reset will turn of
	 * ots_reset flag.
	 */
	timeout(iMB2_reset, NULL, 200);
}


/* FUNCTION:		Tp_bind_req()
 *
 * ABTRACT:	Transport User Bind Request: T_BIND_REQ
 *
 *	Input State: TS_UNBND --> TS_WACK_BREQ
 *
 *	Output State: TS_IDLE
 *
 * NOTES: Acknowledgement is build and sent in iTLIrd.c by
 *		iTLI_bind_req_complete()
 */
int
Tp_bind_req(ep, mptr, bind)

endpoint *ep;
mblk_t *mptr;
struct T_bind_req *bind;
{
	int	error;

	DEBUGC('e');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_bind_req()\n"));
	/*
	 * Check the value of "max connection count"
	 * A non_zero value indicates that the endpoint is permitted to 
	 * accept incoming connection requests.
	 */
	if (bind->CONIND_number > otscfg.max_pend)
		bind->CONIND_number = otscfg.max_pend;
	ep->max_pend = bind->CONIND_number;

	error = iMB2_bind_req(ep, mptr, bind);
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_bind_req => %d\n", error));
	return(error);
}


/* FUNCTION:		Tp_conn_req()
 *
 * PURPOSE:	Transport User Connection Request: T_CONN_REQ
 *
 *	This driver does NOT allow multiple VC's on a single endpoint.
 *	Therefore, connection requests on listening endpoints are rejected.
 *	Also, requests containing options and associated connection buffers
 *	are rejected.
 *
 *	Input State: TS_IDLE --> TS_WACK_CREQ
 *
 *	Output state: TS_WCON_CREQ
 */
int
Tp_conn_req(ep, mptr, conn)

endpoint *ep;
struct T_conn_req *conn;
mblk_t *mptr;
{
	int error;

	DEBUGC('f');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_req()\n"));
	if ((ep->options & OPT_COTS) == FALSE)
	{
		DEBUGP(DEB_ERROR,(CE_CONT, "Tp_conn_req: not a connection service\n"));
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_req => TNOTSUPPORT\n"));
		return(TNOTSUPPORT|HIGH_PRI);
	}
	else
	{
		if ((error = iMB2_conn_req(ep, mptr)) == 0)
		{
			DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_req => 0\n"));
			return(0);
		}
		else
			return(error|HIGH_PRI);	/* connect failed */
	}
}


/* FUNCTION:		Tp_conn_res()
 *
 * ABSTRACT:
 *
 *	Transport User Connection Response: T_CONN_RES
 *
 *	Input State: TS_WRES_CIND --> TS_WACK_CRES
 *
 *	Output State: TS_IDLE, TS_DATA_XFER, or TS_WRES_CIND
 */
int
Tp_conn_res(listen_ep, mptr, cresp)

endpoint *listen_ep;
mblk_t *mptr;
struct T_conn_res *cresp;
{
	endpoint *accept_ep;
	int error;

	DEBUGC('g');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res()\n"));
	/*
	 * Is the STREAM on which the connection is being accepted valid?
	 */
	if (cresp->QUEUE_ptr != listen_ep->rd_q)
	{
		/*
		 * The accepting STREAM is not the one which did
		 * the listening.
		 */
		DEBUGP(DEB_FULL,(CE_CONT, "Tp_conn_res: different streams\n"));
		for (accept_ep = &ots_endpoints[0];
		     accept_ep <= &ots_endpoints[otscfg.n_vcs];
		     accept_ep++)
		{
			if (  (cresp->QUEUE_ptr == accept_ep->rd_q)
			    &&((accept_ep->str_state & C_ERROR) == FALSE)
			   )
			{
				if (ti_statetbl[TE_PASS_CONN][accept_ep->tli_state]
						!= TS_INVALID)
					break;
				else
				{
					DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res => TOUTSTATE\n"));
					return(TOUTSTATE|HIGH_PRI);
				}
			}
		}
		if (accept_ep >= &ots_endpoints[otscfg.n_endpoints])
		{
			DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res => TBADF\n"));
			return(TBADF|HIGH_PRI);
		}
		else if (accept_ep->nbr_pend > 1)
		{
			DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res: connect on listener => TBADF\n"));
			return(TBADF|HIGH_PRI);
		}
	}
	else	/* the accepting and listening STREAM are identical */
	{
		DEBUGP(DEB_FULL,(CE_CONT, "Tp_conn_res: accept on listening stream\n"));
		if (listen_ep->nbr_pend > 1)
		{
			DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res: connect on listener => TBADF\n"));
			return(TBADF|HIGH_PRI);
		}
		else
			accept_ep = listen_ep;
	}
	/*
	 * Notify the remote endpoint of the acceptance 
	 */
	if (error = iMB2_conn_res((int)cresp->SEQ_number,
		listen_ep, accept_ep, mptr))
	{
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res => %x\n", error));
		return(error|HIGH_PRI);
	}
	else
	{
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_conn_res => 0\n"));
		return(0);
	}
}


/* FUNCTION:		Tp_data_req()
 *
 * ABSTRACT:	Transport User Data Request: T_DATA_REQ
 *
 *	Input State: TS_DATA_XFER   --> TS_DATA_XFER
 *		     TS_WREQ_ORDREL --> TS_WREQ_ORDREL
 *
 *	Output State: no change
 */
int
Tp_data_req(ep, mptr, data)

endpoint *ep;
mblk_t *mptr;
struct T_data_req *data;
{
	mblk_t *body;
	int data_length;
	int more;

	DEBUGC('h');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_data_req():ep=%x,mptr=%x,data=%x\n",ep,mptr,data));

	/* ensure send doesn't exceed TSDU size */

	if (mptr->b_datap->db_type == M_DATA)
	{
		body = mptr;
		more = FALSE;
	}
	else
	{
		body = mptr->b_cont;
		if (otscfg.tsdu_size)
			more = data->MORE_flag;
		else
			more = FALSE;
	}

	data_length = msgdsize(body);
	if (  (otscfg.tsdu_size)
	    &&(  ((ep->tsdu_sbytes += data_length) > otscfg.tsdu_size)
		 ||(  (ep->tsdu_sbytes == otscfg.tsdu_size)
		    &&(more)
		   )
	      )
	   )
	{
		DEBUGP(DEB_ERROR,(CE_CONT, "Tp_data_req: ep=%x TSDU count exceeded\n",ep));
		freemsg(mptr);
		iTLI_pferr(ep, EPROTO);
		return(0);
	}
	else if (more == FALSE)
		ep->tsdu_sbytes = 0;

	DEBUGP(DEB_FULL,(CE_CONT, "Tp_data_req: ep->tsdu_sbytes = %x\n", ep->tsdu_sbytes));
	/*
	 * Now, just pass through flow control and out
	 */
	putq(WR(ep->rd_q), mptr);
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_data_req => 0\n"));
	return(0);
}


/* FUNCTION:			Tp_discon_req()
 *
 * ABSTRACT:	Transport User Disconnect Request: T_DISCON_REQ
 *
 *	Input State: TS_WCON_CREQ   --> TS_WACK_DREQ6
 *		     TS_WRES_CIND   --> TS_WACK_DREQ7
 *		     TS_DATA_XFER   --> TS_WACK_DREQ9
 *
 *		     TS_WIND_ORDREL --> TS_WACK_DREQ10
 *		     TS_WREQ_ORDREL --> TS_WACK_DREQ11
 *
 *	Output State: TS_IDLE or
 *		       back to original input state on error
 */
int
Tp_discon_req(ep, mptr, dis)

endpoint *ep;
mblk_t *mptr;
struct T_discon_req *dis;
{
	int error;

	DEBUGC('i');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_discon_req()\n"));

	dis = (struct T_discon_req *)mptr->b_rptr;

	switch(ep->tli_state)
	{
	case TS_WACK_DREQ10:
	case TS_WACK_DREQ11:
	case TS_WACK_DREQ9:
		/*
		 * The endpoint has one (and only one) active connection,
		 * take down the connection.  Flush all output and input
		 * not yet processed.
		 */
		iTLI_send_flush(ep);
		if (error = iMB2_discon_req(ep, 0, mptr))
			return(error);
		break;
	case TS_WACK_DREQ7:
		/*
		 * The endpoint is listening, drop a pending connection request.
		 */
		if (error = iMB2_discon_req(ep, (int)dis->SEQ_number, mptr))
			return(error);
		break;
	default: /* WACK_DREQ6 */
		/*
		 * The endpoint is a client who got tired of waiting for
		 * the connect confirm.
		 */
		if (error = iMB2_discon_req(ep, 0, mptr))
			return(error);
		break;
	}
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_discon_req => 0\n"));

	/* acknowledgment done in iTLI_discon_req_complete() */

	return(0);
}


/* FUNCTION:		Tp_exdata_req()
 *
 * ABSTRACT:	Transport Expedited Data Request: T_EXDATA_REQ
 *
 *	Verify that user hasn't exceeded ETSDU with this send.  If so,
 *	abort the connection; otherwise, queue the request.
 *	We queue these messages ahead of any normal data messages already
 *	queued.
 *
 *	Input State: TS_DATA_XFER   --> TS_DATA_XFER
 *		     TS_WREQ_ORDREL --> TS_WREQ_ORDREL
 *
 *	Output State: no change
 *
 * NOTES:
 *
 *   1) This implementation assumes data portion of message is referenced by
 *	one and only one mblock.
 */
int
Tp_exdata_req(ep, mptr, exdata)

endpoint *ep;
mblk_t *mptr;
struct T_exdata_req *exdata;
{
	mblk_t *tmp;		/* used to parse queue messages list */
	int data_length;
	int more;

	DEBUGC('j');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_exdata_req:ep=%x,mptr=%x,exdata=%x\n",ep,mptr,exdata));

	if ((ep->options & OPT_EXP) == FALSE)
	{
		DEBUGP(DEB_ERROR,(CE_CONT, "Tp_exdata_req: no expedited service\n"));
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_exdata_req => TNOTSUPPORT\n"));
		return(TNOTSUPPORT|HIGH_PRI);
	}
	else if (  (mptr->b_cont->b_cont)		/* Note 1 */
		 ||((data_length = msgdsize(mptr->b_cont)) == 0)
		)
		return(TNOTSUPPORT|HIGH_PRI);

	if (otscfg.etsdu_size)
		more = exdata->MORE_flag;
	else
		exdata->MORE_flag = FALSE;


	if (  (otscfg.etsdu_size)
	    &&(  ((ep->etsdu_sbytes += data_length) > (int)otscfg.etsdu_size)
	       ||(  (more)
		   &&(ep->etsdu_sbytes == otscfg.etsdu_size)
		 )
	      )
	   )
	{
		freemsg(mptr);
		iTLI_pferr(ep, EPROTO);
		return(0);
	}
	else if (more == FALSE)
		ep->etsdu_sbytes = 0;

	/* queue message ahead of any normal data messages */

	tmp = WR(ep->rd_q)->q_first;
	while (tmp != NULL)
	{
		if (  (tmp->b_datap->db_type == M_DATA)
		    ||(((union T_primitives *)tmp->b_rptr)->type == T_DATA_REQ)
		   )
			break;
		tmp = tmp->b_next;
	}
	insq(WR(ep->rd_q), tmp, mptr);
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_exdata_req => 0\n"));
	return(0);
}


/* FUNCTION:		Tp_info_req()
 *
 * ABSTRACT:	Transport User Information Request: T_INFO_REQ
 *
 *	Input State: Any
 *
 *	Output State: no change
 */
int 
Tp_info_req(ep, mptr)

endpoint *ep;
mblk_t *mptr;
{
	long	serv_type;

	DEBUGC('k');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_info_req()\n"));
	if (ep->options & OPT_CLTS)
	{
		serv_type = T_CLTS;
		iTLI_info_ack(ep, mptr, (long)otscfg.datagram_size, (long)-2,
			(long)-2, (long)-2,
			(long)otscfg.addr_size, (long)otscfg.opts_size,
			(long)TIDU_SIZE, (long)ep->tli_state,
			(long)T_CLTS);
	}
	else
	{
		if (ep->options & OPT_ORD)
			serv_type = T_COTS_ORD;
		else
			serv_type = T_COTS;
		iTLI_info_ack(ep, mptr,
			(long)otscfg.tsdu_size, (long)otscfg.etsdu_size,
			(long)otscfg.cdata_size, (long)otscfg.ddata_size,
			(long)otscfg.addr_size, (long)otscfg.opts_size,
			(long)TIDU_SIZE, (long)ep->tli_state,
			(long)serv_type);
	}
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_info_req => NULL\n"));
}


/* FUNCTION:		Tp_optmgmt_req()
 *
 * ABSTRACT:	 Transport User Options Management Request: T_OPTMGMT_REQ
 *
 *	Input State: TS_IDLE --> TS_WACK_OPTREQ
 *
 *	Output State: TS_IDLE
 *
 *	Endpoint options are also modified at connection establishment
 *	time.  We don't allow the user to modify options when an endpoint
 *	is in the data transfer state.
 */
int
Tp_optmgmt_req(ep, mptr, opt_req)

endpoint *ep;
mblk_t *mptr;
struct T_optmgmt_req *opt_req;
{
	int flags;		/* request result returned to user */
	opts *options;		/* user specified options */
	opts default_options;	/* default initial endpoint options */
	opts driver_options;	/* all options the driver supports */

	DEBUGC('l');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_optmgmt_req()\n"));
	switch (flags = opt_req->MGMT_flags)
	{
	case T_DEFAULT:
	case T_CHECK:
	case T_NEGOTIATE:
		break;
	default:
		return(TBADFLAG);
	}
	
	if (ep <= &ots_endpoints[otscfg.n_vcs])
	{
		default_options = otscfg.vc_defaults;
		driver_options = U_OPTIONS;
		driver_options &= ~OPT_CLTS;
	}
	else
	{
		default_options = otscfg.dg_defaults;
		driver_options = U_OPTIONS;
		driver_options &= ~(OPT_COTS | OPT_ORD | OPT_EXP);
	}

	if (opt_req->OPT_length == 0)
	{
		/*
		 * The user did not provide any options.
		 */
		DEBUGP(DEB_FULL,(CE_CONT, "Tp_optmgmt_req: No user options given\n"));
		if (flags & T_NEGOTIATE)
			return(TBADOPT);
		else if (flags == T_CHECK)
		{
			/*
			 * No options to check: Automatic success.
			 */
			iTLI_optmgmt_ack(ep, mptr, (flags | T_SUCCESS), 0, 0);
		}
		else
			iTLI_optmgmt_ack(ep, mptr, flags, COPT_LEN, default_options);

		DEBUGP(DEB_CALL,(CE_CONT, "Tp_optmgmt_req => 0\n"));
		return(0);
	}
	else if (opt_req->OPT_length != COPT_LEN)
		return(TBADOPT);
	else
	{
		options = (opts *)(mptr->b_rptr + opt_req->OPT_offset);
		DEBUGP(DEB_FULL,(CE_CONT, "Tp_optmgmt_req: User input %x\n", *options));
		switch (opt_req->MGMT_flags)
		{
		case T_DEFAULT:
			/*
			 * Return the default options.
			 */
			*options = default_options;
			break;
		case T_CHECK:
			/*
			 * See if any unknown or illegal options were specified.
			 */
			if (*options & ~driver_options)
				flags |= T_FAILURE;
			else
				flags |= T_SUCCESS;
			break;
		case T_NEGOTIATE:
			/*
			 * If user is changing endpoint options during the
			 * TS_DATA_XFER state, return an error.
			 * Otherwise, see if any unknown or illegal options
			 * were specified. Select a good set, and install them.
			 */
			if (  (ep->tli_state == TS_DATA_XFER)
			    &&(ep->options != *options)
			   )
				return(TBADOPT);
			else if (*options & ~driver_options)
				return(TBADOPT);
			else
				ep->options = *options;
			DEBUGP(DEB_FULL,(CE_CONT, "Tp_optmgmt_req: Replying with %x\n", *options));
			break;
		}
		iTLI_optmgmt_ack(ep, mptr, flags, COPT_LEN, *options);
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_optmgmt_req => 0\n"));
		return(0);
	}
}


/* FUNCTION:		Tp_ordrel_req()
 *
 * ABSTRACT:
 *
 *	Verify that orderly release is supported on this endpoint; then
 *	queue request for service procedure thereby ensuring that all
 *	send data requests are handled first.
 */
int
Tp_ordrel_req(ep, mptr, body)

endpoint *ep;
mblk_t *mptr;
union T_primitives *body;
{
	DEBUGC('o');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_ordrel_req()\n"));

	if ((ep->options & OPT_ORD) == FALSE)
	{
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_ordrel_req => TNOTSUPPORT\n"));
		return(TNOTSUPPORT);
	}
	putq(WR(ep->rd_q), mptr);
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_ordrel_req => 0\n"));
	return(0);
}


/* FUNCTION:		Tp_unbind_req()
 *
 * ABSTRACT:	Transport User Unbind Request: T_UNBIND_REQ
 *
 *	Input State: TS_IDLE --> TS_WACK_UREQ
 *
 *	Output State: TS_UNBND;
 */
int
Tp_unbind_req(ep, mptr, unbind)

endpoint *ep;
mblk_t *mptr;
struct T_unbind_req *unbind;
{
	DEBUGC('m');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_unbind_req()\n"));

	/*
	 * Just pass through flow control and out
	 */
	putq(WR(ep->rd_q), mptr);
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_unbind_req => 0\n"));
	return(0);
}


/* FUNCTION:		Tp_unitdata_req()
 *
 * ABSTRACT:	Transport Unit Data Request: T_UNITDATA_REQ
 *
 *	Input State: TS_IDLE --> TS_IDLE
 *
 *	Output State: no change
 */
int
Tp_unitdata_req(ep, mptr, data)

endpoint *ep;
mblk_t *mptr;
struct T_unitdata_req *data;
{
	DEBUGC('n');
	DEBUGP(DEB_CALL,(CE_CONT, "Tp_unitdata_req()\n"));
	if ((ep->options & OPT_CLTS) == FALSE)
		return(TNOTSUPPORT);
	else if  (msgdsize(mptr->b_cont) > (int)otscfg.datagram_size)
	{
		freemsg(mptr);
		iTLI_pferr(ep, EPROTO);
		return(0);
	}
	else
	{
		/*
		 * Just pass through flow control and out
		 */
		putq(WR(ep->rd_q), mptr);
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_unitdata_req => 0\n"));
		return(0);
	}
}


/* FUNCTION:		Ts_data_req()
 *
 * ABSTRACT:	Service Routine for Transport Data Request: T_DATA_REQ
 */
bool
Ts_data_req(ep, mptr)

endpoint *ep;			/* sending endpoint */
mblk_t *mptr;			/* T_DATA_REQ message to send */
{
	mblk_t *body;		/* data portion of message */
	mblk_t *nextp;		/* next mblock in data portion */
	int data_length;	/* length of current mblock */
	int error;		/* error in data send */
	int more;		/* MORE flag */

	DEBUGC('p');
	DEBUGP(DEB_CALL,(CE_CONT, "Ts_data_req()\n"));

	if (mptr->b_datap->db_type == M_DATA)
	{
		DEBUGP(DEB_FULL,(CE_CONT, "  db_type = M_DATA\n"));
		body = mptr;
		mptr = NULL;
		more = FALSE;
	}
	else	/* use more flag in the M_PROTO message */
	{
		more = (((struct T_data_req *)mptr->b_rptr)->MORE_flag != 0);
		body = unlinkb(mptr);
	}
	DEBUGP(DEB_FULL,(CE_CONT, "  len=%d, data=%x, more=%d\n",
		body->b_wptr-body->b_rptr,*(long *)body->b_rptr, more));

	ep->nbr_datarq++;
	while (body)
	{
		data_length = msgdsize(body);
		nextp = unlinkb(body);

		if (error = iMB2_data_req(ep, body, ((nextp) ? TRUE : more)))
		{
			if (mptr)
				linkb(mptr, body);
			else
				mptr = body;
			linkb(mptr, nextp);

			if (error != M_FLOW_CONTROL)
			{
				ep->nbr_datarq--;
				DEBUGP(DEB_ERROR,(CE_CONT, "Ts_data_req => %d\n",error));
				iTLI_pferr(ep, error);
				freemsg(mptr);
			}
			else
			{
				iTLI_data_req_complete(ep, mptr,M_FLOW_CONTROL);
			}
			DEBUGP(DEB_CALL,(CE_CONT, "Ts_data_req => 1\n"));
			return(1);
		}
		else
		{
			ots_stat[ST_BSNT] += data_length;
			ots_stat[ST_SPCK]++;
		}
		body = nextp;
	}
	if (!more)
		ots_stat[ST_SMSG]++;
	if (mptr)
		freemsg(mptr);

	DEBUGP(DEB_CALL,(CE_CONT, "Ts_data_req => 0\n"));
	return(0);
}


/* FUNCTION:			Ts_exdata_req()
 *
 * ABSTRACT:	Service Routine for Expedited Data Request: T_EXDATA_REQ
 */
bool
Ts_exdata_req(ep, mptr)

endpoint *ep;			/* sending endpoint */
mblk_t *mptr;			/* T_EXDATA_REQ message to send */
{
	mblk_t *body;
	int data_length;	/* size of data in message */
	int error;		/* error in data send */
	int more;		/* MORE flag */

	/*
	 * Read the more flag from the M_PROTO message block in front
	 * of the user data.  We get rid of the block down below.
	 */
	body = unlinkb(mptr);
	data_length = msgdsize(body);
	more = (((struct T_exdata_req *)mptr->b_rptr)->MORE_flag != 0);

	ep->nbr_datarq++;

	if (error = iMB2_exdata_req(ep, body, more))
	{
		linkb(mptr, body);

		if (error == M_FLOW_CONTROL)
			iTLI_exdata_req_complete(ep, mptr, M_FLOW_CONTROL);
		else
		{
			ep->nbr_datarq--;
			freemsg(mptr);
			iTLI_pferr(ep, EPROTO);		/* fatal */
		}
		DEBUGP(DEB_CALL,(CE_CONT, "Ts_exdata_req => 1\n"));
		return(1);
	}
	else
	{
		ots_stat[ST_ESNT] += data_length;
		ots_stat[ST_EPCK]++;
		if (!more)
			ots_stat[ST_SEXP]++;
		freemsg(mptr);
		DEBUGP(DEB_CALL,(CE_CONT, "Tp_exdata_req => 0\n"));
		return(0);
	}
}


/* FUNCTION:		Ts_ordrel_req()
 *
 * Transport Orderly Release Request: T_ORDREL_REQ
 */
bool
Ts_ordrel_req(ep, mptr)

endpoint *ep;
mblk_t *mptr;
{
	int error;

	DEBUGC('q');
	DEBUGP(DEB_CALL,(CE_CONT, "Ts_ordrel_req()\n"));

	if (error = iMB2_ordrel_req(ep, mptr))
	{
		DEBUGP(DEB_ERROR,(CE_CONT, "Ts_ordrel_req() => %d\n", error));
		iTLI_pferr(ep, error);
		return(1);
	}
	else
		return(0);
}

/* FUNCTION:		Ts_unbind_req()
 *
 * ABSTRACT:	Transport User Unbind Request: T_UNBIND_REQ
 *
 *	Input State: TS_IDLE --> TS_WACK_UREQ
 *
 *	Output State: TS_UNBND;
 */
bool
Ts_unbind_req(ep, mptr)

endpoint *ep;
mblk_t *mptr;
{
	int	error;

	DEBUGC('m');
	DEBUGP(DEB_CALL,(CE_CONT, "Ts_unbind_req()\n"));
	/*
	 * Generate a M_FLUSH.
	 */
	iTLI_send_flush(ep);
	/*
	 * Tell the lower layer we're gone
	 */
	if (error = iMB2_unbind_req(ep, mptr))
	{
		iTLI_pferr(ep, error);
		return(1);
	}
	else
	{
		DEBUGP(DEB_CALL,(CE_CONT, "Ts_unbind_req => 0\n"));
		return(0);
	}
}


/* FUNCTION:		Ts_unitdata_req()
 *
 * Transport Unit Data Request: T_UNITDATA_REQ
 *
 * NOTE: What about T_UNIT_DATA_REQ options fields?  Currently OTS defines no
 *		provision for these.
 */
bool
Ts_unitdata_req(ep, mptr)

endpoint *ep;
mblk_t *mptr;
{
	int data_length;
	int error;
	mblk_t *data;
	mblk_t *nextp;
	struct T_unitdata_req *udata;

	DEBUGC('q');
	DEBUGP(DEB_CALL,(CE_CONT, "Ts_unitdata_req()\n"));

	if (  (ep->tli_state != TS_IDLE)
	    &&(ep->tli_state != TS_WACK_UREQ)	/* unbind queued behind */
	   )
	{
		freemsg(mptr);
		return(0);
	}

	udata = (struct T_unitdata_req *)mptr->b_rptr;
	data = unlinkb(mptr);
	while (data != NULL)
	{
		nextp = unlinkb(data);
		data_length = data->b_wptr - data->b_rptr;
		if (data_length == 0)
		{
			continue;
		}

		ep->nbr_datarq++;

		if (error = iMB2_unitdata_req(ep, data, udata))
		{
			linkb(mptr, data);
			linkb(mptr, nextp);
			if (error != M_FLOW_CONTROL)
			{
				DEBUGP(DEB_CALL,(CE_CONT, "Ts_unitdata_req => %d\n",error));
				freemsg(mptr);
				ep->nbr_datarq--;
				iTLI_uderror_ind(ep, error,
					(int)udata->DEST_length,
					(char *)udata+udata->DEST_offset,
					(int)udata->OPT_length,
					(char *)udata+udata->OPT_offset);
			}
			else
			{
				iTLI_unitdata_req_complete(ep, mptr,
					M_FLOW_CONTROL);
			}
			DEBUGP(DEB_CALL,(CE_CONT, "Ts_unitdata_req => 1\n"));
			return(1);
		}
		else
		{
			ots_stat[ST_USNT] += data_length;
			ots_stat[ST_UPCK]++;
		}
		data = nextp;
	}
	DEBUGP(DEB_CALL,(CE_CONT, "Ts_unitdata_req => 0\n"));
	ots_stat[ST_SUNI]++;
	freemsg(mptr);
	return(0);
}
