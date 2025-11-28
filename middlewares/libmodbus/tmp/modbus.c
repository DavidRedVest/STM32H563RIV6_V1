
#include "modbus.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "errno.h"

#include "modbus_private.h"
#include "FreeRTOS.h"
#include "task.h"
#include "modbus_version.h"
#include "osal_mem.h"

/* Internal use */
#define MSG_LENGTH_UNDEFINED -1


/* Max between RTU and TCP max adu length (so TCP) */
#define MAX_MESSAGE_LENGTH 260


/* 3 steps are used to parse the query */
typedef enum {
    _STEP_FUNCTION,
    _STEP_META,
    _STEP_DATA
} _step_t;


const char *modbus_strerror(int errnum)
{
    switch (errnum) {
    case EMBXILFUN:
        return "Illegal function";
    case EMBXILADD:
        return "Illegal data address";
    case EMBXILVAL:
        return "Illegal data value";
    case EMBXSFAIL:
        return "Slave device or server failure";
    case EMBXACK:
        return "Acknowledge";
    case EMBXSBUSY:
        return "Slave device or server is busy";
    case EMBXNACK:
        return "Negative acknowledge";
    case EMBXMEMPAR:
        return "Memory parity error";
    case EMBXGPATH:
        return "Gateway path unavailable";
    case EMBXGTAR:
        return "Target device failed to respond";
    case EMBBADCRC:
        return "Invalid CRC";
    case EMBBADDATA:
        return "Invalid data";
    case EMBBADEXC:
        return "Invalid exception code";
    case EMBMDATA:
        return "Too many data";
    case EMBBADSLAVE:
        return "Response not from requested slave";
    default:
        return strerror(errnum);
    }
}


void _error_print(modbus_t *ctx, const char *context)
{
    if (ctx->debug) {
        //debug_fprintf(stderr, "ERROR %s", modbus_strerror(errno));
        MODBUS_DEBUG("ERROR %s", modbus_strerror(errno));
        if (context != NULL) {
            //debug_fprintf(stderr, ": %s\n", context);
			MODBUS_DEBUG(": %s \r\n",context);
        } else {
            //debug_fprintf(stderr, "\n");
			MODBUS_DEBUG("\r\n");
        }
    }
}
static void _sleep_response_timeout(modbus_t *ctx)
{
 	//转化为毫秒
	vTaskDelay( ctx->response_timeout.tv_sec * 1000 + ((long int) ctx->response_timeout.tv_usec) / 1000 );
}
int modbus_flush(modbus_t *ctx)
{
    int rc;

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    rc = ctx->backend->flush(ctx);
    if (rc != -1 && ctx->debug) {
        /* Not all backends are able to return the number of bytes flushed */
        MODBUS_DEBUG("Bytes flushed (%d)\n", rc);
    }
    return rc;
}


/* Sends a request/response */
static int send_msg(modbus_t *ctx, uint8_t *msg, int msg_length)
{
    int rc;
    int i;

    msg_length = ctx->backend->send_msg_pre(msg, msg_length);

    if (ctx->debug) {
        for (i = 0; i < msg_length; i++)
            MODBUS_DEBUG("[%.2X]", msg[i]);
        MODBUS_DEBUG("\n");
    }

    /* In recovery mode, the write command will be issued until to be
       successful! Disabled by default. */
    do {
        rc = ctx->backend->send(ctx, msg, msg_length);
        if (rc == -1) {
            _error_print(ctx, NULL);
            if (ctx->error_recovery & MODBUS_ERROR_RECOVERY_LINK) {

                int saved_errno = errno;

                if ((errno == EBADF || errno == ECONNRESET || errno == EPIPE)) {
                    modbus_close(ctx);
                    _sleep_response_timeout(ctx);
                    modbus_connect(ctx);
                } else {
                    _sleep_response_timeout(ctx);
                    modbus_flush(ctx);
                }
                errno = saved_errno;

            }
        }
    } while ((ctx->error_recovery & MODBUS_ERROR_RECOVERY_LINK) && rc == -1);

    if (rc > 0 && rc != msg_length) {
        errno = EMBBADDATA;
        return -1;
    }

    return rc;
}


/* Computes the length to read after the function received */
static uint8_t compute_meta_length_after_function(int function, msg_type_t msg_type)
{
    int length;

    if (msg_type == MSG_INDICATION) {
        if (function <= MODBUS_FC_WRITE_SINGLE_REGISTER) {
            length = 4;
        } else if (function == MODBUS_FC_WRITE_MULTIPLE_COILS ||
                   function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS) {
            length = 5;
        } else if (function == MODBUS_FC_MASK_WRITE_REGISTER) {
            length = 6;
        } else if (function == MODBUS_FC_WRITE_AND_READ_REGISTERS) {
            length = 9;
        } else if( (function == MODBUS_FC_WRITE_FILE_RECORD) ||
			(function == MODBUS_FC_READ_FILE_RECORD)) {
			/* 功能码后只有 ByteCount（1 字节）是固定的；其后子请求均为可变部分 */
        	length = 1;
        } else {
            /* MODBUS_FC_READ_EXCEPTION_STATUS, MODBUS_FC_REPORT_SLAVE_ID */
            length = 0;
        }
    } else {
        /* MSG_CONFIRMATION */
        switch (function) {
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            length = 4;
            break;
        case MODBUS_FC_MASK_WRITE_REGISTER:
            length = 6;
            break;
        case MODBUS_FC_WRITE_FILE_RECORD:
		case MODBUS_FC_READ_FILE_RECORD:
			/* 应答里功能码后同样只有 ByteCount 是固定字段 */
			length = 1;
        	break;
        default:
            length = 1;
        }
    }

    return length;
}

/* Computes the length to read after the meta information (address, count, etc) */
static int
compute_data_length_after_meta(modbus_t *ctx, uint8_t *msg, msg_type_t msg_type)
{
    int function = msg[ctx->backend->header_length];
    int length;
	/* MSG_INDICATION表示请求数据 */
    if (msg_type == MSG_INDICATION) {
        switch (function) {
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            length = msg[ctx->backend->header_length + 5];
            break;
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            length = msg[ctx->backend->header_length + 9];
            break;
		case MODBUS_FC_READ_FILE_RECORD:
        case MODBUS_FC_WRITE_FILE_RECORD:
			/* ByteCount 位于功能码后的第1个字节 */
			length = msg[ctx->backend->header_length + 1];
        	break;
        default:
            length = 0;
        }
    } else {
        /* MSG_CONFIRMATION 回复数据*/
        if (function <= MODBUS_FC_READ_INPUT_REGISTERS ||
            function == MODBUS_FC_REPORT_SLAVE_ID ||
            function == MODBUS_FC_WRITE_AND_READ_REGISTERS) {
            length = msg[ctx->backend->header_length + 1];
        } else if(function == MODBUS_FC_WRITE_FILE_RECORD ||
			function == MODBUS_FC_READ_FILE_RECORD) {
			/* 响应同样 ByteCount 位于功能码后 +1 */
			length = msg[ctx->backend->header_length + 1];
        }
        else {
            length = 0;
        }
    }

    length += ctx->backend->checksum_length;

    return length;
}


int _modbus_receive_msg(modbus_t *ctx, uint8_t *msg, msg_type_t msg_type)
{	
	int rc = 0;
	struct timeval tv;
	struct timeval *p_tv;
	unsigned int length_to_read;
	 int msg_length = 0;
	 int recv_timeout = 0; 
	 _step_t step;

	if(ctx->debug) {
		if(msg_type == MSG_INDICATION){
			MODBUS_DEBUG("Waiting for an indication... \r\n");
		} else {
			MODBUS_DEBUG("Waiting for a confirmation... \r\n");
		}
	}

	if(!ctx->backend->is_connected(ctx)) {
		if(ctx->debug) {
			MODBUS_DEBUG("ERROR The Connection is not established! \r\n");
		}
		return -1;
	}
	step = _STEP_FUNCTION;
	length_to_read = ctx->backend->header_length + 1;

	if(msg_type == MSG_INDICATION) {
		/* Wait for a message, we don't know when the message will be
         * received */
		if( (ctx->indication_timeout.tv_sec == 0) && (ctx->indication_timeout.tv_usec == 0) ) {
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			p_tv = &tv;
		} else {
			/* Wait for an indication (name of a received request by a server, see schema)
             */
            tv.tv_sec = ctx->indication_timeout.tv_sec;
			tv.tv_usec = ctx->indication_timeout.tv_usec;
			p_tv = &tv;
		}
	} else {
		tv.tv_sec = ctx->response_timeout.tv_sec;
		tv.tv_usec = ctx->response_timeout.tv_usec;
		p_tv = &tv;
	}

	while(0 != length_to_read) {
		recv_timeout = p_tv->tv_sec * 1000 + p_tv->tv_usec / 1000;
		rc = ctx->backend->recv(ctx, msg + msg_length, length_to_read, recv_timeout);

       // MODBUS_DEBUG("_modbus_receive_msg rc:%d  time:%d\r\n", rc,recv_timeout);

		if(0 == rc) {
			errno = ECONNRESET;
			rc = -1;
		}

		if(-1 == rc) {
			_error_print(ctx, "read");
		
			if( (ctx->error_recovery & MODBUS_ERROR_RECOVERY_LINK) &&
				(ctx->backend->backend_type == _MODBUS_BACKEND_TYPE_TCP) &&
				(errno == ECONNRESET || errno == ECONNREFUSED || errno == EBADF)) {
				int saved_errno = errno;
				modbus_close(ctx);
				modbus_connect(ctx);
				/* Could be removed by previous calls */
	            errno = saved_errno;
			}
			return -1;
		}

		/* Display the hex code of each character received */
        if (ctx->debug) {
            int i;
            for (i = 0; i < rc; i++)
                MODBUS_DEBUG("<%.2X>", msg[msg_length + i]);
        }
		/* Sums bytes received */
		msg_length += rc;
		/* Computes remaining bytes */
		length_to_read -= rc;

		if(0 == length_to_read) {
			switch(step) {
			case _STEP_FUNCTION:
			{
				/* Function code position */
				length_to_read = compute_meta_length_after_function(
                    msg[ctx->backend->header_length], msg_type);
				if(0 != length_to_read) {
					step = _STEP_META;
					
				}
			} /* else switches straight to the next step */
			break;
			case _STEP_META:
			{
				length_to_read = compute_data_length_after_meta(ctx, msg, msg_type);
				if( (msg_length + length_to_read) > ctx->backend->max_adu_lengeh ) {
					errno = EMBBADDATA;
					_error_print(ctx, "too many data");
					return -1;
				}
				step = _STEP_DATA;
			}
			break;
			default:
				break;
			}
		}

		if(length_to_read >0 && 
			(ctx->byte_timeout.tv_sec > 0 || ctx->byte_timeout.tv_usec > 0 ) ) {
				tv.tv_sec = ctx->byte_timeout.tv_sec;
          	 	tv.tv_usec = ctx->byte_timeout.tv_usec;
           		p_tv = &tv;
			}
		/* else timeout isn't set again, the full response must be read before
           expiration of response timeout (for CONFIRMATION only) */
	}
	
	if (ctx->debug) {
        MODBUS_DEBUG("\n");
	}
    //MODBUS_DEBUG("_modbus_receive_msg end!!! \r\n");
	return ctx->backend->check_integrity(ctx, msg, msg_length);
}




/* Computes the length of the expected response including checksum */
static unsigned int compute_response_length_from_request(modbus_t *ctx, uint8_t *req)
{
    int length;
    const int offset = ctx->backend->header_length;

    switch (req[offset]) {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS: {
        /* Header + nb values (code from write_bits) */
        int nb = (req[offset + 3] << 8) | req[offset + 4];
        length = 2 + (nb / 8) + ((nb % 8) ? 1 : 0);
    } break;
    case MODBUS_FC_WRITE_AND_READ_REGISTERS:
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS:
        /* Header + 2 * nb values */
        length = 2 + 2 * (req[offset + 3] << 8 | req[offset + 4]);
        break;
    case MODBUS_FC_READ_EXCEPTION_STATUS:
        length = 3;
        break;
    case MODBUS_FC_REPORT_SLAVE_ID:
        /* The response is device specific (the header provides the
           length) */
        return MSG_LENGTH_UNDEFINED;
	case MODBUS_FC_READ_FILE_RECORD:
	case MODBUS_FC_WRITE_FILE_RECORD:
         length = req[offset + 1] + 2;
        break;
			//return MSG_LENGTH_UNDEFINED;  /* 响应大小取决于记录数量，无法提前计算 */
    case MODBUS_FC_MASK_WRITE_REGISTER:
        length = 7;
        break;
    default:
        length = 5;
    }

    return offset + length + ctx->backend->checksum_length;
}

static int check_confirmation(modbus_t *ctx, uint8_t *req, uint8_t *rsp, int rsp_length)
{
	int rc;
	int rsp_length_computed;
	const unsigned int offset = ctx->backend->header_length;
	const int function = rsp[offset];

	if(ctx->backend->pre_check_confirmation) {
		rc = ctx->backend->pre_check_confirmation(ctx,req,rsp,rsp_length);
		if(rc == -1) {
			if(ctx->error_recovery & MODBUS_ERROR_RECOVERY_PROTOCOL) {
				_sleep_response_timeout(ctx);
				modbus_flush(ctx);
			}
			return -1;
		}
	}

	rsp_length_computed = compute_response_length_from_request(ctx, req);
	
	/* Exception code */
	if(function >= 0x80 ) {
		if(rsp_length == (int)(offset + 2 + ctx->backend->checksum_length) && 
			req[offset] == (rsp[offset] - 0x80)) {
				/* Valid exception code received */
				int exception_code = rsp[offset + 1];
				if(exception_code < MODBUS_EXCEPTION_MAX) {
					errno = MODBUS_ENOBASE + exception_code;
				} else {
					errno = EMBBADEXC;
				}
				_error_print(ctx, NULL);
				return -1;
		} else {
			errno = EMBBADEXC;
			_error_print(ctx, NULL);
			return -1;
		}
	}

/* Check length */
    if ((rsp_length == rsp_length_computed ||
         rsp_length_computed == MSG_LENGTH_UNDEFINED) &&
        function < 0x80) {
        int req_nb_value = 0;
        int rsp_nb_value = 0;
        int resp_addr_ok = TRUE;
        int resp_data_ok = TRUE;

        /* Check function code */
        if (function != req[offset]) {
            if (ctx->debug) {
                MODBUS_DEBUG(
                    "Received function not corresponding to the request (0x%X != 0x%X)\n",
                    function,
                    req[offset]);
            }
            if (ctx->error_recovery & MODBUS_ERROR_RECOVERY_PROTOCOL) {
                _sleep_response_timeout(ctx);
                modbus_flush(ctx);
            }
            errno = EMBBADDATA;
            return -1;
        }

        /* Check the number of values is corresponding to the request */
        switch (function) {
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            /* Read functions, 8 values in a byte (nb
             * of values in the request and byte count in
             * the response. */
            req_nb_value = (req[offset + 3] << 8) + req[offset + 4];
            req_nb_value = (req_nb_value / 8) + ((req_nb_value % 8) ? 1 : 0);
            rsp_nb_value = rsp[offset + 1];
            break;
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_READ_INPUT_REGISTERS:
            /* Read functions 1 value = 2 bytes */
            req_nb_value = (req[offset + 3] << 8) + req[offset + 4];
            rsp_nb_value = (rsp[offset + 1] / 2);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            /* address in request and response must be equal */
            if ((req[offset + 1] != rsp[offset + 1]) ||
                (req[offset + 2] != rsp[offset + 2])) {
                resp_addr_ok = FALSE;
            }
            /* N Write functions */
            req_nb_value = (req[offset + 3] << 8) + req[offset + 4];
            rsp_nb_value = (rsp[offset + 3] << 8) | rsp[offset + 4];
            break;
        case MODBUS_FC_REPORT_SLAVE_ID:
            /* Report slave ID (bytes received) */
            req_nb_value = rsp_nb_value = rsp[offset + 1];
            break;
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            /* address in request and response must be equal */
            if ((req[offset + 1] != rsp[offset + 1]) ||
                (req[offset + 2] != rsp[offset + 2])) {
                resp_addr_ok = FALSE;
            }
            /* data in request and response must be equal */
            if ((req[offset + 3] != rsp[offset + 3]) ||
                (req[offset + 4] != rsp[offset + 4])) {
                resp_data_ok = FALSE;
            }
            /* 1 Write functions & others */
            req_nb_value = rsp_nb_value = 1;
            break;
		case MODBUS_FC_WRITE_FILE_RECORD:
			if(!memcmp(req,rsp, rsp_length)) {
				//rsp_nb_value = req[2] - 7;	/* data len */
				//req_nb_value = rsp_nb_value;
                req_nb_value = rsp_nb_value = req[2] - 7; /* data len */
			}
			break;
        default:
            /* 1 Write functions & others */
            req_nb_value = rsp_nb_value = 1;
            break;
        }

        if ((req_nb_value == rsp_nb_value) && (resp_addr_ok == TRUE) &&
            (resp_data_ok == TRUE)) {
            rc = rsp_nb_value;
        } else {
            if (ctx->debug) {
                MODBUS_DEBUG("Received data not corresponding to the request (%d != %d)\n",
                        rsp_nb_value,
                        req_nb_value);
            }

            if (ctx->error_recovery & MODBUS_ERROR_RECOVERY_PROTOCOL) {
                _sleep_response_timeout(ctx);
                modbus_flush(ctx);
            }

            errno = EMBBADDATA;
            rc = -1;
        }
    } else {
        if (ctx->debug) {
            MODBUS_DEBUG("Message length not corresponding to the computed length (%d != %d)\n",
                rsp_length,
                rsp_length_computed);
        }
        if (ctx->error_recovery & MODBUS_ERROR_RECOVERY_PROTOCOL) {
            _sleep_response_timeout(ctx);
            modbus_flush(ctx);
        }
        errno = EMBBADDATA;
        rc = -1;
    }
    return rc;	
}


static int
response_io_status(uint8_t *tab_io_status, int address, int nb, uint8_t *rsp, int offset)
{
    int shift = 0;
    /* Instead of byte (not allowed in Win32) */
    int one_byte = 0;
    int i;

    for (i = address; i < address + nb; i++) {
        one_byte |= tab_io_status[i] << shift;
        if (shift == 7) {
            /* Byte is full */
            rsp[offset++] = one_byte;
            one_byte = shift = 0;
        } else {
            shift++;
        }
    }

    if (shift != 0)
        rsp[offset++] = one_byte;

    return offset;
}


/* Build the exception response */
static int response_exception(modbus_t *ctx,
                              sft_t *sft,
                              int exception_code,
                              uint8_t *rsp,
                              unsigned int to_flush,
                              const char *template,
                              ...)
{
	int rsp_length;

	/* Print debug message */
	if(ctx->debug) {
		va_list ap;
		
		va_start(ap, template);
		vfprintf(stderr, template, ap);
		va_end(ap);
	}
	/* Fluash if required */
	if(to_flush) {
		_sleep_response_timeout(ctx);
		modbus_flush(ctx);
	}

	/* Build exception response */
	sft->function = sft->function + 0x80;
	rsp_length = ctx->backend->build_response_basis(sft, rsp);
	rsp[rsp_length++] = exception_code;

	return rsp_length;
}

#if 0
/* 测试回复 */
int modbus_reply(modbus_t *ctx,
                 const uint8_t *req,
                 int req_length,
                 modbus_mapping_t *mb_mapping)
{
	unsigned int offset;
	int slave;
	int function;
	uint16_t address;
	uint8_t rsp[MAX_MESSAGE_LENGTH];
	int rsp_length = 0;
	sft_t sft;

	if(NULL == ctx) {
		errno = EINVAL;
        return -1;
	}

	offset = ctx->backend->header_length;
	slave = req[offset - 1];
	function = req[offset];
	address = (req[offset + 1] << 8) + req[offset + 2];

	sft.slave = slave;
	sft.function = function;
	sft.t_id = ctx->backend->get_response_tid(req);	//目前不需要，返回0
	switch(function)
	{
	case MODBUS_FC_READ_COILS:
	case MODBUS_FC_READ_DISCRETE_INPUTS:
	{
		unsigned int is_input = (function == MODBUS_FC_READ_DISCRETE_INPUTS);
		int start_bits = is_input ? mb_mapping->start_input_bits : mb_mapping->start_bits;
		int nb_bits = is_input ? mb_mapping->nb_input_bits : mb_mapping->nb_bits;
		uint8_t *tab_bits = is_input ? mb_mapping->tab_input_bits : mb_mapping->tab_bits;
		const char *const name = is_input ? "read_input_bits" : "read_bits";
		int nb = (req[offset + 3] << 8) + req[offset + 4];
		/* The mapping can be shifted to reduce memory consumption and it
           doesn't always start at address zero. */
         int mapping_address = address - start_bits;

		if( (nb < 1) || (MODBUS_MAX_READ_BITS < nb) ) {
			rsp_length = response_exception(ctx,
											&sft,
											MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
											rsp,
											TRUE,
											"Illegal nb of values %d in %s (max %d)\n",
											nb,
											name,
											MODBUS_MAX_READ_BITS
											);
		} else if(mapping_address < 0 || (mapping_address + nb) > nb_bits) {
			rsp_length = response_exception(ctx,
											&sft,
											MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
											rsp,
											FALSE,
											"Illegal data address 0x%0X in %s\n",
											mapping_address < 0 ? address : address + nb,
											name);
		} else {
			rsp_length = ctx->backend->build_response_basis(&sft, rsp);
			rsp[rsp_length++] = (nb / 8) + ( (nb % 8) ? 1 : 0 );
			rsp_length = response_io_status(tab_bits, mapping_address, nb, rsp, rsp_length);
		}
		 
	}
	break;
	case MODBUS_FC_READ_HOLDING_REGISTERS:
	case MODBUS_FC_READ_INPUT_REGISTERS:
	{
		unsigned int is_input = (function == MODBUS_FC_READ_INPUT_REGISTERS);
        int start_registers =
            is_input ? mb_mapping->start_input_registers : mb_mapping->start_registers;
        int nb_registers =
            is_input ? mb_mapping->nb_input_registers : mb_mapping->nb_registers;
        uint16_t *tab_registers =
            is_input ? mb_mapping->tab_input_registers : mb_mapping->tab_registers;
        const char *const name = is_input ? "read_input_registers" : "read_registers";
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        /* The mapping can be shifted to reduce memory consumption and it
           doesn't always start at address zero. */
        int mapping_address = address - start_registers;
		
		if (nb < 1 || MODBUS_MAX_READ_REGISTERS < nb) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                                            rsp,
                                            TRUE,
                                            "Illegal nb of values %d in %s (max %d)\n",
                                            nb,
                                            name,
                                            MODBUS_MAX_READ_REGISTERS);
        } else if (mapping_address < 0 || (mapping_address + nb) > nb_registers) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                            rsp,
                                            FALSE,
                                            "Illegal data address 0x%0X in %s\n",
                                            mapping_address < 0 ? address : address + nb,
                                            name);
        } else {
            int i;

            rsp_length = ctx->backend->build_response_basis(&sft, rsp);
            rsp[rsp_length++] = nb << 1;
            for (i = mapping_address; i < mapping_address + nb; i++) {
                rsp[rsp_length++] = tab_registers[i] >> 8;
                rsp[rsp_length++] = tab_registers[i] & 0xFF;
            }
        }

	}
	break;
	case MODBUS_FC_WRITE_SINGLE_COIL:
	{
		int mapping_address = address - mb_mapping->start_bits;

        if (mapping_address < 0 || mapping_address >= mb_mapping->nb_bits) {
            rsp_length = response_exception(ctx,
                &sft,
                MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                rsp,
                FALSE,
                "Illegal data address 0x%0X in write bit\n",
                address);
            break;
        }	
		/* This check is only done here to ensure using memcpy is safe. */
        rsp_length = compute_response_length_from_request(ctx, (uint8_t *) req);
        if (rsp_length != req_length) {
            /* Bad use of modbus_reply */
            rsp_length = response_exception(
                ctx,
                &sft,
                MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                rsp,
                FALSE,
                "Invalid request length in modbus_reply to write bit (%d)\n",
                req_length);
            break;
        }
		/* Don't copy the CRC, if any, it will be computed later (even if identical to the
         * request) */
        rsp_length -= ctx->backend->checksum_length;
		int data = (req[offset + 3] << 8) + req[offset + 4];
		if (data == 0xFF00 || data == 0x0) {
			/* Apply the change to mapping */
			mb_mapping->tab_bits[mapping_address] = data ? ON : OFF;
			/* Prepare response */
			memcpy(rsp, req, rsp_length);
		} else {
			rsp_length = response_exception(
				ctx,
				&sft,
				MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
				rsp,
				FALSE,
				"Illegal data value 0x%0X in write_bit request at address %0X\n",
				data,
				address);
		}
	}
	break;
	case MODBUS_FC_WRITE_SINGLE_REGISTER:
	{
		int mapping_address = address - mb_mapping->start_registers;
	
		if (mapping_address < 0 || mapping_address >= mb_mapping->nb_registers) {
			rsp_length =
				response_exception(ctx,
								   &sft,
								   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
								   rsp,
								   FALSE,
								   "Illegal data address 0x%0X in write_register\n",
								   address);
			break;
		}

		rsp_length = compute_response_length_from_request(ctx, (uint8_t *) req);
		if (rsp_length != req_length) {
			/* Bad use of modbus_reply */
			rsp_length = response_exception(
				ctx,
				&sft,
				MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
				rsp,
				FALSE,
				"Invalid request length in modbus_reply to write register (%d)\n",
				req_length);
			break;
		}
		int data = (req[offset + 3] << 8) + req[offset + 4];

		mb_mapping->tab_registers[mapping_address] = data;

		rsp_length -= ctx->backend->checksum_length;
		memcpy(rsp, req, rsp_length);

	}
	break;
	case MODBUS_FC_WRITE_MULTIPLE_COILS:
	{
		int nb = (req[offset + 3] << 8) + req[offset + 4];
		int nb_bits = req[offset + 5];
		int mapping_address = address - mb_mapping->start_bits;

		if (nb < 1 || MODBUS_MAX_WRITE_BITS < nb || nb_bits * 8 < nb) {
			/* May be the indication has been truncated on reading because of
			 * invalid address (eg. nb is 0 but the request contains values to
			 * write) so it's necessary to flush. */
			rsp_length =
				response_exception(ctx,
								   &sft,
								   MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
								   rsp,
								   TRUE,
								   "Illegal number of values %d in write_bits (max %d)\n",
								   nb,
								   MODBUS_MAX_WRITE_BITS);
		} else if (mapping_address < 0 || (mapping_address + nb) > mb_mapping->nb_bits) {
			rsp_length = response_exception(ctx,
											&sft,
											MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
											rsp,
											FALSE,
											"Illegal data address 0x%0X in write_bits\n",
											mapping_address < 0 ? address : address + nb);
		} else {
			/* 6 = byte count */
			modbus_set_bits_from_bytes(
				mb_mapping->tab_bits, mapping_address, nb, &req[offset + 6]);

			rsp_length = ctx->backend->build_response_basis(&sft, rsp);
			/* 4 to copy the bit address (2) and the quantity of bits */
			memcpy(rsp + rsp_length, req + rsp_length, 4);
			rsp_length += 4;
		}

	}
	break;
	case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
	{
		int nb = (req[offset + 3] << 8) + req[offset + 4];
		int nb_bytes = req[offset + 5];
		int mapping_address = address - mb_mapping->start_registers;

		if (nb < 1 || MODBUS_MAX_WRITE_REGISTERS < nb || nb_bytes != nb * 2) {
			rsp_length = response_exception(
				ctx,
				&sft,
				MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
				rsp,
				TRUE,
				"Illegal number of values %d in write_registers (max %d)\n",
				nb,
				MODBUS_MAX_WRITE_REGISTERS);
		} else if (mapping_address < 0 ||
				   (mapping_address + nb) > mb_mapping->nb_registers) {
			rsp_length =
				response_exception(ctx,
								   &sft,
								   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
								   rsp,
								   FALSE,
								   "Illegal data address 0x%0X in write_registers\n",
								   mapping_address < 0 ? address : address + nb);
		} else {
			int i, j;
			for (i = mapping_address, j = 6; i < mapping_address + nb; i++, j += 2) {
				/* 6 and 7 = first value */
				mb_mapping->tab_registers[i] =
					(req[offset + j] << 8) + req[offset + j + 1];
			}

			rsp_length = ctx->backend->build_response_basis(&sft, rsp);
			/* 4 to copy the address (2) and the no. of registers */
			memcpy(rsp + rsp_length, req + rsp_length, 4);
			rsp_length += 4;
		}

	}
	break;
	case MODBUS_FC_REPORT_SLAVE_ID:
	{
		int str_len;
		int byte_count_pos;

		rsp_length = ctx->backend->build_response_basis(&sft, rsp);
		/* Skip byte count for now */
		byte_count_pos = rsp_length++;
		rsp[rsp_length++] = _REPORT_SLAVE_ID;
		/* Run indicator status to ON */
		rsp[rsp_length++] = 0xFF;
		/* LMB + length of LIBMODBUS_VERSION_STRING */
		str_len = 3 + strlen(LIBMODBUS_VERSION_STRING);
		memcpy(rsp + rsp_length, "LMB" LIBMODBUS_VERSION_STRING, str_len);
		rsp_length += str_len;
		rsp[byte_count_pos] = rsp_length - byte_count_pos - 1;

	}
	break;
	case MODBUS_FC_READ_FILE_RECORD:
	{

	}
	break;
	case MODBUS_FC_WRITE_FILE_RECORD:
	{
        memcpy(rsp, req, req_length);
        rsp_length = req_length;
	}
	break;
	case MODBUS_FC_READ_EXCEPTION_STATUS:
	{
		if (ctx->debug) {
			MODBUS_DEBUG("FIXME Not implemented\n");
		}
		errno = ENOPROTOOPT;
		return -1;

	}
	break;
	case MODBUS_FC_MASK_WRITE_REGISTER:
	{
		int mapping_address = address - mb_mapping->start_registers;
	
		if (mapping_address < 0 || mapping_address >= mb_mapping->nb_registers) {
			rsp_length =
				response_exception(ctx,
								   &sft,
								   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
								   rsp,
								   FALSE,
								   "Illegal data address 0x%0X in write_register\n",
								   address);
		} else {
			uint16_t data = mb_mapping->tab_registers[mapping_address];
			uint16_t and = (req[offset + 3] << 8) + req[offset + 4];
			uint16_t or = (req[offset + 5] << 8) + req[offset + 6];

			data = (data & and) | (or &(~and) );
			mb_mapping->tab_registers[mapping_address] = data;

			rsp_length = compute_response_length_from_request(ctx, (uint8_t *) req);
			if (rsp_length != req_length) {
				/* Bad use of modbus_reply */
				rsp_length = response_exception(ctx,
												&sft,
												MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
												rsp,
												FALSE,
												"Invalid request length in modbus_reply "
												"to mask write register (%d)\n",
												req_length);
				break;
			}

			rsp_length -= ctx->backend->checksum_length;
			memcpy(rsp, req, rsp_length);
		}

	}
	break;
	case MODBUS_FC_WRITE_AND_READ_REGISTERS:
	{
		int nb = (req[offset + 3] << 8) + req[offset + 4];
		uint16_t address_write = (req[offset + 5] << 8) + req[offset + 6];
		int nb_write = (req[offset + 7] << 8) + req[offset + 8];
		int nb_write_bytes = req[offset + 9];
		int mapping_address = address - mb_mapping->start_registers;
		int mapping_address_write = address_write - mb_mapping->start_registers;

		if (nb_write < 1 || MODBUS_MAX_WR_WRITE_REGISTERS < nb_write || nb < 1 ||
			MODBUS_MAX_WR_READ_REGISTERS < nb || nb_write_bytes != nb_write * 2) {
			rsp_length = response_exception(
				ctx,
				&sft,
				MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
				rsp,
				TRUE,
				"Illegal nb of values (W%d, R%d) in write_and_read_registers (max W%d, "
				"R%d)\n",
				nb_write,
				nb,
				MODBUS_MAX_WR_WRITE_REGISTERS,
				MODBUS_MAX_WR_READ_REGISTERS);
		} else if (mapping_address < 0 ||
				   (mapping_address + nb) > mb_mapping->nb_registers ||
				   mapping_address_write < 0 ||
				   (mapping_address_write + nb_write) > mb_mapping->nb_registers) {
			rsp_length = response_exception(
				ctx,
				&sft,
				MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
				rsp,
				FALSE,
				"Illegal data read address 0x%0X or write address 0x%0X "
				"write_and_read_registers\n",
				mapping_address < 0 ? address : address + nb,
				mapping_address_write < 0 ? address_write : address_write + nb_write);
		} else {
			int i, j;
			rsp_length = ctx->backend->build_response_basis(&sft, rsp);
			rsp[rsp_length++] = nb << 1;

			/* Write first.
			   10 and 11 are the offset of the first values to write */
			for (i = mapping_address_write, j = 10; i < mapping_address_write + nb_write;
				 i++, j += 2) {
				mb_mapping->tab_registers[i] =
					(req[offset + j] << 8) + req[offset + j + 1];
			}

			/* and read the data for the response */
			for (i = mapping_address; i < mapping_address + nb; i++) {
				rsp[rsp_length++] = mb_mapping->tab_registers[i] >> 8;
				rsp[rsp_length++] = mb_mapping->tab_registers[i] & 0xFF;
			}
		}

	}
	break;
	default:
		rsp_length = response_exception(ctx,
                                        &sft,
                                        MODBUS_EXCEPTION_ILLEGAL_FUNCTION,
                                        rsp,
                                        TRUE,
                                        "Unknown Modbus function code: 0x%0X\n",
                                        function);		
		break;

	}

	/* Suppress any responses in RTU when the request was a broadcast, excepted when
     * quirk is enabled. */
    if (ctx->backend->backend_type == _MODBUS_BACKEND_TYPE_RTU &&
        slave == MODBUS_BROADCAST_ADDRESS &&
        !(ctx->quirks & MODBUS_QUIRK_REPLY_TO_BROADCAST)) {
        return 0;
    }
    return send_msg(ctx, rsp, rsp_length);
}

#else
/* 对比程序 */
int modbus_reply(modbus_t *ctx,
                 const uint8_t *req,
                 int req_length,
                 modbus_mapping_t *mb_mapping)
{
    unsigned int offset;
    int slave;
    int function;
    uint16_t address;
    uint8_t rsp[MAX_MESSAGE_LENGTH];
    int rsp_length = 0;
    sft_t sft;

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    offset = ctx->backend->header_length;
    slave = req[offset - 1];
    function = req[offset];
    address = (req[offset + 1] << 8) + req[offset + 2];

    sft.slave = slave;
    sft.function = function;
 //   sft.t_id = ctx->backend->prepare_response_tid(req, &req_length);
    sft.t_id = ctx->backend->get_response_tid(req);

    /* Data are flushed on illegal number of values errors. */
    switch (function) {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS: {
        unsigned int is_input = (function == MODBUS_FC_READ_DISCRETE_INPUTS);
        int start_bits = is_input ? mb_mapping->start_input_bits : mb_mapping->start_bits;
        int nb_bits = is_input ? mb_mapping->nb_input_bits : mb_mapping->nb_bits;
        uint8_t *tab_bits = is_input ? mb_mapping->tab_input_bits : mb_mapping->tab_bits;
        const char *const name = is_input ? "read_input_bits" : "read_bits";
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        /* The mapping can be shifted to reduce memory consumption and it
           doesn't always start at address zero. */
        int mapping_address = address - start_bits;

        if (nb < 1 || MODBUS_MAX_READ_BITS < nb) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                                            rsp,
                                            TRUE,
                                            "Illegal nb of values %d in %s (max %d)\n",
                                            nb,
                                            name,
                                            MODBUS_MAX_READ_BITS);
        } else if (mapping_address < 0 || (mapping_address + nb) > nb_bits) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                            rsp,
                                            FALSE,
                                            "Illegal data address 0x%0X in %s\n",
                                            mapping_address < 0 ? address : address + nb,
                                            name);
        } else {
            rsp_length = ctx->backend->build_response_basis(&sft, rsp);
            rsp[rsp_length++] = (nb / 8) + ((nb % 8) ? 1 : 0);
            rsp_length =
                response_io_status(tab_bits, mapping_address, nb, rsp, rsp_length);
        }
    } break;
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS: {
        unsigned int is_input = (function == MODBUS_FC_READ_INPUT_REGISTERS);
        int start_registers =
            is_input ? mb_mapping->start_input_registers : mb_mapping->start_registers;
        int nb_registers =
            is_input ? mb_mapping->nb_input_registers : mb_mapping->nb_registers;
        uint16_t *tab_registers =
            is_input ? mb_mapping->tab_input_registers : mb_mapping->tab_registers;
        const char *const name = is_input ? "read_input_registers" : "read_registers";
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        /* The mapping can be shifted to reduce memory consumption and it
           doesn't always start at address zero. */
        int mapping_address = address - start_registers;

        if (nb < 1 || MODBUS_MAX_READ_REGISTERS < nb) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                                            rsp,
                                            TRUE,
                                            "Illegal nb of values %d in %s (max %d)\n",
                                            nb,
                                            name,
                                            MODBUS_MAX_READ_REGISTERS);
        } else if (mapping_address < 0 || (mapping_address + nb) > nb_registers) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                            rsp,
                                            FALSE,
                                            "Illegal data address 0x%0X in %s\n",
                                            mapping_address < 0 ? address : address + nb,
                                            name);
        } else {
            int i;

            rsp_length = ctx->backend->build_response_basis(&sft, rsp);
            rsp[rsp_length++] = nb << 1;
            for (i = mapping_address; i < mapping_address + nb; i++) {
                rsp[rsp_length++] = tab_registers[i] >> 8;
                rsp[rsp_length++] = tab_registers[i] & 0xFF;
            }
        }
    } break;
    case MODBUS_FC_WRITE_SINGLE_COIL: {
        int mapping_address = address - mb_mapping->start_bits;

        if (mapping_address < 0 || mapping_address >= mb_mapping->nb_bits) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                            rsp,
                                            FALSE,
                                            "Illegal data address 0x%0X in write_bit\n",
                                            address);
        } else {
            int data = (req[offset + 3] << 8) + req[offset + 4];

            if (data == 0xFF00 || data == 0x0) {
                mb_mapping->tab_bits[mapping_address] = data ? ON : OFF;
                memcpy(rsp, req, req_length);
                rsp_length = req_length;
            } else {
                rsp_length = response_exception(
                    ctx,
                    &sft,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                    rsp,
                    FALSE,
                    "Illegal data value 0x%0X in write_bit request at address %0X\n",
                    data,
                    address);
            }
        }
    } break;
    case MODBUS_FC_WRITE_SINGLE_REGISTER: {
        int mapping_address = address - mb_mapping->start_registers;

        if (mapping_address < 0 || mapping_address >= mb_mapping->nb_registers) {
            rsp_length =
                response_exception(ctx,
                                   &sft,
                                   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                   rsp,
                                   FALSE,
                                   "Illegal data address 0x%0X in write_register\n",
                                   address);
        } else {
            int data = (req[offset + 3] << 8) + req[offset + 4];

            mb_mapping->tab_registers[mapping_address] = data;
            memcpy(rsp, req, req_length);
            rsp_length = req_length;
        }
    } break;
    case MODBUS_FC_WRITE_MULTIPLE_COILS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        int nb_bits = req[offset + 5];
        int mapping_address = address - mb_mapping->start_bits;

        if (nb < 1 || MODBUS_MAX_WRITE_BITS < nb || nb_bits * 8 < nb) {
            /* May be the indication has been truncated on reading because of
             * invalid address (eg. nb is 0 but the request contains values to
             * write) so it's necessary to flush. */
            rsp_length =
                response_exception(ctx,
                                   &sft,
                                   MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                                   rsp,
                                   TRUE,
                                   "Illegal number of values %d in write_bits (max %d)\n",
                                   nb,
                                   MODBUS_MAX_WRITE_BITS);
        } else if (mapping_address < 0 || (mapping_address + nb) > mb_mapping->nb_bits) {
            rsp_length = response_exception(ctx,
                                            &sft,
                                            MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                            rsp,
                                            FALSE,
                                            "Illegal data address 0x%0X in write_bits\n",
                                            mapping_address < 0 ? address : address + nb);
        } else {
            /* 6 = byte count */
            modbus_set_bits_from_bytes(
                mb_mapping->tab_bits, mapping_address, nb, &req[offset + 6]);

            rsp_length = ctx->backend->build_response_basis(&sft, rsp);
            /* 4 to copy the bit address (2) and the quantity of bits */
            memcpy(rsp + rsp_length, req + rsp_length, 4);
            rsp_length += 4;
        }
    } break;
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        int nb_bytes = req[offset + 5];
        int mapping_address = address - mb_mapping->start_registers;

        if (nb < 1 || MODBUS_MAX_WRITE_REGISTERS < nb || nb_bytes != nb * 2) {
            rsp_length = response_exception(
                ctx,
                &sft,
                MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                rsp,
                TRUE,
                "Illegal number of values %d in write_registers (max %d)\n",
                nb,
                MODBUS_MAX_WRITE_REGISTERS);
        } else if (mapping_address < 0 ||
                   (mapping_address + nb) > mb_mapping->nb_registers) {
            rsp_length =
                response_exception(ctx,
                                   &sft,
                                   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                   rsp,
                                   FALSE,
                                   "Illegal data address 0x%0X in write_registers\n",
                                   mapping_address < 0 ? address : address + nb);
        } else {
            int i, j;
            for (i = mapping_address, j = 6; i < mapping_address + nb; i++, j += 2) {
                /* 6 and 7 = first value */
                mb_mapping->tab_registers[i] =
                    (req[offset + j] << 8) + req[offset + j + 1];
            }

            rsp_length = ctx->backend->build_response_basis(&sft, rsp);
            /* 4 to copy the address (2) and the no. of registers */
            memcpy(rsp + rsp_length, req + rsp_length, 4);
            rsp_length += 4;
        }
    } break;
    case MODBUS_FC_WRITE_FILE_RECORD: {
        memcpy(rsp, req, req_length);
        rsp_length = req_length;
#if 0
        /* 简单校验: 检查 Sub-Req Reference Type 是否为 0x06 (req[2] 是 ByteCnt, req[3] 是 RefType) */
        /* 注意：req[0]=Slave, req[1]=FC, req[2]=ByteCnt, req[3]=RefType */
        if (req_length > 3 && req[3] == 0x06) {
            memcpy(rsp, req, req_length);
            rsp_length = req_length;
        } else {
            /* 返回异常码 03 (Illegal Data Value) */
            rsp_length = response_exception(ctx, &sft, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE, rsp, TRUE, "Bad Ref Type");
        }
#endif
    } break;
    case MODBUS_FC_REPORT_SLAVE_ID: {
        int str_len;
        int byte_count_pos;

        rsp_length = ctx->backend->build_response_basis(&sft, rsp);
        /* Skip byte count for now */
        byte_count_pos = rsp_length++;
        rsp[rsp_length++] = _REPORT_SLAVE_ID;
        /* Run indicator status to ON */
        rsp[rsp_length++] = 0xFF;
        /* LMB + length of LIBMODBUS_VERSION_STRING */
        str_len = 3 + strlen(LIBMODBUS_VERSION_STRING);
        memcpy(rsp + rsp_length, "LMB" LIBMODBUS_VERSION_STRING, str_len);
        rsp_length += str_len;
        rsp[byte_count_pos] = rsp_length - byte_count_pos - 1;
    } break;
    case MODBUS_FC_READ_EXCEPTION_STATUS:
        if (ctx->debug) {
            //debug_fprintf(stderr, "FIXME Not implemented\n");
        }
        errno = ENOPROTOOPT;
        return -1;
        break;
    case MODBUS_FC_MASK_WRITE_REGISTER: {
        int mapping_address = address - mb_mapping->start_registers;

        if (mapping_address < 0 || mapping_address >= mb_mapping->nb_registers) {
            rsp_length =
                response_exception(ctx,
                                   &sft,
                                   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                                   rsp,
                                   FALSE,
                                   "Illegal data address 0x%0X in write_register\n",
                                   address);
        } else {
            uint16_t data = mb_mapping->tab_registers[mapping_address];
            uint16_t and = (req[offset + 3] << 8) + req[offset + 4];
            uint16_t or = (req[offset + 5] << 8) + req[offset + 6];

            data = (data & and) | (or &(~and));
            mb_mapping->tab_registers[mapping_address] = data;
            memcpy(rsp, req, req_length);
            rsp_length = req_length;
        }
    } break;
    case MODBUS_FC_WRITE_AND_READ_REGISTERS: {
        int nb = (req[offset + 3] << 8) + req[offset + 4];
        uint16_t address_write = (req[offset + 5] << 8) + req[offset + 6];
        int nb_write = (req[offset + 7] << 8) + req[offset + 8];
        int nb_write_bytes = req[offset + 9];
        int mapping_address = address - mb_mapping->start_registers;
        int mapping_address_write = address_write - mb_mapping->start_registers;

        if (nb_write < 1 || MODBUS_MAX_WR_WRITE_REGISTERS < nb_write || nb < 1 ||
            MODBUS_MAX_WR_READ_REGISTERS < nb || nb_write_bytes != nb_write * 2) {
            rsp_length = response_exception(
                ctx,
                &sft,
                MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                rsp,
                TRUE,
                "Illegal nb of values (W%d, R%d) in write_and_read_registers (max W%d, "
                "R%d)\n",
                nb_write,
                nb,
                MODBUS_MAX_WR_WRITE_REGISTERS,
                MODBUS_MAX_WR_READ_REGISTERS);
        } else if (mapping_address < 0 ||
                   (mapping_address + nb) > mb_mapping->nb_registers ||
                   mapping_address_write < 0 ||
                   (mapping_address_write + nb_write) > mb_mapping->nb_registers) {
            rsp_length = response_exception(
                ctx,
                &sft,
                MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                rsp,
                FALSE,
                "Illegal data read address 0x%0X or write address 0x%0X "
                "write_and_read_registers\n",
                mapping_address < 0 ? address : address + nb,
                mapping_address_write < 0 ? address_write : address_write + nb_write);
        } else {
            int i, j;
            rsp_length = ctx->backend->build_response_basis(&sft, rsp);
            rsp[rsp_length++] = nb << 1;

            /* Write first.
               10 and 11 are the offset of the first values to write */
            for (i = mapping_address_write, j = 10; i < mapping_address_write + nb_write;
                 i++, j += 2) {
                mb_mapping->tab_registers[i] =
                    (req[offset + j] << 8) + req[offset + j + 1];
            }

            /* and read the data for the response */
            for (i = mapping_address; i < mapping_address + nb; i++) {
                rsp[rsp_length++] = mb_mapping->tab_registers[i] >> 8;
                rsp[rsp_length++] = mb_mapping->tab_registers[i] & 0xFF;
            }
        }
    } break;

    default:
        rsp_length = response_exception(ctx,
                                        &sft,
                                        MODBUS_EXCEPTION_ILLEGAL_FUNCTION,
                                        rsp,
                                        TRUE,
                                        "Unknown Modbus function code: 0x%0X\n",
                                        function);
        break;
    }

    /* Suppress any responses in RTU when the request was a broadcast, excepted when quirk
     * is enabled. */
    if (ctx->backend->backend_type == _MODBUS_BACKEND_TYPE_RTU &&
        slave == MODBUS_BROADCAST_ADDRESS &&
        !(ctx->quirks & MODBUS_QUIRK_REPLY_TO_BROADCAST)) {
        return 0;
    }
    return send_msg(ctx, rsp, rsp_length);
}

#endif

void _modbus_init_common(modbus_t *ctx)
{
    /* Slave and socket are initialized to -1 */
    ctx->slave = -1;
    ctx->s = -1;

    ctx->debug = FALSE; //TRUE--FALSE
    ctx->error_recovery = MODBUS_ERROR_RECOVERY_NONE;
    ctx->quirks = MODBUS_QUIRK_NONE;

    ctx->response_timeout.tv_sec = 0;
    ctx->response_timeout.tv_usec = _RESPONSE_TIMEOUT;

    ctx->byte_timeout.tv_sec = 0;
    ctx->byte_timeout.tv_usec = _BYTE_TIMEOUT;

    ctx->indication_timeout.tv_sec = 0;
    ctx->indication_timeout.tv_usec = 0;
}

/* Reads IO Status */
static int read_io_status(modbus_t *ctx,int function, int addr, int nb, uint8_t *dest)
{
	int rc;
	int req_length;

	uint8_t req[_MIN_REQ_LENGTH];
	uint8_t rsp[MAX_MESSAGE_LENGTH];

	req_length = ctx->backend->build_request_basis(ctx, function, addr, nb, req);

	rc = send_msg(ctx, req, req_length);
	if(rc > 0) {
		int temp, bit;
		int pos = 0;
		unsigned int offset;
		unsigned int offset_end;

		rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
		if( -1 == rc) {
			return -1;
		}

		rc = check_confirmation(ctx, req, rsp, rc);
		if(-1 == rc) {
			return -1;
		}
		offset = ctx->backend->header_length + 2;
		offset_end = offset + rc;
		for (unsigned int i = offset; i < offset_end; i++) {
			/* Shift reg hi_byte to temp */
			temp = rsp[i];

			for (bit = 0x01; (bit & 0xff) && (pos < nb);) {
				dest[pos++] = (temp & bit) ? TRUE : FALSE;
				bit = bit << 1;
			}
		}

	}
	return rc;
}

/* Reads the boolean status of bits and sets the array elements
   in the destination to TRUE or FALSE (single bits). */
int modbus_read_bits(modbus_t *ctx, int addr, int nb, uint8_t *dest)
{
	int rc;
	if(NULL == ctx) {
		errno = EINVAL;
		return -1;
	}

	if(nb > MODBUS_MAX_READ_BITS) {
		if(ctx->debug) {
			MODBUS_DEBUG("ERROR Too many bits requested (%d > %d)\n", nb, MODBUS_MAX_READ_BITS);
		}
		errno = EMBMDATA;
		return -1;
	}

	rc = read_io_status(ctx, MODBUS_FC_READ_COILS, addr,nb,dest);
	if(-1 == rc) {
		return -1;
	} else {
		return nb;
	}
}

int modbus_read_input_bits(modbus_t *ctx, int addr, int nb, uint8_t *dest)
{
	int rc;
	if(NULL == ctx) {
		errno = EINVAL;
		return -1;
	}
	if(nb > MODBUS_MAX_READ_BITS) {
		if(ctx->debug) {
			MODBUS_DEBUG("ERROR Too many discrete inputs requested (%d > %d)\n",
                    nb,
                    MODBUS_MAX_READ_BITS);
		}
		errno = EMBMDATA;
		return -1;
	}

	rc = read_io_status(ctx, MODBUS_FC_READ_DISCRETE_INPUTS, addr, nb, dest);
	if(-1 == rc) {
		return -1;
	} else {
		return nb;
	}
}

static int read_registers(modbus_t *ctx, int function, int addr, int nb, uint16_t *dest)
{
	int rc;
	int req_length;
	uint8_t req[_MIN_REQ_LENGTH];
	uint8_t rsp[MAX_MESSAGE_LENGTH];

	if (nb > MODBUS_MAX_READ_REGISTERS) {
		if (ctx->debug) {
			MODBUS_DEBUG("ERROR Too many registers requested (%d > %d)\n",
					nb,
					MODBUS_MAX_READ_REGISTERS);
		}
		errno = EMBMDATA;
		return -1;
	}

	req_length = ctx->backend->build_request_basis(ctx, function, addr, nb, req);

	rc = send_msg(ctx, req, req_length);
	if (rc > 0) {
		unsigned int offset;
		int i;

		rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
		if (rc == -1)
			return -1;

		rc = check_confirmation(ctx, req, rsp, rc);
		if (rc == -1)
			return -1;

		offset = ctx->backend->header_length;

		for (i = 0; i < rc; i++) {
			/* shift reg hi_byte to temp OR with lo_byte */
			dest[i] = (rsp[offset + 2 + (i << 1)] << 8) | rsp[offset + 3 + (i << 1)];
		}
	}

	return rc;
}

int modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest)
{
	int status;

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (nb > MODBUS_MAX_READ_REGISTERS) {
        if (ctx->debug) {
            MODBUS_DEBUG("ERROR Too many registers requested (%d > %d)\n",
                    nb,
                    MODBUS_MAX_READ_REGISTERS);
        }
        errno = EMBMDATA;
        return -1;
    }

    status = read_registers(ctx, MODBUS_FC_READ_HOLDING_REGISTERS, addr, nb, dest);
    return status;
}

int modbus_read_input_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest)
{
	int status;

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (nb > MODBUS_MAX_READ_REGISTERS) {
        MODBUS_DEBUG("ERROR Too many input registers requested (%d > %d)\n",
                nb,
                MODBUS_MAX_READ_REGISTERS);
        errno = EMBMDATA;
        return -1;
    }

    status = read_registers(ctx, MODBUS_FC_READ_INPUT_REGISTERS, addr, nb, dest);

    return status;
}

static int write_single(modbus_t *ctx, int function, int addr, const uint16_t value)
{
	int rc;
    int req_length;
    uint8_t req[_MIN_REQ_LENGTH];

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    req_length = ctx->backend->build_request_basis(ctx, function, addr, (int) value, req);

    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        /* Used by write_bit and write_register */
        uint8_t rsp[MAX_MESSAGE_LENGTH];

        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc == -1)
            return -1;

        rc = check_confirmation(ctx, req, rsp, rc);
    }

    return rc;
}

int modbus_write_bit(modbus_t *ctx, int addr, int status)
{
	if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    return write_single(ctx, MODBUS_FC_WRITE_SINGLE_COIL, addr, status ? 0xFF00 : 0);
}

int modbus_write_register(modbus_t *ctx, int addr, const uint16_t value)
{
	if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    return write_single(ctx, MODBUS_FC_WRITE_SINGLE_REGISTER, addr, value);
}

int modbus_write_bits(modbus_t *ctx, int addr, int nb, const uint8_t *src)
{
	int rc;
    int i;
    int byte_count;
    int req_length;
    int bit_check = 0;
    int pos = 0;
    uint8_t req[MAX_MESSAGE_LENGTH];

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (nb > MODBUS_MAX_WRITE_BITS) {
        if (ctx->debug) {
            MODBUS_DEBUG("ERROR Writing too many bits (%d > %d)\n",
                    nb,
                    MODBUS_MAX_WRITE_BITS);
        }
        errno = EMBMDATA;
        return -1;
    }

    req_length = ctx->backend->build_request_basis(
        ctx, MODBUS_FC_WRITE_MULTIPLE_COILS, addr, nb, req);
    byte_count = (nb / 8) + ((nb % 8) ? 1 : 0);
    req[req_length++] = byte_count;

    for (i = 0; i < byte_count; i++) {
        int bit;

        bit = 0x01;
        req[req_length] = 0;

        while ((bit & 0xFF) && (bit_check++ < nb)) {
            if (src[pos++])
                req[req_length] |= bit;
            else
                req[req_length] &= ~bit;

            bit = bit << 1;
        }
        req_length++;
    }

    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        uint8_t rsp[MAX_MESSAGE_LENGTH];

        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc == -1)
            return -1;

        rc = check_confirmation(ctx, req, rsp, rc);
    }

    return rc;
}

int modbus_write_registers(modbus_t *ctx, int addr, int nb, const uint16_t *src)
{
	int rc;
    int i;
    int req_length;
    int byte_count;
    uint8_t req[MAX_MESSAGE_LENGTH];

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (nb > MODBUS_MAX_WRITE_REGISTERS) {
        if (ctx->debug) {
            MODBUS_DEBUG("ERROR Trying to write to too many registers (%d > %d)\n",
                    nb,
                    MODBUS_MAX_WRITE_REGISTERS);
        }
        errno = EMBMDATA;
        return -1;
    }

    req_length = ctx->backend->build_request_basis(
        ctx, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, addr, nb, req);
    byte_count = nb * 2;
    req[req_length++] = byte_count;

    for (i = 0; i < nb; i++) {
        req[req_length++] = src[i] >> 8;
        req[req_length++] = src[i] & 0x00FF;
    }

    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        uint8_t rsp[MAX_MESSAGE_LENGTH];

        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc == -1)
            return -1;

        rc = check_confirmation(ctx, req, rsp, rc);
    }

    return rc;
}

int modbus_mask_write_register(modbus_t *ctx,
                               int addr,
                               uint16_t and_mask,
                               uint16_t or_mask)
{
    int rc;
    int req_length;
    /* The request length can not exceed _MIN_REQ_LENGTH - 2 and 4 bytes to
     * store the masks. The ugly substraction is there to remove the 'nb' value
     * (2 bytes) which is not used. */
    uint8_t req[_MIN_REQ_LENGTH + 2];

    req_length = ctx->backend->build_request_basis(
        ctx, MODBUS_FC_MASK_WRITE_REGISTER, addr, 0, req);

    /* HACKISH, count is not used */
    req_length -= 2;

    req[req_length++] = and_mask >> 8;
    req[req_length++] = and_mask & 0x00ff;
    req[req_length++] = or_mask >> 8;
    req[req_length++] = or_mask & 0x00ff;

    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        /* Used by write_bit and write_register */
        uint8_t rsp[MAX_MESSAGE_LENGTH];

        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc == -1)
            return -1;

        rc = check_confirmation(ctx, req, rsp, rc);
    }

    return rc;
}

/* Write multiple registers from src array to remote device and read multiple
   registers from remote device to dest array. */
int modbus_write_and_read_registers(modbus_t *ctx,
                                    int write_addr,
                                    int write_nb,
                                    const uint16_t *src,
                                    int read_addr,
                                    int read_nb,
                                    uint16_t *dest)

{
    int rc;
    int req_length;
    int i;
    int byte_count;
    uint8_t req[MAX_MESSAGE_LENGTH];
    uint8_t rsp[MAX_MESSAGE_LENGTH];

    if (ctx == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (write_nb > MODBUS_MAX_WR_WRITE_REGISTERS) {
        if (ctx->debug) {
            MODBUS_DEBUG("ERROR Too many registers to write (%d > %d)\n",
                    write_nb,
                    MODBUS_MAX_WR_WRITE_REGISTERS);
        }
        errno = EMBMDATA;
        return -1;
    }

    if (read_nb > MODBUS_MAX_WR_READ_REGISTERS) {
        if (ctx->debug) {
            MODBUS_DEBUG("ERROR Too many registers requested (%d > %d)\n",
                    read_nb,
                    MODBUS_MAX_WR_READ_REGISTERS);
        }
        errno = EMBMDATA;
        return -1;
    }
    req_length = ctx->backend->build_request_basis(
        ctx, MODBUS_FC_WRITE_AND_READ_REGISTERS, read_addr, read_nb, req);

    req[req_length++] = write_addr >> 8;
    req[req_length++] = write_addr & 0x00ff;
    req[req_length++] = write_nb >> 8;
    req[req_length++] = write_nb & 0x00ff;
    byte_count = write_nb * 2;
    req[req_length++] = byte_count;

    for (i = 0; i < write_nb; i++) {
        req[req_length++] = src[i] >> 8;
        req[req_length++] = src[i] & 0x00FF;
    }

    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        unsigned int offset;

        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc == -1)
            return -1;

        rc = check_confirmation(ctx, req, rsp, rc);
        if (rc == -1)
            return -1;

        offset = ctx->backend->header_length;
        for (i = 0; i < rc; i++) {
            /* shift reg hi_byte to temp OR with lo_byte */
            dest[i] = (rsp[offset + 2 + (i << 1)] << 8) | rsp[offset + 3 + (i << 1)];
        }
    }

    return rc;
}

#if 0
int modbus_write_file_record(modbus_t *ctx,
                               uint16_t file_no,
                               uint16_t record_no,
                                uint8_t *buffer,
                               uint16_t len)
{

	int rc;
    int req_length;
    int i;
    uint8_t req[MAX_MESSAGE_LENGTH];
    uint8_t rsp[MAX_MESSAGE_LENGTH];
	/* 获取当前后端（RTU/TCP）的头部长度偏移量,(RTU=1, TCP=7) */
	int header_length = ctx->backend->header_length;
    

	if(NULL == ctx || NULL == buffer) {
		errno = EINVAL;
		return -1;
	}

	/* 长度校验：
     * 1. 长度不能超过 ADU 限制 (256 - header - crc - PDU overhead)
     * 2. len 必须 > 0
     */
	if(len < 1 || len > 244) {
		errno = EMBMDATA;
		return -1;
	}
    /* 计算对齐后的长度（寄存器数量 * 2） */
	int data_len_aligned = (len + 1) & ~0x01;
	int word_count = data_len_aligned / 2;

    /* 包头 */
    req[0] = ctx->slave;
	/* 填充PDU */
    req[header_length] = MODBUS_FC_WRITE_FILE_RECORD;
    req[header_length + 1] = 7 + data_len_aligned;
    req[header_length + 2] = 0x06;

	/* File Number */
	req[header_length + 3] = file_no >> 8;
	req[header_length + 4] = file_no & 0x00FF;

	/* Record Number */
	req[header_length + 5] = record_no >> 8;
	req[header_length + 6] = record_no & 0x00FF;

	/* Register Length */
	req[header_length + 7] = (word_count) >> 8;
	req[header_length + 8] = (word_count) & 0x00FF;

	/* PDU当前长度 */
	int pdu_data_offset = header_length + 9;

	/* 填充数据，处理内存越界问题 */
	for(i = 0; i < len; ++i) {
		req[pdu_data_offset++] = buffer[i];
	}
	/* 如果原始长度是奇数，补一个 0x00 */
    if (len < data_len_aligned) {
        req[pdu_data_offset++] = 0x00;
    }

	/* 计算总长度传入 send_msg */
    req_length = header_length + 9 + data_len_aligned;

	rc = send_msg(ctx, req, req_length);
	if(rc > 0) {

		rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
		if(rc < 0){
			return -1;
		}

		rc = check_confirmation(ctx, req, rsp, rc);
	}
	return rc;
}

#else

int modbus_write_file_record(modbus_t *ctx,
                               uint16_t file_no,
                               uint16_t record_no,
                                uint8_t *buffer,
                               uint16_t len)
{
    int rc;
    int i;
    int byte_count;
    int req_length;
    int bit_check = 0;
    int pos = 0;
    uint8_t req[MAX_MESSAGE_LENGTH];

    /* 长度是2N */
    len = (len + 1) & ~0x1;

    /* ADU最大是256, 256-1字节设备地址-2字节CRC=253
     * 功能码等包头是7字节
     * 传输的数据最大253-9=244
     */
    if (len < 2 || len > 244 )
        return -1;

    /* 包头 */
    req[0] = ctx->slave;
    req[1] = MODBUS_FC_WRITE_FILE_RECORD;
    req[2] = 7 + len;        /* Request data length */
    req[3] = 0x06;           /* Sub-Req. x, Reference Type */

    req[4] = file_no >> 8;        /* Sub-Req. x, File Number */
    req[5] = file_no & 0x00ff;

    req[6] = record_no >> 8;        /* Sub-Req. x, Record Number */
    req[7] = record_no & 0x00ff;
    
    req[8] = (len/2) >> 8;        /* Sub-Req. x, Record length */
    req[9] = (len/2) & 0x00ff;

    req_length = 10;

    /* 数据 */
    for (i = 0; i < len; i++)
    {
        req[req_length++] = buffer[i];
    }

    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        /* Used by write_bit and write_register */
        uint8_t rsp[MAX_MESSAGE_LENGTH];

        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc < 0) {
            return -2;
        }
            

        rc = check_confirmation(ctx, req, rsp, rc);
    }

    return rc;

}
#endif

/* * 功能码 0x14: 读文件记录 
 * len: 这里指需要读取的“字(Word)”的数量，即寄存器数量。
 * dest: 存放读取结果的缓冲区，大小至少为 len * 2 字节。
 */
int modbus_read_file_record(modbus_t *ctx, uint16_t file_no, uint16_t record_no, 
                            uint8_t *dest, uint16_t len)
{
    int rc;
    int req_length;
    int i;
    uint8_t req[MAX_MESSAGE_LENGTH];
    uint8_t rsp[MAX_MESSAGE_LENGTH];
    int header_length = ctx->backend->header_length;

    if (ctx == NULL || dest == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (len > 125) { /* 125 words * 2 = 250 bytes，接近 ADU 上限 */
        errno = EMBMDATA;
        return -1;
    }

    /* --- 构建请求 PDU --- */
    req[header_length] = MODBUS_FC_READ_FILE_RECORD;
    
    /* Byte Count: 0x06(RefType) + File(2) + Rec(2) + Len(2) = 7 字节 */
    req[header_length + 1] = 0x07;
    
    /* Sub-Req Reference Type: 0x06 */
    req[header_length + 2] = 0x06;
    
    /* File Number */
    req[header_length + 3] = file_no >> 8;
    req[header_length + 4] = file_no & 0x00FF;
    
    /* Record Number */
    req[header_length + 5] = record_no >> 8;
    req[header_length + 6] = record_no & 0x00FF;
    
    /* Record Length (Word Count) */
    req[header_length + 7] = len >> 8;
    req[header_length + 8] = len & 0x00FF;

    req_length = header_length + 9;

    /* --- 发送与接收 --- */
    rc = send_msg(ctx, req, req_length);
    if (rc > 0) {
        rc = _modbus_receive_msg(ctx, rsp, MSG_CONFIRMATION);
        if (rc == -1) return -1;

        rc = check_confirmation(ctx, req, rsp, rc);
        if (rc == -1) return -1;

        /* * --- 解析响应数据 --- 
         * 响应格式：
         * Header
         * Function Code (1 byte)
         * Resp. Data Length (1 byte) - 总长度
         * Sub-Req 1 Data Length (1 byte) - 实际数据长度 + 1 (RefType)
         * Sub-Req 1 Ref Type (1 byte) - 0x06
         * Data (N bytes)
         */
        
        int offset = header_length;
        
        /* 校验 Sub-Req Ref Type 是否为 0x06 */
        /* offset+1 是总字节数，offset+2 是第一个子请求长度，offset+3 是 RefType */
        if (rsp[offset + 3] != 0x06) {
            errno = EMBBADDATA;
            return -1;
        }

        /* 获取实际数据字节数 (Sub-Req Length - 1 byte for RefType) */
        int data_bytes_received = rsp[offset + 2] - 1;
        
        /* 检查返回的数据长度是否符合预期 (请求的字数 * 2) */
        if (data_bytes_received != (len * 2)) {
            errno = EMBBADDATA; // 或者返回部分长度
            // 注意：这里可以选择返回实际读到的长度，或者报错。
            // 严格模式下，如果不匹配则报错。
        }

        /* 拷贝数据到用户缓冲区 */
        for (i = 0; i < data_bytes_received; i++) {
            dest[i] = rsp[offset + 4 + i];
        }
        
        /* 返回读取到的字节数 */
        return data_bytes_received;
    }

    return rc;
}
/* * 优化 2: 纯数值操作的大小端转换
 * 比指针操作更快，且无需强制类型转换。
 * 如果使用 GCC/Clang，可以直接用 __builtin_bswap32(val)
 */
static inline uint32_t swap_uint32(uint32_t val)
{
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
}

/* CRC32 计算 */
static uint32_t calc_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}
int modbus_write_file(modbus_t *ctx,
                      uint16_t file_no,
                      const char *file_name,
                      const uint8_t *buffer,
                      uint32_t len)
{
    FileInfo tFileInfo;
    int rc;
    uint16_t record_no = 0;
    uint32_t pos = 0;
    uint16_t send_len = 0;

    if (!ctx || !buffer) return -1;

    /* --- 1. 填充文件头信息 --- */
    memset(&tFileInfo, 0, sizeof(tFileInfo));

    /* 计算 CRC32 (针对文件内容 buffer) */
    /* 注意：CRC计算通常是针对原始内容，在大小端转换之前进行 */
 //   uint32_t file_crc = calc_crc32(buffer, len);

    /* 赋值并进行大小端转换 (Host to Network / Little to Big) */
   // tFileInfo.version   = swap_uint32(version);
   tFileInfo.file_len = len;
    tFileInfo.file_len  = swap_uint32(tFileInfo.file_len);
 //   tFileInfo.load_addr = swap_uint32(load_addr);
 //   tFileInfo.crc32     = swap_uint32(file_crc);

    /* 处理文件名 (安全拷贝) */
    if (file_name) {
        strncpy((char *)tFileInfo.file_name, file_name, sizeof(tFileInfo.file_name) - 1);
        /* 确保最后一位是 0，因为 strncpy 不保证 */
        tFileInfo.file_name[sizeof(tFileInfo.file_name) - 1] = 0;
    }

    /* --- 2. 发送文件头 (Record 0) --- */
    rc = modbus_write_file_record(ctx, file_no, record_no, (uint8_t *)&tFileInfo, sizeof(tFileInfo));
    if (rc < 0) {
        return rc; // 建议透传底层错误码
    }
    record_no++;

    /* --- 3. 循环发送文件内容 --- */
    while ( pos < len ) {
        send_len = len - pos;
        /* 保持 240 字节，配合 16字节对齐的 Flash 写入 */
        if (send_len > 240) {
            send_len = 240;
        }
        
        rc = modbus_write_file_record(ctx, file_no, record_no, (uint8_t *)(buffer + pos), send_len);
        if (rc < 0) {
            return -5; /* 传输中断 */
        }

        record_no++;
        pos += send_len;
    }

    return 0; /* 成功返回 0，符合标准 C 习惯 */
}

/**
 * @brief 从 Modbus 设备读取完整文件
 * * @param ctx       Modbus 上下文
 * @param file_no   文件编号
 * @param out_buf   [输出] 用于存储文件内容的缓冲区
 * @param max_len   [输入] out_buf 的最大容量，防止溢出
 * @param out_info  [输出] 读到的文件头信息 (版本、文件名等)
 * @return int      成功返回读取的文件总长度 (bytes)，失败返回 -1
 */
int modbus_read_file(modbus_t *ctx,
                     uint16_t file_no,
                     uint8_t *out_buf,
                     uint32_t max_len,
                     FileInfo *out_info)
{
    int rc;
    uint16_t record_no = 0;
    uint32_t total_read = 0;
    uint32_t file_len = 0;
    
    /* 临时存放 Header 的 buffer */
    FileInfo tHead; 
    
    if (!ctx || !out_buf || !out_info) {
        errno = EINVAL;
        return -1;
    }

    /* --- 1. 读取文件头 (Record 0) --- */
    /* FileInfo 大小为 32 字节，即 16 个寄存器(Word) */
    uint16_t header_words = sizeof(FileInfo) / 2;
    
    rc = modbus_read_file_record(ctx, file_no, record_no, (uint8_t *)&tHead, header_words);
    if (rc != sizeof(FileInfo)) {
        /* 读取长度不对，或者通信失败 */
        return -1; 
    }
    
    /* --- 2. 解析文件头 --- */
    /* 将网络字节序转为主机字节序 */
    out_info->version   = swap_uint32(tHead.version);
    out_info->file_len  = swap_uint32(tHead.file_len);
    out_info->load_addr = swap_uint32(tHead.load_addr);
    out_info->crc32     = swap_uint32(tHead.crc32);
    memcpy(out_info->file_name, tHead.file_name, sizeof(tHead.file_name));
    
    file_len = out_info->file_len;

    /* --- 3. 安全性检查 --- */
    /* 检查1: 文件长度是否超过用户提供的缓冲区 */
    if (file_len > max_len) {
        errno = ENOBUFS; // Buffer too small
        return -1;
    }
    /* 检查2: 文件长度是否合理 (例如不能是 0 或 巨大值) */
    if (file_len == 0) {
        return 0; // 空文件，直接返回
    }

    /* --- 4. 循环读取文件内容 --- */
    record_no++; // 指向 Record 1 (对应 write 中的 record_no++)
    
    while (total_read < file_len) {
        uint32_t remaining = file_len - total_read;
        uint16_t read_bytes = 0;
        uint16_t read_words = 0;

        /* 确定本次读取字节数，最大限制 240 字节 (120 words) */
        if (remaining > 240) {
            read_bytes = 240;
        } else {
            read_bytes = (uint16_t)remaining;
        }

        /* Modbus 读记录按 Word 计数，长度必须进位偶数 */
        read_words = (read_bytes + 1) / 2;

        /* 读取数据到 out_buf 的对应偏移位置 */
        rc = modbus_read_file_record(ctx, file_no, record_no, 
                                     out_buf + total_read, 
                                     read_words);
        
        if (rc < 0) {
            return -1; // 通信错误
        }

        /* * 注意：modbus_read_file_record 返回的是实际读到的字节数 (words * 2)
         * 但如果是最后一次读取，且剩余是奇数(例如3字节)，
         * 底层返回的可能是4字节。我们需要只保留实际需要的长度。
         */
        if (rc < read_bytes) {
            /* 读回来的数据比请求的少，异常 */
            errno = EIO;
            return -1;
        }

        /* 更新进度 */
        total_read += read_bytes;
        record_no++;
    }

    /* --- 5. CRC 校验 --- */
    uint32_t calc_crc = calc_crc32(out_buf, file_len);
    if (calc_crc != out_info->crc32) {
        errno = EBADE; // Invalid exchange / Checksum error
        /* * 这里是否返回 -1 取决于你的策略。
         * 严格模式返回 -1，宽容模式可以打印警告但返回长度。
         * 建议返回 -1 保证数据可靠性。
         */
        return -1; 
    }

    return (int)total_read;
}


int modbus_set_slave(modbus_t *ctx, int slave)
{
	if(NULL == ctx) {
		return -1;
	}
	return ctx->backend->set_slave(ctx,slave);
}

int modbus_receive(modbus_t *ctx, uint8_t *req)
{
	if(NULL == ctx) {
		return -1;
	}
	return ctx->backend->receive(ctx, req);
}


int modbus_set_response_timeout(modbus_t *ctx, uint32_t to_sec, uint32_t to_usec)
{
    if (ctx == NULL || (to_sec == 0 && to_usec == 0) || to_usec > 999999) {
        errno = EINVAL;
        return -1;
    }

    ctx->response_timeout.tv_sec = to_sec;
    ctx->response_timeout.tv_usec = to_usec;
    return 0;
}

modbus_mapping_t *modbus_mapping_new_start_address(unsigned int start_bits,
                                                   unsigned int nb_bits,
                                                   unsigned int start_input_bits,
                                                   unsigned int nb_input_bits,
                                                   unsigned int start_registers,
                                                   unsigned int nb_registers,
                                                   unsigned int start_input_registers,
                                                   unsigned int nb_input_registers)
{
	modbus_mapping_t *mb_mapping;
	mb_mapping = (modbus_mapping_t *)osal_malloc(sizeof(modbus_mapping_t));
	if(NULL == mb_mapping) {
		return NULL;
	}

	/* 0X */
	mb_mapping->nb_bits = nb_bits;
	mb_mapping->start_bits = start_bits;
	if(0 == nb_bits) {
		mb_mapping->tab_bits = NULL;
	} else {
		mb_mapping->tab_bits = (uint8_t *)osal_malloc(nb_bits * sizeof(uint8_t));
		if(NULL == mb_mapping->tab_bits) {
			osal_free(mb_mapping);
			return NULL;
		}
		memset(mb_mapping->tab_bits, 0, nb_bits * sizeof(uint8_t));
	}

	/* 1X */
	mb_mapping->nb_input_bits = nb_input_bits;
	mb_mapping->start_input_bits = start_input_bits;
	if(0 == nb_input_bits) {
		mb_mapping->tab_input_bits = NULL;
	} else {
		mb_mapping->tab_input_bits = (uint8_t *)osal_malloc(nb_input_bits * sizeof(uint8_t));
		if(NULL == mb_mapping->tab_input_bits) {
			osal_free(mb_mapping->tab_bits);
			osal_free(mb_mapping);
			return NULL;
		}
		memset(mb_mapping->tab_input_bits, 0, nb_input_bits * sizeof(uint8_t));
	}

	/* 4X */
	mb_mapping->nb_registers = nb_registers;
	mb_mapping->start_registers = start_registers;
	if(0 == nb_registers) {
		mb_mapping->tab_registers = NULL;
	} else {
		mb_mapping->tab_registers = (uint16_t *)osal_malloc(nb_registers * sizeof(uint16_t));
		if(NULL == mb_mapping->tab_registers) {
			osal_free(mb_mapping->tab_input_bits);
			osal_free(mb_mapping->tab_bits);
			osal_free(mb_mapping);
			return NULL;
		}
		memset(mb_mapping->tab_registers, 0, nb_registers * sizeof(uint16_t));
	}
	

	/* 3X */
	mb_mapping->nb_input_registers = nb_input_registers;
	mb_mapping->start_input_registers = start_input_registers;
	if(0 == nb_input_registers) {
		mb_mapping->tab_input_registers = NULL;
	} else {
		mb_mapping->tab_input_registers = (uint16_t *)osal_malloc(nb_input_registers * sizeof(uint16_t));
		if(NULL == mb_mapping->tab_input_registers) {
			osal_free(mb_mapping->tab_registers);
			osal_free(mb_mapping->tab_input_bits);
			osal_free(mb_mapping->tab_bits);
			osal_free(mb_mapping);
			return NULL;
		}
		memset(mb_mapping->tab_input_registers, 0, nb_input_registers * sizeof(uint16_t));
	}
	
	return mb_mapping;
}


int modbus_connect(modbus_t *ctx)
{
	if(NULL == ctx) {
		return -1;
	}
	return ctx->backend->connect(ctx);
}

void modbus_close(modbus_t *ctx)
{
	if(NULL == ctx) {
		return;
	}
	ctx->backend->close(ctx);
}

void modbus_free(modbus_t *ctx)
{
    if (ctx == NULL)
        return;

    ctx->backend->free(ctx);
}

/* Frees the 4 arrays */
void modbus_mapping_free(modbus_mapping_t *mb_mapping)
{
    if (mb_mapping == NULL) {
        return;
    }

    osal_free(mb_mapping->tab_input_registers);
    osal_free(mb_mapping->tab_registers);
    osal_free(mb_mapping->tab_input_bits);
    osal_free(mb_mapping->tab_bits);
    osal_free(mb_mapping);
}


