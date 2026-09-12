// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_STATUS_H
#define TTX_DATA_STATUS_H

#include "perimortem/core/perimortem.h"

// Data starts with completed descriptions, but matching those descriptions or
// fulfilling an access can still fail. These statuses report that concrete
// outcome to the owning operation. An unresolved semantic question must be
// settled before it becomes a Data schema, rather than represented as an
// unknown size or layout inside the transport.
typedef U8 ttx_data_status;
#define TTX_DATA_SUCCESS ((ttx_data_status)0)
#define TTX_DATA_INVALID ((ttx_data_status)1)
#define TTX_DATA_BOUNDS ((ttx_data_status)2)
#define TTX_DATA_OVERFLOW ((ttx_data_status)3)
#define TTX_DATA_INCOMPATIBLE ((ttx_data_status)4)
#define TTX_DATA_UNSUPPORTED ((ttx_data_status)5)
#define TTX_DATA_BUSY ((ttx_data_status)6)
#define TTX_DATA_DENIED ((ttx_data_status)7)
#define TTX_DATA_IO_ERROR ((ttx_data_status)8)

#endif
