// example_client.c
//
/****************************************************************************
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "lscp/client.h"

#include <time.h>

#define SERVER_PORT 8888

#if defined(WIN32)
static WSADATA _wsaData;
#endif

////////////////////////////////////////////////////////////////////////

lscp_status_t client_callback ( lscp_client_t *pClient, const char *pchBuffer, int cchBuffer, void *pvData )
{
    lscp_status_t ret = LSCP_OK;

    char *pszBuffer = (char *) malloc(cchBuffer + 1);
    if (pszBuffer) {
        memcpy(pszBuffer, pchBuffer, cchBuffer);
        pszBuffer[cchBuffer] = (char) 0;
        printf("client_callback: [%s]\n", pszBuffer);
        free(pszBuffer);
    }
    else ret = LSCP_FAILED;

    return ret;
}

////////////////////////////////////////////////////////////////////////

void  client_test_start   ( clock_t *pclk ) { *pclk = clock(); }
float client_test_elapsed ( clock_t *pclk ) { return (float) ((long) clock() - *pclk) / (float) CLOCKS_PER_SEC; }

#define CLIENT_TEST(p, x) { clock_t c; void *v;\
    printf(#x ":\n"); client_test_start(&c); v = (void *) (x); \
    printf("  elapsed=%gs\n", client_test_elapsed(&c)); \
    printf("  ret=%p (%d)\n", v, (int) v); \
    printf("  errno=%d\n", lscp_client_get_errno(p)); \
    printf("  result=\"%s\"\n", lscp_client_get_result(p)); }

void client_test ( lscp_client_t *pClient )
{
    int iSamplerChannel;
    
    CLIENT_TEST(pClient, lscp_get_available_engines(pClient));
    CLIENT_TEST(pClient, lscp_get_engine_info(pClient, "DefaultEngine"));
    
    CLIENT_TEST(pClient, iSamplerChannel = lscp_get_channels(pClient)); iSamplerChannel++;
    CLIENT_TEST(pClient, lscp_add_channel(pClient));
    CLIENT_TEST(pClient, lscp_get_channel_info(pClient, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_load_engine(pClient, "DefaultEngine", iSamplerChannel));
    CLIENT_TEST(pClient, lscp_load_instrument(pClient, "DefaultInstrument.gig", 0, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_get_channel_voice_count(pClient, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_get_channel_stream_count(pClient, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_get_channel_buffer_fill(pClient, LSCP_USAGE_BYTES, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_get_channel_buffer_fill(pClient, LSCP_USAGE_PERCENTAGE, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_set_channel_audio_type(pClient, iSamplerChannel, LSCP_AUDIO_ALSA));
    CLIENT_TEST(pClient, lscp_set_channel_audio_channel(pClient, iSamplerChannel, 0));
    CLIENT_TEST(pClient, lscp_set_channel_midi_type(pClient, iSamplerChannel, LSCP_MIDI_ALSA));
    CLIENT_TEST(pClient, lscp_set_channel_midi_channel(pClient, iSamplerChannel, 0));
    CLIENT_TEST(pClient, lscp_set_channel_midi_port(pClient, iSamplerChannel, "130:0"));
    CLIENT_TEST(pClient, lscp_set_channel_volume(pClient, iSamplerChannel, 0.5));
    CLIENT_TEST(pClient, lscp_get_channel_info(pClient, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_reset_channel(pClient, iSamplerChannel));
    CLIENT_TEST(pClient, lscp_remove_channel(pClient, iSamplerChannel));
}

////////////////////////////////////////////////////////////////////////

void client_usage (void)
{
    printf("\n  %s %s (Build: %s)\n", lscp_client_package(), lscp_client_version(), lscp_client_build());
    
    fputs("\n  Available client commands: help, test, exit, quit, subscribe, unsubscribe", stdout);
    fputs("\n  (all else are sent verbatim to server)\n\n", stdout);
    
}

void client_prompt (void)
{
    fputs("lscp_client> ", stdout);
}

int main (int argc, char *argv[] )
{
    lscp_client_t *pClient;
    char *pszHost = "localhost";
    char  szLine[1024];
    int  cchLine;
    char  szResp[1024];
    int  cchResp;

#if defined(WIN32)
    if (WSAStartup(MAKEWORD(1, 1), &_wsaData) != 0) {
        fprintf(stderr, "lscp_client: WSAStartup failed.\n");
        return -1;
    }
#endif

    if (argc > 1)
        pszHost = argv[1];

    pClient = lscp_client_create(pszHost, SERVER_PORT, client_callback, NULL);
    if (pClient == NULL)
        return -1;

    client_usage();
    client_prompt();

    while (fgets(szLine, sizeof(szLine) - 3, stdin)) {
        
        cchLine = strlen(szLine);
        while (cchLine > 0 && (szLine[cchLine - 1] == '\n' || szLine[cchLine - 1] == '\r'))
            cchLine--;
        szLine[cchLine] = '\0';
        
        if (strcmp(szLine, "exit") == 0 || strcmp(szLine, "quit") == 0)
            break;
        else
        if (strcmp(szLine, "subscribe") == 0)
            lscp_client_subscribe(pClient);
        else
        if (strcmp(szLine, "unsubscribe") == 0)
            lscp_client_unsubscribe(pClient);
        else
        if (strcmp(szLine, "test") == 0)
            client_test(pClient);
        else
        if (cchLine > 0 && strcmp(szLine, "help") != 0) {
            szLine[cchLine++] = '\r';
            szLine[cchLine++] = '\n';
            szLine[cchLine]   = '\0';
            cchResp = sizeof(szResp) - 1;
            if (lscp_client_call(pClient, szLine, strlen(szLine), szResp, &cchResp) == LSCP_OK) {
                szResp[cchResp] = (char) 0;
                fputs(szResp, stdout);
            }
        }
        else client_usage();
        
        client_prompt();
    }

    lscp_client_destroy(pClient);

#if defined(WIN32)
    WSACleanup();
#endif

    return 0;
}

// end of example_client.c
