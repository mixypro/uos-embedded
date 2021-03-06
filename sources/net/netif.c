#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <mem/mem.h>
#include <buf/buf.h>
#include <net/netif.h>
#include <net/arp.h>

sock_error
netif_output_prio (netif_t *netif, buf_t *p
        , ip_addr_const ipdest
        , ip_addr_const ipsrc
        , small_uint_t prio)
{
	if (netif->arp && ipadr_not0(ipsrc)) {	/* vch: для бриджуемых фреймов не нужен arp */
		unsigned char *ethdest = 0;

		/* For broadcasts, ipdest must be NULL. */
		if (ipadr_not0(ipdest) && (ipref_as_ucs(ipdest)[0] & 0xf0) != 0xe0) {
			/* Search the ARP table for MAC address. */
			ethdest = arp_lookup (netif, ipdest);
			if (! ethdest) {
				/* Send ARP request. */
			    if (netif_is_overlaped(p)){
			        netif_free_buf(netif, p);
			        p = 0;
			    }
				arp_request (netif, p, ipdest, ipsrc);
				//goto discard;
				++netif->out_discards;
				return SENETUNREACH;
			}
		}
		if (! arp_add_header (netif, p, ipdest, ethdest)) {
            netif_free_buf (netif, p);
//discard:    /* Count this packet as discarded. */
			++netif->out_discards;
			return SEBADFD;
		}
	}
    netif_io_overlap* over = netif_is_overlaped(p);
	if (over){
	    over->status |= nios_IPOK;
	}
	return netif->interface->output (netif, p, prio);
}

sock_error
netif_output (netif_t *netif, buf_t *p
        , ip_addr_const ipdest
        , ip_addr_const ipsrc)
{
	return netif_output_prio (netif, p, ipdest, ipsrc, 0);
}

buf_t *
netif_input (netif_t *netif)
{
	struct _buf_t *p;

	p = netif->interface->input (netif);
	if (p && netif->arp)
		p = arp_input (netif, p);
	return p;
}

void
netif_set_address (netif_t *netif, unsigned char *ethaddr)
{
	if (netif->interface->set_address){
		netif->interface->set_address (netif, ethaddr);
		if (netif->arp)
		    ++(netif->arp->stamp);
	}
}



void netif_free_buf(netif_t *u, buf_t *p){
    netif_io_overlap* over = netif_is_overlaped(p);
    if (over == (void*)0){
        buf_free (p);
        return;
    }
    unsigned action = over->options&nioo_ActionMASK;
    if (action == nioo_ActionNone){
        buf_free (p);
        return;
    }
    over->asynco = nios_inaction;
    if (over->action.signal != 0){ 
        if (action == nioo_ActionMutex){
            mutex_signal(over->action.signal, (void*)(over->arg));
        }
        else
        if (action == nioo_ActionCB){
           over->action.callback(p, over->arg);
        }
    }

    if ( (over->options&nioo_ActionTerminate) == 0){
        over->asynco = nios_leave;
    }
    if ( (over->options&nioo_ActionTerminate) == 0){
        over->asynco = nios_free;
    }
    else {
        action = over->options&nioo_ActionMASK;
        if (action == nioo_ActionNone)
            buf_free (p);
    }
}

// TODO - надо протестировать конкурентную терминацию\обрыв буфера
//прерывает передачу буфера - драйвер не отсылает буфер, а сразу его освобождает 
void netif_abort_buf(netif_t *u, buf_t *p){
    netif_io_overlap* over = netif_is_overlaped(p);
    if (over == (void*)0){
        return;
    }
    
    arch_state_t x = arch_intr_off();
    if (over->asynco <= nios_leave){
    }
    else {
        over->options |= nioo_ActionTerminate;
    }
    arch_intr_restore(x);
}

void netif_terminate_buf(netif_t *u, buf_t *p){
    netif_io_overlap* over = netif_is_overlaped(p);
    if (over == (void*)0){
        buf_free (p);
        return;
    }
    arch_state_t x = arch_intr_off();
    if (over->asynco <= nios_leave){
        buf_free (p);
    }
    else {
        over->options = nioo_ActionTerminate|nioo_ActionNone;
    }
    arch_intr_restore(x);
}

INLINE 
netif_io_overlap* netif_overlap_mark(buf_t *p){
    netif_io_overlap* over = (netif_io_overlap*)((char*)p + sizeof(buf_t));
    assert( (p->payload - sizeof(netif_io_overlap)) >= (unsigned char*)over );
    over->mark = NETIF_OVERLAP_MARK;
    return over;
}

void netif_overlap_assign_mutex(buf_t *p, unsigned options
                                , mutex_t* signal, void* msg)
{
    netif_io_overlap* over = netif_overlap_mark(p);
    if (signal != 0){
        over->action.signal = signal;
        over->arg = (unsigned)msg;
        over->options = options | nioo_ActionMutex;
    }
    else
        over->options = options;
}

void netif_overlap_assign_cb(buf_t *p, unsigned options
                                , netif_callback cb, void* msg)
{
    netif_io_overlap* over = netif_overlap_mark(p);
    if (cb != 0){
        over->action.callback = cb;
        over->arg = (unsigned)msg;
        over->options = options | nioo_ActionCB;
    }
    else
        over->options = options;
}
