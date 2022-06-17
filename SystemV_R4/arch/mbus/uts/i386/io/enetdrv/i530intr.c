/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1985, 1986, 1987, 1989  Intel Corporation	*/
/*	All Rights Reserved	*/

/*	INTEL CORPORATION PROPRIETARY INFORMATION	*/

/*	This software is supplied to AT & T under the terms of a license   */ 
/*	agreement with Intel Corporation and may not be copied nor         */
/*	disclosed except in accordance with the terms of that agreement.   */	

#ident	"@(#)mbus:uts/i386/io/enetdrv/i530intr.c	1.3.2.1"

#ident "@(#)i530intr.c  $SV_enet SV-Eval01 - 06/25/90$"

/* MODULE:		i530intr.c
 *
 * PURPOSE:	Processes interrupts caused by incoming messages
 *           received by LCI Server.
 *
 * MODIFICATIONS:
 *	I000	6/30/88		rjs
 *		The i410lckld program, used by the ina961 script to download
 *	iNA, expects an ICS_INIT_DONE flag to appear in a controller
 *	interconnect space register when the controller is finally initialized.
 *	Because neither iNA nor the 186/530 firmware set this flag, we do it
 *	in the driver when notified by iNA that it has initialized.
 *
 *	I001	8/28/88		rjs		Intel
 *		Restored RB ptr from inside RB.  Necessary because phystokv()
 *		doesn't	restore initial virtual address.
 *	I002	9/28/88		rjs		Intel
 *		Default ina on 186/530 is R3.0.
 *	I003	2/07/88		rjs		Intel
 *		Maintain PRESENT bit in state flags; also ensure we free
 *		the transaction associated with the init request when the
 *		acknowledgment finally arrives from iNA.  These changes
 *		are necessary so we can reset the board and download iNA
 *		again.
 *	I004	07/06/89	rjf		Intel
 *		Added EDL support.
 *	I005	07/11/89	rjf		Intel
 *		Made lint fixes.
 *	I006	10/16/89	rjf		Intel
 *		Added support for MIX board.
 */

#define DEBUG 1

#define ICS_INIT_DONE	0x81		/* I000 */
#define ICSREG		0x44		/* I000 */

#include "sys/enet.h"
#include "sys/lcidef.h"
#include <sys/immu.h>

/*
 * The following are major variables defined in the space.c file for 186/530
 *
 * (enet statistics structure defined in enet.c)
 */

extern struct enetboard	enet_boards[];
extern char		ti_statetbl[TE_NOEVENTS][TS_NOSTATES];
extern endpoint		endpoints[];
extern int		enet_n_boards;
extern int		enet_nvc;
extern ulong		enet_stat[enet_SCNT];
extern int		mps_max_tran;			/* I003 */
extern unsigned char	ics_myslotid();

#ifdef EDL
extern void (*edl_conn_complete)();
extern void (*edl_disc_complete)();
extern void (*edl_rawtran_complete)();
extern void (*edl_rawrecv_complete)();
#endif

int	enet_z;

ulong lci_xmit_chan;		/* mpc channel from where data is xmitted */
ulong lci_rcv_chan;		/* mpc channel where data is received */
ulong lci_rb_chan;		/* mpc channel where rb is xmitted and rcvd */

int g_lci_error;		/* global variable for lci error codes */
ulong init_tid;

lci_rxadr_str lci_rxadr[6];     /* unoptimized version. */

uchar name_idx;

struct dma_buf *lci_rcv_datbuf_p;

extern struct enetinf enet_inform[];

void enet_bind_req_complete();
void enet_do_bind_req_complete();
void enet_discon_ind_complete();
void enet_discon_req_complete();
void enet_conn_ind_complete();
void enet_conn_req_continue();
void enet_data_ok();
void enet_expedited_data_ok();
void enet_data_ind_complete();
void enet_expedited_data_ind_complete();

void enet_data_req_complete();
void enet_close_req_complete();
void enet_expedited_data_req_complete();
void enet_send_datagram_complete();
void enet_datagram_ind();
void enet_withdraw_datagram_complete();
void enet_expedited_data_ind();

unchar ics_read();
dword Xlate_name();

unchar de_register();

/*
 * enet debug level defined in enet.c
 */
extern int enet_debug;

/*
 * enet_conn_req
 *
 * Just pass the request to the board
 */
/*
* enetintr
*     enet interrupt handler.
*
* Inputs:
*     level:  interrupt level
*
* This routines is invoked as a result of an interrupt
* generated by the enet.
*
* Assumes interrupt context is sufficient mutex on MIP Q's.
*/
void
enet_rcv_rb(mbuf_p)
mps_msgbuf_t *mbuf_p;
{
   int		i;
   ushort     s_id;      /* slot id of the 186/530 */
   struct     enetboard *board_p;
   ushort j;

   DEBUGP(DEB_CALL,(CE_CONT, "enetintr entered\n"));
/*   DEBUGP(DEB_CALL,(CE_CONT, "Incoming mbuf_p=%x, mb_data pointer=%x\n",(char *)mbuf_p,(char *)mbuf_p->mb_data)); */
   /* decide if the incoming unsol message is a buffer request or
      a solicited input completion interrupt */
   DEBUGP(DEB_CALL,(CE_CONT, "Incoming LCI opcode=%x\n",((rb_buf_req_str *)(mbuf_p->mb_data))->op_code));

   /* monitor(); */

   if ((mbuf_p->mb_flags & MPS_MG_DONE) == MPS_MG_DONE)
      {       /*message is input completion */
       if ((mbuf_p->mb_data[MPS_MG_MT] == MPS_MG_BREQ) &&
       ((((rb_buf_req_str *)(mbuf_p->mb_data))->op_code) == LCI_RBSEND_OPCODE))
      {  
	  DEBUGP(DEB_CALL,(CE_CONT, "enetintr: rb send completion msg.\n"));
	  /* monitor(); */
      /* completion of a request block sent to LCI */

          freeb((mblk_t *)(mbuf_p->mb_bind));
          DEBUGP(DEB_CALL,(CE_CONT, "enetintr: rb sent to LCI: completion rcvd\n"));
          mps_free_msgbuf(mbuf_p);
          return;
      }
      else if ((((unsol_msg_str *)(mbuf_p->mb_data))->op_code) == LCI_INIT_OPCODE)
         {   /* message is an init ack unsol message */
         /* get slot_id of the enet board */
         s_id = mbuf_p->mb_data[1];
         s_id = s_id & 0x1F;
         for (i=0; i < enet_n_boards; i++)
            {
            if (s_id == enet_boards[i].slot_id)
               {
               board_p = &enet_boards[i];
               board_p->state = (INIT_DONE | BOOTED | PRESENT);	/* I003 */
               board_p->eaddr[0] = mbuf_p->mb_data[LCI_INIT_EID_INDEX];
               board_p->eaddr[1] = mbuf_p->mb_data[LCI_INIT_EID_INDEX+1];
               board_p->eaddr[2] = mbuf_p->mb_data[LCI_INIT_EID_INDEX+2];
               board_p->eaddr[3] = mbuf_p->mb_data[LCI_INIT_EID_INDEX+3];
               board_p->eaddr[4] = mbuf_p->mb_data[LCI_INIT_EID_INDEX+4];
               board_p->eaddr[5] = mbuf_p->mb_data[LCI_INIT_EID_INDEX+5];
		for (j=0; j< 6; j++)
			enet_inform[i].eaddr[j] = (unchar)board_p->eaddr[j];
		enet_inform[i].base = 0xffffffff;
		enet_inform[i].port = 0xffffffff;
		enet_inform[i].slot = s_id;
		enet_inform[i].numvc = enet_nvc;
		enet_inform[i].inav = 31;
		for (j=02; j <= 0xb; j++)
			enet_inform[i].board_type[j-2] = (unchar)ics_read(s_id, j);
		enet_inform[i].board_type[10] = 0;
		DEBUGP(DEB_FULL,(CE_CONT, "enetintr 10: Init ack processed. board_p=%x\n",board_p));
		ics_write(s_id, ICSREG, ICS_INIT_DONE);		/* I000 */
		cmn_err(CE_CONT, "iNA960 started on %s in slot %d\n",
				enet_inform[i].board_type, s_id); /* I006 */
		break;
               }
            }
         mps_free_msgbuf(mbuf_p);
         if (mps_free_tid((long)lci_rb_chan,(unchar)init_tid) != -1)   /* I003 */
	 	init_tid = mps_max_tran;
         }
      else cmn_err(CE_WARN, "LCI Error: unknown MPC interrupt\n");
   }
   else if ((mbuf_p->mb_flags & MPS_MG_TERR) == MPS_MG_TERR)
      {      /* error message */
      /* log error in a global variable */ 
      g_lci_error = E_MPC_MG_TERR;
      DEBUGP(DEB_ERROR,(CE_CONT, "enetintr 8: MPS_MG_TERR in interrupt from MPC\n"));
      cmn_err(CE_WARN, "LCI Error: Transmission Error in RB channel\n");
      mps_free_msgbuf(mbuf_p);
      }
   else
      {
      /* error: unknown interrupt from MPC */
      cmn_err(CE_WARN, "Ethernet Driver: unknown interrupt from MPC\n");
      mps_free_msgbuf(mbuf_p);
      }
   DEBUGP(DEB_CALL,(CE_CONT, "enetintr exited\n"));
   }

/*
* enetintr - Send Data to LCI server.
*     enet interrupt handler.
*
* Inputs:
*     level:  interrupt level
*
* This routines is invoked as a result of an interrupt
* generated by the enet when it sends an unsolicited message
* to port LCI_XMIT_PORT requesting data. 
*
* The unsolicited message contains the pointer on the host from
* where the data is to be sent and the length of the data to be sent.
*
*/
void
enet_send_data(unsol_p)
mps_msgbuf_t *unsol_p;
{
   ulong   data_adr;
   ushort   data_len;
   struct dma_buf *dbp;
   mps_msgbuf_t *mbuf_p;
   unsigned long socket_id;
   long		ret;
   uchar	tid;
   ushort	lci_opcode;

   DEBUGP(DEB_CALL,(CE_CONT, "enet_send_data entering send data:\n" ));
   /* the incoming message could be an unsol message, a completion   
      message or an error message */
   if ((unsol_p->mb_flags & MPS_MG_TERR) == MPS_MG_TERR)
   {
   /* log error in a global variable */
   g_lci_error = E_MPC_XMITDATA;
   mps_free_msgbuf(unsol_p); 
   cmn_err(CE_WARN, "lcisend: MPC Error in Send Data\n");
   return;
   }
   else if ((unsol_p->mb_flags & MPS_MG_DONE) == MPS_MG_DONE)
      /* message is a completion message */
   {
   mps_free_msgbuf(unsol_p); 
   DEBUGP(DEB_FULL,(CE_CONT, "enet_send_data 6: Data Xmit completion message\n"));
   return;
   }
   else if ((unsol_p->mb_flags & MPS_MG_UNSOL) == MPS_MG_UNSOL)
   {      /* message is an unsol message */
   DEBUGP(DEB_CALL,(CE_CONT, "enet_send_data 1: send data from enet;mbufp=%x\n",unsol_p));
   /* from the unsol_p get the pointer and the length of the data
      to be sent and convert it to physical address */
   data_adr = Xlate_name(((unsol_msg_str *)(unsol_p->mb_data))->remote_adr);
   data_len = ((unsol_msg_str *)(unsol_p->mb_data))->msg_len;
   DEBUGP(DEB_FULL,(CE_CONT, "enet_send_data 2: data_adr=%lx, len=%d\n",data_adr,data_len));
   /* cmn_err(CE_NOTE, "data_adr=%lx, len=%d\n",data_adr,data_len); */

   /* monitor();  */
   /* allocate a datbuf_p and stuff the physical address in to the
      datbuf_p chain */

   dbp = mps_get_dmabuf(2, DMA_NOSLEEP);
      
   dbp->address = data_adr;
   dbp->count = data_len;
   (dbp->next_buf)->count = 0;
   (dbp->next_buf)->next_buf = (struct dma_buf *)NULL;
   (dbp->next_buf)->address = (ulong)NULL;
   DEBUGP(DEB_FULL,(CE_CONT, "enet_send_data 3: adma data chain created\n"));
    /*
    * Create a buffer request message.
   */
   socket_id = ((ulong)(((unsol_msg_str *)(unsol_p->mb_data))->src_adr)) << 16;
   mbuf_p = mps_get_msgbuf(KM_NOSLEEP);
   mbuf_p->mb_bind = data_adr;
/*
   buf_req = (buf_req_str *) (mbuf_p->mb_data);
   buf_req->dest_adr = ((unsol_msg_str *)(unsol_p->mb_data))->src_adr;
   DEBUGP(DEB_FULL, ("socket id %x, dest_adr %x\n",socket_id, buf_req->dest_adr)); 
   buf_req->src_adr = ics_myslotid();

   buf_req->msg_type = MPS_MG_BREQ;
   buf_req->msg_len1 = (data_len & 0xFF);
   buf_req->msg_len2 = (data_len >> 8) & 0xff;
   buf_req->msg_len3 = '\0';
   buf_req->req_id = 0x80;
   buf_req->hw_preserved = 0;
   buf_req->op_code = LCI_TXDATA_OPCODE;
   buf_req->proto_id = 0;
   buf_req->xmission_ctl = 0;
   buf_req->dest_port_id = LCI_SERVER_PORT;
   buf_req->src_port_id = LCI_XMIT_PORT;
*/
   lci_opcode = LCI_TXDATA_OPCODE;
   DEBUGP(DEB_FULL,(CE_CONT, "enet_send_data 4: About to send buffer request\n")); 
   tid = ((unsol_msg_str *)(unsol_p->mb_data))->trans_id;
   mps_mk_solrply(mbuf_p,(socket_id | LCI_SERVER_PORT),tid,
					(unsigned char *)&lci_opcode,2,1);
/*   DEBUGP(DEB_CALL,(CE_CONT, "enetsend_data: mps_mk_solrply ok return.\n")); */
   mps_free_msgbuf(unsol_p); 
   ret = mps_AMPsend_reply((long)lci_xmit_chan,mbuf_p,dbp); 
  
   if (ret == -1)
       {
       DEBUGP(DEB_CALL,(CE_CONT, "enetsend_data: mps_AMPsend_reply ERROR. data not sent.\n"));
       mps_free_msgbuf(mbuf_p);
       mps_free_dmabuf(dbp);
       }
   }
else cmn_err(CE_WARN, "send_data : Unknown intr\n");
}

void
enet_rcv_data(mbuf_p)
mps_msgbuf_t *mbuf_p;
{
   mps_msgbuf_t    *mbp;
   struct dma_buf    *dbp;
   long      ret;
   struct dma_buf   *temp_datbuf_p;
   ushort      lid;

   register struct req_blk *rb_p;
   register unchar opcode;
   char 	*temp;
   unsigned char *erb_p;
   ulong	buf_len;
   mblk_t	*ibuf_p;
   ulong	ibuf_adr;
   ulong	temp_ulong;
   unsigned long socket_id;
   ushort	ta_len;
   char 	*ta_p;
   ulong	temp_ta_adr;
   /* the message can be either
      -  a buffer request with an lci_code of either lci_put_adr or
         lci_put_data or lci_rbsend_opcode.
      -  a completion message indicating the address array has been
         receive or the data has been received.
      -  an error message.
   */
   if (mbuf_p->mb_data[MPS_MG_MT] == MPS_MG_BREQ)
      /*   the message is a buffer request */
   {
   /* from the br_p get the address array of the pointers and the 
      lengths of the data to be received. The second solicited transfer
      at this port will be the string of bytes of data
   */
   /* for the first phase implementation, an array of only one pointer
      is supported. The first message, then, will always have just one
      pointer and one length.
   */
      
   /*
   get lci_code from the buffer request. 
   */
   temp = (char *)(mbuf_p->mb_data);
   socket_id = ((ulong) (((rb_buf_req_str *)temp)->src_adr)) << 16;
   socket_id |= LCI_SERVER_PORT;
   name_idx = ((rb_buf_req_str *)temp)->name_idx;
   if (((rb_buf_req_str *)temp)->op_code == LCI_RBSEND_OPCODE)
      {
          /* allocb a buffer to hold incoming erb and rb */
      buf_len =(ulong) *((ushort *)&(((buf_req_str *)temp)->msg_len1));
      DEBUGP(DEB_FULL,(CE_CONT, "enetintr 6: BR: Incoming rb len = %x;\n", buf_len));
      if ((ibuf_p = allocb((int)buf_len, BPRI_HI)) == (mblk_t *)NULL) 
          {
          cmn_err(CE_WARN, "Ethernet Driver: no buffer for rb; allocb failed.\n");
          mps_free_msgbuf(mbuf_p);
          return;
          }
    
      /* put adr of buffer in mb_bind */

      ibuf_adr = (ulong)kvtophys((caddr_t)(ibuf_p->b_datap->db_base));
/*       DEBUGP(DEB_FULL,(CE_CONT, "enetintr 6: BR: Incoming rb temp adr = %x;\n", ibuf_adr)); */
         
      /* given ibuf (rb buffer) construct a data chain of the form struct dma_buf */
      dbp = mps_get_dmabuf(2,DMA_NOSLEEP);
      
      /* put ibuf inside dbp */
      dbp->address = ibuf_adr;
      dbp->count = buf_len;
      temp_datbuf_p = dbp->next_buf;
      temp_datbuf_p->count = 0;
      temp_datbuf_p->address = (ulong)NULL;
      temp_datbuf_p->next_buf = (struct dma_buf *)NULL;

      lid = mbuf_p->mb_data[MPS_MG_RI];

      /* Now build a buffer grant message */
      mbp = mps_get_msgbuf(KM_NOSLEEP);
      mbp->mb_bind = (unsigned long)ibuf_p;
      mps_mk_bgrant(mbp,socket_id,lid,(int)dbp->count);
      DEBUGP(DEB_FULL,(CE_CONT, "enetintr 7: BR: About to send buffer grant at %x,socket=%x\n",(char *)mbp,socket_id));
      ret = mps_AMPreceive ((long)lci_rcv_chan,(ulong)socket_id,mbp,dbp);
/*      DEBUGP(DEB_FULL,(CE_CONT, "enetintr 8: BR: Buffer grant sent.Dest socket=%x\n",socket_id)); */
      if (ret != -1)
          {
          DEBUGP(DEB_CALL,(CE_CONT, "enetintr: mps_AMPreceive for rb return; ok return.\n"));
          }
      else
          {
          DEBUGP(DEB_CALL,(CE_CONT, "enetintr: mps_AMPreceive ERROR. rb not rcvd.\n"));
          mps_free_msgbuf(mbp);
          mps_free_dmabuf(dbp);
          freeb(ibuf_p);
	  cmn_err(CE_WARN, "LCI Error: AMP receive error on LCI receive channel.\n");
          monitor();
          }
      mps_free_msgbuf(mbuf_p);
      }
  else if (((buf_req_str *)(mbuf_p->mb_data))->op_code == LCI_RXADRDT_OPCODE)
      {
      lid = mbuf_p->mb_data[MPS_MG_RI];
      mbp = mps_get_msgbuf(KM_NOSLEEP);
      lci_rcv_datbuf_p = mps_get_dmabuf(LCI_RCV_MAXBUFS,DMA_NOSLEEP);
      mbp->mb_bind = (ulong) lci_rcv_datbuf_p;
      mps_mk_bgrant(mbp,socket_id,lid,LCI_MAX_RX_SIZE);
      temp_datbuf_p = lci_rcv_datbuf_p;
      temp_datbuf_p->count = ((rx_buf_req_str *)temp)->buf_len;
      temp_datbuf_p->address = 
		Xlate_name(*(dword *)(((rx_buf_req_str *)temp)->fpointer));
      temp_datbuf_p = temp_datbuf_p->next_buf;
      temp_datbuf_p->next_buf = (struct dma_buf *)NULL;
      temp_datbuf_p->address = (ulong)NULL;
      temp_datbuf_p->count = 0;
      ret = mps_AMPreceive ((long)lci_rcv_chan,(ulong)socket_id,mbp, lci_rcv_datbuf_p);
      if (ret != -1)
          {
          DEBUGP(DEB_CALL,(CE_CONT, "enet_rcv_data: mps_AMPreceive data ok return.\n"));
          }
      else
          {
          DEBUGP(DEB_CALL,(CE_CONT, "enet_rcv_data: mps_AMPreceive ERROR. data not rcvd.\n"));
          mps_free_msgbuf(mbp);
          }
       /* monitor(); */
      mps_free_msgbuf(mbuf_p);
	}
  	else cmn_err(CE_WARN, "rcv_data : Unknown intr in MPS_MG_BREQ\n"); 
   }
   else if ((mbuf_p->mb_flags & MPS_MG_DONE) == MPS_MG_DONE)
      /* the message is a completion message */
   {
      if (mbuf_p->mb_bind == (unsigned long)lci_rcv_datbuf_p)
      /* The bind indicates completion of lci_put_data */
      {
      DEBUGP(DEB_FULL,(CE_CONT, "enet_rcv_data 11: lci_put_data completion msg rcvd\n"));
	/* monitor(); */
         mps_free_msgbuf(mbuf_p);
         return;
      }
      else if (mbuf_p->mb_data[MPS_MG_MT] == MPS_MG_BGRANT)
      {  
      /* completion of a request block received from LCI */
         
          /* DEBUGP(DEB_CALL,(CE_CONT, "enetintr: rb return message.mbuf_p=%x\n",(char *)mbuf_p)); */
	  erb_p = (((mblk_t *)(mbuf_p->mb_bind))->b_datap->db_base);
          temp_ulong = (ulong)(((erb_header_str *)erb_p)->rb_adr);
          buf_len = (ulong)(((erb_header_str *)erb_p)->rb_size) -
                            sizeof(erb_header_str);
          ta_len = (ushort)(((erb_header_str *)erb_p)->ta_size);
          temp_ta_adr = 0;
	  DEBUGP(DEB_CALL,(CE_CONT, "enetintr: erb_p=%lx; rb_p=%lx; rb_len=%x; ta_len=%x\n",erb_p, temp_ulong, buf_len, ta_len));
          rb_p = (struct req_blk *)phystokv(temp_ulong);

	/*
	 * restore original address of RB
	 */
	rb_p = (struct req_blk *)rb_p->rb; /* I001 */

	  /* DEBUGP(DEB_CALL,(CE_CONT, "enetintr: erb_p=%lx; rb_p=%lx; rb_len=%x; ta_len=%x\n",erb_p, rb_p, buf_len, ta_len)); */
          if (ta_len != 0)
             {
    	     switch(((crbh *)rb_p)->c_opcode)  {

              case OP_ACR:
              case OP_SCR:
              case OP_ACRU:
                 temp_ta_adr = *((ulong *)(((struct crrb *)rb_p)->cr_tabufp));
	         break;

              case OP_SDGM:
              case OP_RDGM:
              case OP_WDGM:
                 temp_ta_adr = *((ulong *)(((struct drb *)rb_p)->dr_tabufp));
	         break;

#ifdef EDL
	      case OP_EDL_TRAN:
	      case OP_EDL_RAWTRAN:
                 temp_ta_adr =
		     *((ulong *)(((struct tranrb *)rb_p)->tr_dst_addr_ptr));
	         break;
#endif

              default:
              break;
              }
             ta_p = (char *)phystokv(temp_ta_adr);
             }
          /* points to the rb just received */
         /* DEBUGP(DEB_FULL,(CE_CONT, "enetrcv: returned rb_p=%x,erb_p=%x,rb_len=%x\n",rb_p,erb_p,buf_len)); */

         /* monitor(); */
         if (rb_p != NULL)
         {
         /* copy the transport address from the temporary buffer to where
	    it is supposed to be */
	   if (ta_len !=0 )
	   {
           /* DEBUGP(DEB_FULL,(CE_CONT, "memcpy: about to copy ta from=%x,to=%x,len=%x\n",(char *)(erb_p + ((erb_header_str *)erb_p)->rb_size),ta_p,ta_len)); */
           memcpy (ta_p, (char *)(erb_p + ((erb_header_str *)erb_p)->rb_size),
		(int)ta_len);
           }

         /* copy the request block from the temporary buffer (temp_ulong) to
            where it is supposed to go (rb_p) */
         DEBUGP(DEB_FULL,(CE_CONT, "memcpy: about to copy from=%x,to=%x,len=%x\n",(char *)(erb_p + sizeof(erb_header_str)),(char *)rb_p,buf_len));
         memcpy ((char *)rb_p,(char *)(erb_p + sizeof(erb_header_str)),(int)buf_len);
         /* DEBUGP(DEB_FULL,(CE_CONT, "enetrcv: returned rb copied to streams buffer\n")); */
 	if (name_idx != 0xff)
 	{
 		(void)de_register(name_idx);
 	}
         if (ta_len != 0)
             {
    	     switch(((crbh *)rb_p)->c_opcode)  {

              case OP_ACR:
              case OP_SCR:
              case OP_ACRU:
                 *((ulong *)(((struct crrb *)rb_p)->cr_tabufp)) = temp_ta_adr;
	         break;

              case OP_SDGM:
              case OP_RDGM:
              case OP_WDGM:
                 *((ulong *)(((struct drb *)rb_p)->dr_tabufp)) = temp_ta_adr;
	         break;

              default:
              break;
              }
            }
         freeb((mblk_t *)(mbuf_p->mb_bind));
         opcode = ((crbh *)rb_p)->c_opcode;
         DEBUGP(DEB_FULL,(CE_CONT, "enetintr: returned rb ptr = %x; opcode=%x\n", rb_p,opcode));
	 /* monitor(); */

	 mps_free_msgbuf(mbuf_p);

#ifdef NS
	if (((crbh *)rb_p)->c_subsys == SUB_NS)
	{
		ns_req_complete((struct nsrb *)rb_p);
		return;
	}
#endif
	if (((crbh *)rb_p)->c_subsys == SUB_NMF)
	{
		nmf_req_complete((struct nmfrb *)rb_p);
		return;
	}
         switch(opcode) {
         case OP_OPEN:
            enet_open_complete((struct orb *)rb_p);
            break;
         case OP_ACR:
            enet_accept_conn_ack((struct crrb *)rb_p);
            break;
         case OP_RD:
	    enet_z = SPL();
            enet_data_ind((struct vcrb *)rb_p);
            break;
         case OP_ACLOSE:
                 switch(((struct vcrb *)rb_p)->vc_ep->tli_state) {
            case TS_DATA_XFER:
            case TS_WIND_ORDREL:
            case TS_WREQ_ORDREL:
            case TS_WACK_DREQ9:
            case TS_WACK_DREQ10:
            case TS_WACK_DREQ11:
               enet_stat[ST_CURC]--;
            }
            enet_discon_ind((struct vcrb *)rb_p);
            break;
         case OP_SCR:
            enet_conn_con((struct crrb *)rb_p);
            break;
         case OP_ACRU:
            enet_conn_ind((struct crrb *)rb_p);
            break;
         case OP_SD:
         case OP_EOM_SD:
            enet_data_req_complete((struct vcrb *)rb_p);
            break;
         case OP_CLOSE:
            enet_close_req_complete((struct vcrb *)rb_p);
            break;
         case OP_EX_SD:
            enet_expedited_data_req_complete((struct vcrb *)rb_p);
            break;
         case OP_EX_RD:
            enet_expedited_data_ind((struct vcrb *)rb_p);
            break;
         case OP_SDGM:
            enet_send_datagram_complete((struct drb *)rb_p);
            break;
         case OP_RDGM:
            enet_datagram_ind((struct drb *)rb_p);
            break;
         case OP_WDGM:
            enet_withdraw_datagram_complete((struct drb *)rb_p);
            break;

#ifdef EDL
	 /* EDL cases */
	 case OP_EDL_CONN:
            /*iedl_connect_complete((struct drb *)rb_p);*/
            (*edl_conn_complete)((struct drb *)rb_p);
            break;
	 case OP_EDL_DISC:
            /*iedl_disconnect_complete((struct drb *)rb_p);*/
            (*edl_disc_complete)((struct drb *)rb_p);
            break;
	 case OP_EDL_TRAN:
	 case OP_EDL_RAWTRAN:
            /*iedl_send_complete((struct tranrb *)rb_p);*/
            (*edl_rawtran_complete)((struct tranrb *)rb_p);
            break;
	 case OP_EDL_POST:
	 case OP_EDL_RAWRECV:
            /*iedl_post_complete((struct postrb *)rb_p);*/
            (*edl_rawrecv_complete)((struct postrb *)rb_p);
            break;
#endif

         default:
         /* 
         case OP_ACRT:
         case OP_WRB:
         case OP_WEB:
         case OP_STAT:
         case OP_DEFSTAT:
         case OP_WDB:
         case OP_NMF_RO:
         case OP_NMF_RACO:
         case OP_NMF_SO:
         case OP_NMF_RMEM:
         case OP_NMF_SMEM:
         case OP_NMF_DUMP:
         case OP_NMF_ECHO:
         case OP_NMF_FLOAD:
         case OP_NMF_SUPPLYB:
         case OP_NMF_TAKEB:
         */
            cmn_err(CE_WARN, "Ethernet Driver: Unexpected message (%x) from board\n",
               opcode);
            relrb(rb_p);
            break;
         }
      }
     }
      
     else /* ERROR */
      {
      /*
      log error in a global variable
      */
      g_lci_error = E_MPC_RCVDATA1;
      cmn_err(CE_WARN, "lcircv: MPC Error in Receive Data\n");
      mps_free_msgbuf(mbuf_p);
      return;
      }
   }
   else if ((mbuf_p->mb_flags & MPS_MG_TERR) == MPS_MG_TERR)
      /* message is an error message */
   {
      /*
      log error in a global variable
      */
      g_lci_error = E_MPC_RCVDATA2;
      cmn_err(CE_WARN, "lcircv: MPC Error in Receive Data\n");
      mps_free_msgbuf(mbuf_p);
      return;
   }
else cmn_err(CE_WARN, "Rcv_data : Unknown interrupt\n");

}
