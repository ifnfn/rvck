/* SPDX-License-Identifier: GPL-2.0 */
#if !defined(_TRACE_BMU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_BMU_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM bmu

#include <linux/tracepoint.h>
#include "a210_bmu_type.h"

TRACE_EVENT(
	bmu_cnt_bytes_cycle_trans,
	TP_PROTO(char *name, u8 ch, struct pft_event_array *pft_event),
	TP_ARGS(name, ch, pft_event),
	TP_STRUCT__entry(__field(u32, trac_num) __field(u32, cont_num) __field(
		u8, ch) __array(char, name, 20)
				 __dynamic_array(u8, count,
						 sizeof(struct bmu_count_data) * 3)),
	TP_fast_assign(memcpy(__entry->name, name, 4); __entry->ch = ch;
		       __entry->trac_num = pft_event->trac_num;
		       __entry->cont_num = pft_event->cont_num;
		       memcpy(__get_dynamic_array(count),
			      pft_event->count_event,
			      sizeof(struct bmu_count_data) * 3);),
	TP_printk("bmu=%s chn=%u "
		  "mod=%u id=%u "
		  "rd_bytes=%u rd_cycle=%u rd_trans=%u "
		  "wr_bytes=%u wr_cycle=%u wr_trans=%u ",
		  __entry->name, __entry->ch,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].mod_info,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].usr_id,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].rd_bytes,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].rd_cycle,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].rd_trans,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].wr_bytes,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].wr_cycle,
		  ((struct bmu_count_data *)__get_dynamic_array(count))[0].wr_trans));

TRACE_EVENT(
	bmu_awb_arr_tim_delta,
	TP_PROTO(char *name, u8 ch, struct pft_event_array *pft_event),
	TP_ARGS(name, ch, pft_event),
	TP_STRUCT__entry(
		__field(uint32_t, trac_num) __field(uint32_t, cont_num)
			__field(u8, ch) __array(char, name, 20) __dynamic_array(
				u8, trace, sizeof(struct bmu_trace_data) * 3)
				__dynamic_array(u8, count,
						sizeof(struct bmu_count_data) * 3)),
	TP_fast_assign(memcpy(__entry->name, name, 4); __entry->ch = ch;
		       __entry->trac_num = pft_event->trac_num;
		       __entry->cont_num = pft_event->cont_num;
		       memcpy(__get_dynamic_array(trace),
			      pft_event->trace_event,
			      sizeof(struct bmu_trace_data) * 3);
		       memcpy(__get_dynamic_array(count),
			      pft_event->count_event,
			      sizeof(struct bmu_count_data) * 3);),
	TP_printk("bmu=%s chn=%u "
		  "mod=%u id=%u "
		  "rd_rate=%u wr_rate=%u ",
		  __entry->name, __entry->ch,
		  ((struct bmu_trace_data *)__get_dynamic_array(trace))[0].mod_info,
		  ((struct bmu_trace_data *)__get_dynamic_array(trace))[0].usr_id,
		  ((struct bmu_trace_data *)__get_dynamic_array(trace))[0].rd_rate,
		  ((struct bmu_trace_data *)__get_dynamic_array(trace))[0].wr_rate));
#endif /* _TRACE_BMU_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/soc/zhihe
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
