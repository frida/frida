/*
* Copyright (C) 2015 Eloi Vanderbeken <eloi.vanderbeken@synacktiv.com>
*
* Licence: wxWindows Library Licence, Version 3.1
*/

#ifndef __GUM_TLS_PRIV_H__
#define __GUM_TLS_PRIV_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void _gum_tls_init (void);
G_GNUC_INTERNAL void _gum_tls_realize (void);
G_GNUC_INTERNAL void _gum_tls_deinit (void);

G_END_DECLS

#endif
