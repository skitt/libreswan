/* misc functions to get compile time and runtime options
 * Copyright (C) 2005 Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 2009 Avesh Agarwal <avagarwa@redhat.com>
 * Copyright (C) 2012 Paul Wouters <paul@libreswan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include "lswlog.h"
#include "lswconf.h"
#include "lswalloc.h"

#include <string.h>
#include <nss.h>
#include <nspr.h>
#include <pk11pub.h>

static struct lsw_conf_options global_oco;
static bool setup=FALSE;

#define NSSpwdfilesize 4096
static secuPWData NSSPassword;

#ifdef SINGLE_CONF_DIR
#define SUBDIRNAME(X) ""
#else
#define SUBDIRNAME(X) X
#endif

static void lsw_conf_calculate(struct lsw_conf_options *oco)
{
    char buf[PATH_MAX];

    /* calculate paths to certain subdirs */
    snprintf(buf, sizeof(buf), "%s" SUBDIRNAME("/acerts"), oco->confddir);
    oco->acerts_dir = clone_str(buf, "acert path");

    snprintf(buf, sizeof(buf), "%s" SUBDIRNAME("/cacerts"), oco->confddir);
    oco->cacerts_dir = clone_str(buf, "cacert path");

    snprintf(buf, sizeof(buf), "%s" SUBDIRNAME("/crls"), oco->confddir);
    oco->crls_dir = clone_str(buf, "crls path");

    snprintf(buf, sizeof(buf), "%s" SUBDIRNAME("/private"), oco->confddir);
    oco->private_dir = clone_str(buf, "private path");

    snprintf(buf, sizeof(buf), "%s" SUBDIRNAME("/certs"), oco->confddir);
    oco->certs_dir = clone_str(buf, "certs path");

    snprintf(buf, sizeof(buf), "%s" SUBDIRNAME("/aacerts"), oco->confddir);
    oco->aacerts_dir = clone_str(buf, "aacerts path");

    snprintf(buf, sizeof(buf), "%s/policies", oco->confddir);
    oco->policies_dir = clone_str(buf, "policies path");
}

void lsw_conf_setdefault(void)
{
    char buf[PATH_MAX];
    char *ipsec_conf_dir = FINALCONFDIR;
    char *ipsecd_dir = FINALCONFDDIR;
    char *conffile   = FINALCONFFILE;
    char *var_dir    = FINALVARDIR;
#if 0
    char *exec_dir   = FINALLIBEXECDIR;
    char *lib_dir    = FINALLIBDIR;
    char *sbin_dir   = FINALSBINDIR;
#endif

    memset(&global_oco, 0, sizeof(global_oco));

    /* allocate them all to make it consistent */
    ipsec_conf_dir = clone_str(ipsec_conf_dir, "default conf ipsec_conf_dir");
    ipsecd_dir = clone_str(ipsecd_dir, "default conf ipsecd_dir");
    conffile   = clone_str(conffile, "default conf conffile");
    var_dir    = clone_str(var_dir, "default conf var_dir");
    
    global_oco.rootdir = "";
    global_oco.confddir= ipsecd_dir;
    global_oco.vardir  = var_dir;
    global_oco.confdir = ipsec_conf_dir;
    global_oco.conffile = conffile;

    /* path to NSS password file */
    snprintf(buf, sizeof(buf), "%s/nsspassword", global_oco.confddir);
    NSSPassword.data = clone_str(buf, "nss password file path");
    NSSPassword.source =  PW_FROMFILE;
    /* DBG_log("default setting of ipsec.d to %s", global_oco.confddir); */
}

/* mostly estatic value, to surpress within LEAK_DETECTIVE */
void lsw_conf_free_oco(void)
{
    /* Must be a nicer way to loop over this? */
    pfree(global_oco.crls_dir);
    /* pfree(global_oco.rootdir); */
    pfree(global_oco.confdir); /* there is one more alloc that did not get freed? */
    pfree(global_oco.conffile);
    pfree(global_oco.confddir);
    pfree(global_oco.vardir);
    pfree(global_oco.policies_dir);
    pfree(global_oco.acerts_dir);
    pfree(global_oco.cacerts_dir);
    // wrong leak magic? pfree(global_oco.crls_dir);
    pfree(global_oco.private_dir);
    pfree(global_oco.certs_dir);
    pfree(global_oco.aacerts_dir);
}

const struct lsw_conf_options *lsw_init_options(void)
{
    if(setup) return &global_oco;
    setup = TRUE;

    lsw_conf_setdefault();
    lsw_conf_calculate(&global_oco);

    return &global_oco;
}

const struct lsw_conf_options *lsw_init_rootdir(const char *root_dir)
{
    if(!setup) lsw_conf_setdefault();
    global_oco.rootdir = clone_str(root_dir, "override /");
    lsw_conf_calculate(&global_oco);
    setup = TRUE;
    
    return &global_oco;
}

const struct lsw_conf_options *lsw_init_ipsecdir(const char *ipsec_dir)
{
    if(!setup) lsw_conf_setdefault();
    global_oco.confddir = clone_str(ipsec_dir, "override ipsec.d");
    lsw_conf_calculate(&global_oco);
    setup = TRUE;

    libreswan_log("adjusting ipsec.d to %s", global_oco.confddir);

    return &global_oco;
}

secuPWData *lsw_return_nss_password_file_info(void)
{
    return &NSSPassword;
}

bool Pluto_IsFIPS(void)
{
     char fips_flag[1];
     int n;
     FILE *fd=fopen("/proc/sys/crypto/fips_enabled","r");
     
     if(fd!=NULL) {
	    n = fread ((void *)fips_flag, 1, 1, fd);
		if(n==1) {
		    if(fips_flag[0]=='1') {
		    fclose(fd);
		    return TRUE;
		    }
		    else {
		    libreswan_log("Non-fips mode set in /proc/sys/crypto/fips_enabled");
		    }
		} else {
			libreswan_log("error in reading /proc/sys/crypto/fips_enabled, returning non-fips mode");
		} 
     fclose(fd);
     }
     else {
	libreswan_log("Not able to open /proc/sys/crypto/fips_enabled, returning non-fips mode"); 
     }
return FALSE;
}

char *getNSSPassword(PK11SlotInfo *slot, PRBool retry, void *arg)
{
     secuPWData *pwdInfo = (secuPWData *)arg;
     PRFileDesc *fd;
     PRInt32 nb; /*number of bytes*/
     char* password; 
     char* strings;
     char* token=NULL;
     const long maxPwdFileSize = NSSpwdfilesize;
     int i, tlen=0;

     if (slot) {
     token = PK11_GetTokenName(slot);
         if (token) {
         tlen = PORT_Strlen(token);
	 /* libreswan_log("authentication needed for token name %s with length %d",token,tlen); */
         }
	 else {
		return 0;
	 }
     }
     else {
	return 0;
     }

     if(retry) return 0;

     strings=PORT_ZAlloc(maxPwdFileSize);
     if(!strings) {
     libreswan_log("Not able to allocate memory for reading NSS password file");
     return 0;
     }

     
     if(pwdInfo->source == PW_FROMFILE) {
     	if(pwdInfo->data !=NULL) {
            fd = PR_Open(pwdInfo->data, PR_RDONLY, 0);
	    if (!fd) {
	    PORT_Free(strings);
	    libreswan_log("No password file \"%s\" exists.", pwdInfo->data);
	    return 0;
	    }

	    nb = PR_Read(fd, strings, maxPwdFileSize);
	    PR_Close(fd);

	    if (nb == 0) {
            libreswan_log("password file contains no data");
            PORT_Free(strings);
            return 0;
            }

            i = 0;
            do
            {
               int start = i;
               int slen;

               while (strings[i] != '\r' && strings[i] != '\n' && i < nb) i++;
               strings[i++] = '\0';

               while ( (i<nb) && (strings[i] == '\r' || strings[i] == '\n')) {
               strings[i++] = '\0';
               }

               password = &strings[start];

               if (PORT_Strncmp(password, token, tlen)) continue;
               slen = PORT_Strlen(password);

               if (slen < (tlen+1)) continue;
               if (password[tlen] != ':') continue;
               password = &password[tlen+1];
               break;

            } while (i<nb);

            password = PORT_Strdup((char*)password);
            PORT_Free(strings);

            /* libreswan_log("Password passed to NSS is %s with length %d", password, PORT_Strlen(password)); */
            return password;
	}
	else {
	libreswan_log("File with Password to NSS DB is not provided");
        return 0;
	}
     }

libreswan_log("nss password source is not specified as file");
return 0;
}
    
/*
 * Local Variables:
 * c-basic-offset:4
 * c-style: pluto
 * End:
 */
