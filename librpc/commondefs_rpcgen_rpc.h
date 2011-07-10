#ifndef COMMONDEFS_RPCGEN_RPC_H
#define COMMONDEFS_RPCGEN_RPC_H

typedef bool_t rpc_boolean;

typedef u_long rpc_uint32;

typedef u_short rpc_uint16;

typedef u_char rpc_uint8;

typedef long rpc_int32;

typedef u_char rpc_byte;

typedef u_quad_t rpc_uint64;

int xdr_rpc_int32 (void *xdrs, void *objp);
int xdr_rpc_uint32 (void *xdrs, void *objp);
int xdr_rpc_uint64 (void *xdrs, void *objp);
int xdr_rpc_uint16 (void *xdrs, void *objp);
//int xdr_float (void *xdrs, void *objp);
//int xdr_double (void *xdrs, void *objp);
int xdr_rpc_boolean (void *xdrs, void *objp);


 
/*
typedef rpc_int32 rpc_loc_client_handle_type;

typedef rpc_uint64 rpc_loc_event_mask_type;

typedef rpc_uint64 rpc_loc_position_valid_mask_type;

typedef rpc_uint32 rpc_loc_pos_technology_mask_type;

typedef bool rpc_boolean;
typedef unsigned long rpc_uint32;

typedef unsigned short rpc_uint16;

typedef unsigned char rpc_uint8;

typedef long rpc_int32;

typedef unsigned char rpc_byte;
*/

#endif /* COMMONDEFS_RPCGEN_RPC_H */
