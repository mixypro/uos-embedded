/*
 * This file contains common functions for the TCP implementation,
 * such as functinos for manipulating the data structures and
 * the TCP timer functions. TCP functions related to input and
 * output is found in tcp_input.c and tcp_output.c respectively.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <buf/buf.h>
#include <mem/mem.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/netif.h>

//checks spare segments for destroy to release memmory space
void tcp_segments_timeout_spares(tcp_socket_t *s);
/*
 * Called every 500 ms and implements the retransmission timer and the timer that
 * removes PCBs that have been in TIME-WAIT for enough time. It also increments
 * various timers such as the inactivity timer in each PCB.
 */
void __attribute__((weak))
tcp_slowtmr (ip_t *ip)
{
	tcp_socket_t *s, *prev;
	unsigned long eff_wnd;
	unsigned char s_remove;      /* flag if a PCB should be removed */
	static const unsigned char tcp_backoff[13] =
		{ 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7};

	++ip->tcp_ticks;
	/* if (s == 0)
		tcp_debug ("tcp_slowtmr: no active sockets\n"); */

	/* Steps through all of the active PCBs. */
	prev = 0;
    //tcp_debug ("tcp_slowtmr: processing active sockets\n");
	for (s=ip->tcp_sockets; s; s=s->next) {
		if (!mutex_trylock (&s->lock))
		    continue;
	    tcp_debug ("tcp_slowtmr: processing active socket $%p\n",s);
		assert ( !tcp_socket_is_state(s, tcpfCLOSED| tcpfTIME_WAIT | tcpfLISTEN) );
		s_remove = 0;

		if (s->state == SYN_SENT && s->nrtx == TCP_SYNMAXRTX) {
			++s_remove;
			tcp_debug ("tcp_slowtmr: max SYN retries reached\n");
		} else if (s->nrtx == TCP_MAXRTX) {
			++s_remove;
			tcp_debug ("tcp_slowtmr: max DATA retries reached\n");
		} else {
			++s->rtime;
			if (s->unacked != 0 && s->rtime >= s->rto) {
				/* Time for a retransmission. */
				tcp_debug ("tcp_slowtmr: rtime %u s->rto %u\n",
					s->rtime, s->rto);

				/* Double retransmission time-out unless we are trying to
				 * connect to somebody (i.e., we are in SYN_SENT). */
				if (s->state != SYN_SENT) {
					s->rto = ((s->sa >> 3) + s->sv) <<
						tcp_backoff[s->nrtx];
				}
				tcp_rexmit (s);

				/* Reduce congestion window and ssthresh. */
				eff_wnd = (s->cwnd < s->snd_wnd) ?
					s->cwnd : s->snd_wnd;
				s->ssthresh = eff_wnd >> 1;
				if (s->ssthresh < s->mss) {
					s->ssthresh = s->mss * 2;
				}
				s->cwnd = s->mss;
				tcp_debug ("tcp_slowtmr: cwnd %u ssthresh %u\n",
					s->cwnd, s->ssthresh);
			}
		}
		/* Check if this PCB has stayed too long in transitional state */
		if (ip->tcp_ticks - s->tmr > TCP_STUCK_TIMEOUT / TCP_SLOW_INTERVAL) {
			if (s->state == SYN_RCVD) {
				++s_remove;
				tcp_debug ("tcp_slowtmr: removing s stuck in %S\n",
					tcp_state_name (s->state));
			} else if (s->state == FIN_WAIT_1 ||
			    s->state == FIN_WAIT_2 || s->state == CLOSING) {
				tcp_list_remove (&s->ip->tcp_sockets, s);
				tcp_set_socket_state (s, TIME_WAIT);
				tcp_list_add (&s->ip->tcp_closing_sockets, s);
			}
		}

		/* If the PCB should be removed, do it. */
		if (s_remove) {
			tcp_socket_purge (s);
			tcp_set_socket_state (s, CLOSED);

			/* Remove PCB from tcp_sockets list. */
			if (prev != 0) {
				assert (s != ip->tcp_sockets);
				prev->next = s->next;
			} else {
				/* This PCB was the first. */
				assert (ip->tcp_sockets == s);
				ip->tcp_sockets = s->next;
			}
		} else {
            tcp_segments_timeout_spares(s);
			prev = s;
		}
		mutex_unlock (&s->lock);
	}

	/* Steps through all of the TIME-WAIT PCBs. */
	prev = 0;
	for (s=ip->tcp_closing_sockets; s; s=s->next) {
        if (!mutex_trylock (&s->lock))
            continue;
		assert (s->state == TIME_WAIT);

		/* Check if this PCB has stayed long enough in TIME-WAIT */
		if (ip->tcp_ticks - s->tmr > 2 * TCP_MSL / TCP_SLOW_INTERVAL) {
			tcp_socket_purge (s);

			/* Remove PCB from tcp_closing_sockets list. */
			if (prev != 0) {
				assert (s != ip->tcp_closing_sockets);
				prev->next = s->next;
			} else {
				/* This PCB was the first. */
				assert (ip->tcp_closing_sockets == s);
				ip->tcp_closing_sockets = s->next;
			}
		} else {
			prev = s;
		}
		mutex_unlock (&s->lock);
	}
}


//checks spare segments for destroy to release memmory space
void tcp_segments_timeout_spares(tcp_socket_t *s){
    //резервирую пару сегментов под постоянную активность сокета 
    unsigned preserve_hotready_spares = 2;
    tcp_segment_t* useg;
    tcp_segment_t* pred = 0;
    for (useg = s->spare_segs; useg != 0; pred = useg, useg = useg->next){
        ++useg->datacrc;
        if (useg->datacrc > 2){
            if (preserve_hotready_spares > 0)
                --preserve_hotready_spares;
            else {
                pred->next = 0;
                tcp_segments_free(0, useg);
                return;
            }
        }
    }
}

/*
 * Is called every TCP_FINE_TIMEOUT (100 ms) and sends delayed ACKs.
 */
void __attribute__((weak))
tcp_fasttmr (ip_t *ip)
{
	tcp_socket_t *s;

	/* send delayed ACKs */
	for (s=ip->tcp_sockets; s; s=s->next) {
		if (s->flags & TF_ACK_DELAY) {
			tcp_debug ("tcp_fasttmr: delayed ACK\n");
			tcp_ack_now (s);
			s->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
		}
		else
		if (s->unsent != 0){
		    //* try to activate tcp output if it freese by ip_output failures
		    mutex_signal(&s->lock, s);
		}
	}

    for (s=ip->tcp_closing_sockets; s; s=s->next) {
        if (!(s->flags & TF_ACK_NOW))
            continue;
#if (TCP_LOCK_STYLE >= TCP_LOCK_RELAXED) || (IP_LOCK_STYLE == IP_LOCK_STYLE_OUT1)
        //avoid sleep on sock lock
        if (!mutex_trylock (&s->lock))
            continue;
        tcp_output_poll (s);
        mutex_unlock(&s->lock);
#else
        tcp_output_poll (s);
#endif
    }
}

/*
 * Deallocates a list of TCP segments (tcp_seg structures).
 */
unsigned char
tcp_segments_free (tcp_socket_t *s, tcp_segment_t *seg)
{
	unsigned char count = 0;
	tcp_segment_t *next;

	for (count=0; seg != 0; seg = next) {
		next = seg->next;
		count += tcp_segment_free (s, seg);
	}
	return count;
}

/*
 * Frees a TCP segment.
 */
unsigned char
tcp_segment_free (tcp_socket_t *s, tcp_segment_t *seg)
{
	unsigned char count = 0;

	if (! seg)
		return 0;

    tcp_event_seg(teFREE, seg, 0);
	if (seg->p != 0) {
		count = buf_free (seg->p);
		seg->p = 0;
	}

	if (s != 0){
	    //drop seg buffer to spare pool
	    //tcp_debug("tcp_spare:push >>seg $%x\n", seg);
	    seg->datacrc = 0;
	    seg->next = s->spare_segs;
	    s->spare_segs = seg;
	}
	else
	    mem_free (seg);

	return count;
}

/*
 * Creates a new TCP protocol control block but doesn't place it on
 * any of the TCP PCB lists.
 */
tcp_socket_t *
tcp_alloc (ip_t *ip)
{
	tcp_socket_t *s;
	unsigned long iss;

	s = (tcp_socket_t *)mem_alloc (ip->pool, sizeof(tcp_socket_t));
	if (s == 0) {
		return 0;
	}

	s->ip = ip;
	s->snd_buf = TCP_SND_BUF;
	s->snd_queuelen = 0;
	s->rcv_wnd = TCP_WND;
	s->mss = TCP_MSS;
	s->rto = 3000 / TCP_SLOW_INTERVAL;
	s->sa = 0;
	s->sv = 3000 / TCP_SLOW_INTERVAL;
	s->rtime = 0;
	s->cwnd = 1;
	iss = tcp_next_seqno (ip);
	s->snd_wl2 = iss;
	s->snd_nxt = iss;
	s->snd_max = iss;
	s->lastack = iss;
	s->snd_lbb = iss;
	s->tmr = ip->tcp_ticks;
	buf_queueh_init(&s->inq, sizeof(s->queue));
	return s;
}

/*
 * Purges a TCP PCB. Removes any buffered data and frees the buffer memory.
 */
void
tcp_socket_purge (tcp_socket_t *s)
{
	tcp_debug ("tcp_socket_purge\n");
	if (s->unsent != 0) {
		tcp_debug ("tcp_socket_purge: not all data sent\n");
		tcp_segments_free (0, s->unsent);
		s->unsent = 0;
	}
	if (s->unacked != 0) {
		tcp_debug ("tcp_socket_purge: data left on ->unacked\n");
	    tcp_segment_t *useg = s->unacked;
	    while (useg != 0){
	        tcp_event_seg(teFREE, useg, s);
	        netif_terminate_buf(0, useg->p);
            s->unacked = useg->next;
	        useg->p = 0;
	        tcp_segment_free (0, useg);
	        useg = s->unacked;
	    }
	}
	if (s->spare_segs != 0){
        tcp_debug ("tcp_socket_purge: data left on ->spare\n");
        tcp_segments_free (0, s->spare_segs);
        s->spare_segs = 0;
	}
	s->snd_queuelen = 0;
	
	buf_free(s->iph_cache);
	s->iph_cache = 0;
}

/*
 * Change socket state. Send a signal to notify a user.
 */
void
tcp_set_socket_state (tcp_socket_t *s, tcp_state_t newstate)
{
	s->state = newstate;
	mutex_signal (&s->lock, s);
}

/*
 * Purges the PCB and removes it from a PCB list. Any delayed ACKs are sent first.
 */
void
tcp_socket_remove (tcp_socket_t **slist, tcp_socket_t *s)
{
	tcp_list_remove (slist, s);

	tcp_socket_purge (s);

	/* if there is an outstanding delayed ACKs, send it */
	if (s->state != TIME_WAIT && s->state != LISTEN &&
	    (s->flags & TF_ACK_DELAY)) {
		s->flags |= TF_ACK_NOW;
		tcp_output_poll (s);
	}
	tcp_set_socket_state (s, CLOSED);

	assert (tcp_debug_verify (s->ip));
}

buf_t *
tcp_queue_get (tcp_socket_t *q)
{
	buf_t *p;

	if (q->inq.count == 0) {
		/*tcp_debug ("tcp_queue_get: returned 0\n");*/
		return 0;
	}
	
	p = buf_queueh_get(&q->inq);

	/* Advertise a larger window when the data has been processed. */
	q->rcv_wnd += p->tot_len;
	if (q->rcv_wnd > TCP_WND) {
		q->rcv_wnd = TCP_WND;
	}
	/*tcp_debug ("tcp_queue_get: returned 0x%04x\n", p);*/
	return p;
}

void
tcp_queue_put (tcp_socket_t *q, buf_t *p)
{
    buf_queueh_put(&q->inq, p);
}

void
tcp_queue_free (tcp_socket_t *q)
{
    buf_queueh_clean(&q->inq);
}

#ifdef TCP_DEBUG
/*
  TCP_FIN         = 0x01
, TCP_SYN         = 0x02
, TCP_RST         = 0x04
, TCP_PSH         = 0x08
, TCP_ACK         = 0x10
, TCP_URG         = 0x20
*/

const unsigned char tcp_flags_dumpfmt[] = 
        "\30\1FIN\2SYN\3RST\4PSH\5ACK\6URG";
void
tcp_debug_print_header (tcp_hdr_t *tcphdr)
{
    tcp_debug ( "TCP header:\n"
                "+-------------------------------+\n"
	            "|      %04u     |      %04u     | (src port, dest port)\n"
                "+-------------------------------+\n"
                "|            %08lx           | (seq no)\n"
                "+------------------------------+\n"
                "|            %08lx           | (ack no)\n"
            , NTOHS(tcphdr->src), NTOHS(tcphdr->dest)
            , NTOHL(tcphdr->seqno), NTOHL(tcphdr->ackno)
		    );
    //                "| %2u |    |%u%u%u%u%u|    %5u      | (offset, flags (",
    tcp_debug ("+-------------------------------+\n"
                  "| %2u |    |%6b|    %5u      | (offset, flags (%b), win)\n"
	            , tcphdr->offset
	            , tcphdr->flags, "\2\0"
                , NTOHS(tcphdr->wnd)
                , tcphdr->flags, tcp_flags_dumpfmt
	            );
    tcp_debug ( "+-------------------------------+\n"
                "|    0x%04x     |     %5u     | (chksum, urgp)\n"
                "+-------------------------------+\n"
            ,NTOHS (tcphdr->chksum), NTOHS (tcphdr->urgp));
}

const char *
tcp_state_name (tcp_state_t state)
{
	switch (state) {
	case CLOSED:	  return "CLOSED";	break;
	case LISTEN:	  return "LISTEN";	break;
	case SYN_SENT:	  return "SYN_SENT";	break;
	case SYN_RCVD:	  return "SYN_RCVD";	break;
	case ESTABLISHED: return "ESTABLISHED";	break;
	case FIN_WAIT_1:  return "FIN_WAIT_1";	break;
	case FIN_WAIT_2:  return "FIN_WAIT_2";	break;
	case CLOSE_WAIT:  return "CLOSE_WAIT";	break;
	case CLOSING:	  return "CLOSING";	break;
	case LAST_ACK:	  return "LAST_ACK";	break;
	case TIME_WAIT:	  return "TIME_WAIT";	break;
	}
	return "???";
}

void
tcp_debug_print_flags (tcph_flag_set flags)
{
    /*
	if (flags & TCP_FIN) tcp_debug (" FIN");
	if (flags & TCP_SYN) tcp_debug (" SYN");
	if (flags & TCP_RST) tcp_debug (" RST");
	if (flags & TCP_PSH) tcp_debug (" PSH");
	if (flags & TCP_ACK) tcp_debug (" ACK");
	if (flags & TCP_URG) tcp_debug (" URG");
	*/
    tcp_debug ("%b", flags, tcp_flags_dumpfmt);
}

void tcp_debug_print_socket (tcp_socket_t *s){
    const char *fmt = "Local port %u, foreign port %u "
              "snd_nxt %lu rcv_nxt %lu state %S\n";
    tcp_debug (fmt
            , s->local_port, s->remote_port
            , s->snd_nxt, s->rcv_nxt
            , tcp_state_name (s->state)
            );
}

void
tcp_debug_print_sockets (ip_t *ip)
{
	tcp_socket_t *s;

	tcp_debug ("Active PCB states:\n");
	for (s=ip->tcp_sockets; s; s=s->next) 
	    tcp_debug_print_socket(s);
	tcp_debug ("Listen PCB states:\n");
	for (s=ip->tcp_listen_sockets; s; s=s->next)
        tcp_debug_print_socket(s);
	tcp_debug ("TIME-WAIT PCB states:\n");
	for (s=ip->tcp_closing_sockets; s; s=s->next)
        tcp_debug_print_socket(s);
}

int
tcp_debug_verify (ip_t *ip)
{
	tcp_socket_t *s;

	for (s = ip->tcp_sockets; s != 0; s = s->next) {
		assert (s->state != CLOSED);
		assert (s->state != LISTEN);
		assert (s->state != TIME_WAIT);
	}
	for (s = ip->tcp_closing_sockets; s != 0; s = s->next) {
		assert (s->state == TIME_WAIT);
	}
	return 1;
}
#endif /* TCP_DEBUG */
