/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */

#ifdef SMB2

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <smb2/libsmb2.h>

struct smb2_share {
        struct smb2_share *next;
        struct smb2_context *smb2;
        char *name;
        const char *user;
        const char *password;
        char *url;
};

struct smb2_share *smb2_shares;

static char *
trim_str(char *str)
{
        while (*str == ' ') {
                str++;
        }
        while (strlen(str) > 0 && str[strlen(str) - 1] == ' ') {
                str[strlen(str) - 1] = 0;
        }
        return str;
}

static int
loadSMB2CNF(char *path)
{
        struct smb2_share *smb2_share = NULL;
        char buf[1024], *name, *value;
        FILE *fh;
        int len;

        if ((fh = fopen(path, "r")) == NULL) {
                return -1;
        }

        while (!feof(fh)) {
                buf[0] = 0;
                if (fgets(buf, sizeof(buf), fh) == NULL) {
                        break;
                }
                len = strlen(buf);
                if (len == 0) {
                        break;
                }
                buf[len - 1] = 0; /* clear trailing \n */
                if (buf[0] == '#') {
                        continue;
                }
                name = buf;
                value = strchr((const char *)name, '=');
                if (value == NULL) {
                        continue;
                }
                *value++ = 0;
                name = trim_str(name);
                value = trim_str(value);

                if (smb2_share == NULL) {
                        smb2_share = malloc(sizeof(struct smb2_share));
                }

                if (!strcmp(name, "NAME")) {
                        smb2_share->name = strdup(value);
                } else if (!strcmp(name, "USERNAME")) {
                        smb2_share->user = strdup(value);
                } else if (!strcmp(name, "PASSWORD")) {
                        smb2_share->password = strdup(value);
                } else if (!strcmp(name, "URL")) {
                        struct smb2_context *smb2;
                        struct smb2_url *url;

                        smb2_share->url = strdup(value);
                        smb2_share->smb2 = smb2 = smb2_init_context();
                        smb2_set_user(smb2, smb2_share->user);
                        smb2_set_password(smb2, smb2_share->password);
                        url = smb2_parse_url(smb2, smb2_share->url);
                        if (smb2_connect_share(smb2, url->server, url->share,
                                               smb2_share->user) < 0) {
                                /* FIXME log the error somewhere? */
                                smb2_destroy_url(url);
                                smb2_destroy_context(smb2);
                                free(smb2_share);
                                smb2_share = NULL;
                                continue;
                        }
                        smb2_destroy_url(url);

                        if (smb2_shares == NULL) {
                                smb2_shares = smb2_share;
                        } else {
                                smb2_shares->next = smb2_shares;
                                smb2_shares = smb2_share;
                                smb2_share = NULL;
                        }
                }
        }
	fclose(fh);
	return 0;
}

int init_smb2(void)
{
        struct smb2_context *smb2;

        /* FIXME: find a better path for this */
	loadSMB2CNF("/dev_hdd0/SMB2.CNF");

        return 0;
}

#endif /* SMB2 */
