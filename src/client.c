// client.c
//
/****************************************************************************
   liblscp - LinuxSampler Control Protocol API
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

#include "lscp/client.h"

#include <ctype.h>


// Maximum number of engines as allocation lscp_get_available_engines.
#define LSCP_MAX_ENGINES    8


// Local prototypes.

static const char * _lscp_ltrim                 (const char *psz);

static void         _lscp_client_set_result     (lscp_client_t *pClient, const char *pszResult, int iErrno);

static void         _lscp_engine_info_init      (lscp_engine_info_t *pEngineInfo);
static void         _lscp_engine_info_reset     (lscp_engine_info_t *pEngineInfo);

static void         _lscp_channel_info_init     (lscp_channel_info_t *pChannelInfo);
static void         _lscp_channel_info_reset    (lscp_channel_info_t *pChannelInfo);

static void         _lscp_client_udp_proc       (void *pvClient);


//-------------------------------------------------------------------------
// strtok_r - needed in win32 for parsing results.
#if defined(WIN32)
char *strtok_r ( char *pchBuffer, const char *pszDelim, char **ppch )
{
    char *pszToken;

    if (pchBuffer == NULL)
        pchBuffer = *ppch;

    pchBuffer += strspn(pchBuffer, pszDelim);
    if (*pchBuffer == '\0')
        return NULL;

    pszToken  = pchBuffer;
    pchBuffer = strpbrk(pszToken, pszDelim);
    if (pchBuffer == NULL) {
        *ppch = strchr(pszToken, '\0');
    } else {
        *pchBuffer = '\0';
        *ppch = pchBuffer + 1;
    }

    return pszToken;
}
#endif


//-------------------------------------------------------------------------
// Helper functions.

// Trimming left spaces...
static const char *_lscp_ltrim ( const char *psz )
{
    while (isspace(*psz))
        psz++;
    return psz;
}

// Result buffer internal settler.
static void _lscp_client_set_result ( lscp_client_t *pClient, const char *pszResult, int iErrno )
{
    if (pClient->pszResult)
        free(pClient->pszResult);
    pClient->pszResult = NULL;

    pClient->iErrno = iErrno;

    if (pszResult)
        pClient->pszResult = strdup(_lscp_ltrim(pszResult));
}


// Engine info struct cache member.
static void _lscp_engine_info_init ( lscp_engine_info_t *pEngineInfo )
{
    pEngineInfo->description = NULL;
    pEngineInfo->version     = NULL;
}

static void _lscp_engine_info_reset ( lscp_engine_info_t *pEngineInfo )
{
    if (pEngineInfo->description)
        free(pEngineInfo->description);
    if (pEngineInfo->version)
        free(pEngineInfo->version);

    _lscp_engine_info_init(pEngineInfo);
}


// Channel info struct cache member.
static void _lscp_channel_info_init ( lscp_channel_info_t *pChannelInfo )
{
    pChannelInfo->engine_name   = NULL;
    pChannelInfo->audio_type    = LSCP_AUDIO_NONE;
    pChannelInfo->audio_channel = LSCP_CHANNEL_INVALID;
    pChannelInfo->instrument    = NULL;
    pChannelInfo->midi_type     = LSCP_MIDI_NONE;
    pChannelInfo->midi_port     = NULL;
    pChannelInfo->midi_channel  = LSCP_CHANNEL_INVALID;
    pChannelInfo->volume        = 0.0;
}

static void _lscp_channel_info_reset ( lscp_channel_info_t *pChannelInfo )
{
    if (pChannelInfo->engine_name)
        free(pChannelInfo->engine_name);
    if (pChannelInfo->instrument)
        free(pChannelInfo->instrument);
    if (pChannelInfo->midi_port)
        free(pChannelInfo->midi_port);

    _lscp_channel_info_init(pChannelInfo);
}


//-------------------------------------------------------------------------
// UDP service (datagram oriented).

static void _lscp_client_udp_proc ( void *pvClient )
{
    lscp_client_t *pClient = (lscp_client_t *) pvClient;
    struct sockaddr_in addr;
    int   cAddr;
    char  achBuffer[LSCP_BUFSIZ];
    int   cchBuffer;
    const char *pszSeps = " \r\n";
    char *pszToken;
    char *pch;

#ifdef DEBUG
    fprintf(stderr, "_lscp_client_udp_proc: Client waiting for events.\n");
#endif

    while (pClient->udp.iState) {
        cAddr = sizeof(struct sockaddr_in);
        cchBuffer = recvfrom(pClient->udp.sock, achBuffer, sizeof(achBuffer), 0, (struct sockaddr *) &addr, &cAddr);
        if (cchBuffer > 0) {
#ifdef DEBUG
            lscp_socket_trace("_lscp_client_udp_proc: recvfrom", &addr, achBuffer, cchBuffer);
#endif
            if (strncmp(achBuffer, "PING ", 5) == 0) {
                // Make sure received buffer it's null terminated.
                achBuffer[cchBuffer] = (char) 0;
                strtok_r(achBuffer, pszSeps, &(pch));       // Skip "PING"
                strtok_r(NULL, pszSeps, &(pch));            // Skip port (must be the same as in addr)
                pszToken = strtok_r(NULL, pszSeps, &(pch)); // Have session-id.
                if (pszToken) {
                    // Set now client's session-id, if not already
                    if (pClient->sessid == NULL)
                        pClient->sessid = strdup(pszToken);
                    if (pClient->sessid && strcmp(pszToken, pClient->sessid) == 0) {
                        snprintf(achBuffer, sizeof(achBuffer) - 1, "PONG %s\r\n", pClient->sessid);
                        cchBuffer = strlen(achBuffer);
                        if (sendto(pClient->udp.sock, achBuffer, cchBuffer, 0, (struct sockaddr *) &addr, cAddr) < cchBuffer)
                            lscp_socket_perror("_lscp_client_udp_proc: sendto");
#ifdef DEBUG
                        fprintf(stderr, "> %s", achBuffer);
#endif
                    }
                }
                // Done with life proof.
            } else {
                //
                if ((*pClient->pfnCallback)(
                        pClient,
                        achBuffer,
                        cchBuffer,
                        pClient->pvData) != LSCP_OK) {
                    pClient->udp.iState = 0;
                }
            }
        } else {
            lscp_socket_perror("_lscp_client_udp_proc: recvfrom");
            pClient->udp.iState = 0;
        }
    }

#ifdef DEBUG
    fprintf(stderr, "_lscp_client_udp_proc: Client closing.\n");
#endif
}


//-------------------------------------------------------------------------
// Client versioning teller fuunction.


/** Retrieve the current client library version string. */
const char* lscp_client_package (void) { return LSCP_PACKAGE; }

/** Retrieve the current client library version string. */
const char* lscp_client_version (void) { return LSCP_VERSION; }

/** Retrieve the current client library build timestamp string. */
const char* lscp_client_build   (void) { return __DATE__ " " __TIME__; }


//-------------------------------------------------------------------------
// Client socket functions.


/**
 *  Create a client instance, estabilishing a connection to a server hostname,
 *  which must be listening on the given port. A client callback function is
 *  also supplied for server notification event handling.
 */
lscp_client_t* lscp_client_create ( char *pszHost, int iPort, lscp_client_proc_t pfnCallback, void *pvData )
{
    lscp_client_t  *pClient;
    struct hostent *pHost;
    lscp_socket_t sock;
    struct sockaddr_in addr;
    int cAddr;
    int iSockOpt = (-1);

    if (pfnCallback == NULL) {
        fprintf(stderr, "lscp_client_create: Invalid client callback function.\n");
        return NULL;
    }

    pHost = gethostbyname(pszHost);
    if (pHost == NULL) {
        lscp_socket_perror("lscp_client_create: gethostbyname");
        return NULL;
    }

    // Allocate client descriptor...

    pClient = (lscp_client_t *) malloc(sizeof(lscp_client_t));
    if (pClient == NULL) {
        fprintf(stderr, "lscp_client_create: Out of memory.\n");
        return NULL;
    }
    memset(pClient, 0, sizeof(lscp_client_t));

    pClient->pfnCallback = pfnCallback;
    pClient->pvData = pvData;

#ifdef DEBUG
    fprintf(stderr, "lscp_client_create: pClient=%p: pszHost=%s iPort=%d.\n", pClient, pszHost, iPort);
#endif

    // Prepare the TCP connection socket...

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        lscp_socket_perror("lscp_client_create: tcp: socket");
        free(pClient);
        return NULL;
    }

#if defined(WIN32)
    if (setsockopt(sock, SOL_SOCKET, SO_DONTLINGER, (char *) &iSockOpt, sizeof(int)) == SOCKET_ERROR)
        lscp_socket_perror("lscp_client_create: tcp: setsockopt(SO_DONTLINGER)");
#endif

#ifdef DEBUG
    lscp_socket_getopts("lscp_client_create: tcp", sock);
#endif

    cAddr = sizeof(struct sockaddr_in);
    memset((char *) &addr, 0, cAddr);
    addr.sin_family = pHost->h_addrtype;
    memmove((char *) &(addr.sin_addr), pHost->h_addr, pHost->h_length);
    addr.sin_port = htons((short) iPort);

    if (connect(sock, (struct sockaddr *) &addr, cAddr) == SOCKET_ERROR) {
        lscp_socket_perror("lscp_client_create: tcp: connect");
        closesocket(sock);
        free(pClient);
        return NULL;
    }

    lscp_socket_agent_init(&(pClient->tcp), sock, &addr, cAddr);

#ifdef DEBUG
    fprintf(stderr, "lscp_client_create: tcp: pClient=%p: sock=%d addr=%s port=%d.\n", pClient, pClient->tcp.sock, inet_ntoa(pClient->tcp.addr.sin_addr), ntohs(pClient->tcp.addr.sin_port));
#endif

    // Prepare the UDP datagram service socket...

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        lscp_socket_perror("lscp_client_create: udp: socket");
        lscp_socket_agent_free(&(pClient->tcp));
        free(pClient);
        return NULL;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &iSockOpt, sizeof(int)) == SOCKET_ERROR)
        lscp_socket_perror("lscp_client_create: udp: setsockopt(SO_REUSEADDR)");

#ifdef DEBUG
    lscp_socket_getopts("lscp_client_create: udp", sock);
#endif

    cAddr = sizeof(struct sockaddr_in);
    memset((char *) &addr, 0, cAddr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);

    if (bind(sock, (const struct sockaddr *) &addr, cAddr) == SOCKET_ERROR) {
        lscp_socket_perror("lscp_client_create: udp: bind");
        lscp_socket_agent_free(&(pClient->tcp));
        closesocket(sock);
        free(pClient);
        return NULL;
    }

    if (getsockname(sock, (struct sockaddr *) &addr, &cAddr) == SOCKET_ERROR) {
        lscp_socket_perror("lscp_client_create: udp: getsockname");
        lscp_socket_agent_free(&(pClient->tcp));
        closesocket(sock);
        free(pClient);
        return NULL;
    }

    lscp_socket_agent_init(&(pClient->udp), sock, &addr, cAddr);

#ifdef DEBUG
    fprintf(stderr, "lscp_client_create: udp: pClient=%p: sock=%d addr=%s port=%d.\n", pClient, pClient->udp.sock, inet_ntoa(pClient->udp.addr.sin_addr), ntohs(pClient->udp.addr.sin_port));
#endif

    // No session id, yet.
    pClient->sessid = NULL;
    // Initialize cached members.
    pClient->engines = NULL;
    pClient->iMaxEngines = 0;
    _lscp_engine_info_init(&(pClient->engine_info));
    _lscp_channel_info_init(&(pClient->channel_info));
    // Initialize error stuff.
    pClient->pszResult = NULL;
    pClient->iErrno = -1;
    // Stream usage stuff.
    pClient->buffer_fill = NULL;
    pClient->iStreamCount = 0;

    // Now's finally time to startup threads...
    
    // UDP service thread...
    if (lscp_socket_agent_start(&(pClient->udp), _lscp_client_udp_proc, pClient, 0) != LSCP_OK) {
        lscp_socket_agent_free(&(pClient->tcp));
        lscp_socket_agent_free(&(pClient->udp));
        free(pClient);
        return NULL;
    }

    // Finally we've some success...
    return pClient;
}


/**
 *  Wait for a client instance to terminate graciously.
 */
lscp_status_t lscp_client_join ( lscp_client_t *pClient )
{
    if (pClient == NULL)
        return LSCP_FAILED;

#ifdef DEBUG
    fprintf(stderr, "lscp_client_join: pClient=%p.\n", pClient);
#endif

//  lscp_socket_agent_join(&(pClient->udp));
    lscp_socket_agent_join(&(pClient->tcp));

    return LSCP_OK;
}


/**
 *  Terminate and destroy a client instance.
 */
lscp_status_t lscp_client_destroy ( lscp_client_t *pClient )
{
    int i;
    
    if (pClient == NULL)
        return LSCP_FAILED;

#ifdef DEBUG
    fprintf(stderr, "lscp_client_destroy: pClient=%p.\n", pClient);
#endif

    // Free session-id, if any.
    if (pClient->sessid)
        free(pClient->sessid);
    pClient->sessid = NULL;
    // Free up all cached members.
    _lscp_channel_info_reset(&(pClient->channel_info));
    _lscp_engine_info_reset(&(pClient->engine_info));
    // Free available engine table.
    if (pClient->engines) {
        for (i = 0; i < pClient->iMaxEngines; i++) {
            if (pClient->engines[i])
                free(pClient->engines[i]);
        }
        free(pClient->engines);
    }
    pClient->engines = NULL;
    // Free result error stuff.
    _lscp_client_set_result(pClient, NULL, 0);
    // Frre stream usage stuff.
    if (pClient->buffer_fill)
        free(pClient->buffer_fill);
    pClient->buffer_fill = NULL;
    pClient->iStreamCount = 0;

    // Free socket agents.
    lscp_socket_agent_free(&(pClient->udp));
    lscp_socket_agent_free(&(pClient->tcp));

    free(pClient);

    return LSCP_OK;
}


/**
 *  Submit a raw request to the connected server and store it's response.
 */
lscp_status_t lscp_client_call ( lscp_client_t *pClient, const char *pchBuffer, int cchBuffer, char *pchResult, int *pcchResult )
{
    lscp_status_t ret = LSCP_FAILED;

    if (pClient == NULL)
        return ret;
    if (pchBuffer == NULL || cchBuffer < 1)
        return ret;
    if (pchResult == NULL || pcchResult == NULL)
        return ret;
    if (*pcchResult < 1)
        return ret;

    if (send(pClient->tcp.sock, pchBuffer, cchBuffer, 0) < cchBuffer) {
        lscp_socket_perror("lscp_client_call: send");
    } else {
        *pcchResult = recv(pClient->tcp.sock, pchResult, *pcchResult, 0);
        if (*pcchResult < 1)
            lscp_socket_perror("lscp_client_call: recv");
        else
            ret = LSCP_OK;
    }
    
    return ret;
}


//-------------------------------------------------------------------------
// Client common protocol functions.


/**
 *  Submit a command query string to the server. The query string must be
 *  cr/lf and null terminated.
 */
lscp_status_t lscp_client_query ( lscp_client_t *pClient, const char *pszQuery )
{
    lscp_status_t ret = LSCP_FAILED;
    char  achResult[LSCP_BUFSIZ];
    int   cchResult;
    const char *pszSeps = ":";
    const char *pszResult;
    char *pszToken;
    char *pch;
    int   iErrno;

    if (pClient == NULL)
        return ret;

    pszResult = NULL;
    iErrno = -1;

    // Do the socket transaction...
    cchResult = sizeof(achResult);
    ret = lscp_client_call(pClient, pszQuery, strlen(pszQuery), achResult, &cchResult);
    if (ret == LSCP_OK) {
        // Always force the result to be null terminated (and trim trailing CRLF)!
        while (cchResult > 0 && (achResult[cchResult - 1] == '\n' || achResult[cchResult- 1] == '\r'))
            cchResult--;
        achResult[cchResult] = (char) 0;
        // Check if the response is an error or warning message.
        if (strncmp(achResult, "ERR:", 4) == 0)
            ret = LSCP_ERROR;
        else if (strncmp(achResult, "WRN:", 4) == 0)
            ret = LSCP_WARNING;
        // So we got a result...
        if (ret == LSCP_OK) {
            pszResult = achResult;
            iErrno = 0;
        } else {
            // Parse the error/warning message, skip first colon...
            pszToken = strtok_r(achResult, pszSeps, &(pch));
            if (pszToken) {
                // Get the error number...
                pszToken = strtok_r(NULL, pszSeps, &(pch));
                if (pszToken) {
                    iErrno = atoi(pszToken);
                    // And make the message text our final result.
                    pszResult = strtok_r(NULL, pszSeps, &(pch));
                }
            }
        }
    }

    // Make the result official...
    _lscp_client_set_result(pClient, pszResult, iErrno);

    return ret;
}


/**
 *  Get the last received result string. In case of error or warning,
 *  this is the text of the error or warning message issued.
 */
const char *lscp_client_get_result ( lscp_client_t *pClient )
{
    if (pClient == NULL)
        return NULL;

    return pClient->pszResult;
}


/**
 *  Get the last error/warning number received.
 */
int lscp_client_get_errno ( lscp_client_t *pClient )
{
    if (pClient == NULL)
        return -1;

    return pClient->iErrno;
}


//-------------------------------------------------------------------------
// Client registration protocol functions.

/**
 *  Register frontend for receiving UDP event messages:
 *  SUBSCRIBE NOTIFICATION <udp-port>
 */
lscp_status_t lscp_client_subscribe ( lscp_client_t *pClient )
{
    lscp_status_t ret;
    char szQuery[LSCP_BUFSIZ];
    const char *pszResult;
    const char *pszSeps = "[]";
    char *pszToken;
    char *pch;

    if (pClient == NULL || pClient->sessid)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "SUBSCRIBE NOTIFICATION %d\r\n", ntohs(pClient->udp.addr.sin_port));
    ret = lscp_client_query(pClient, szQuery);
    if (ret == LSCP_OK) {
        pszResult = lscp_client_get_result(pClient);
#ifdef DEBUG
        fprintf(stderr, "lscp_client_subscribe: %s\n", pszResult);
#endif
        // Check for the session-id on "OK[sessid]" response.
        pszToken = strtok_r((char *) pszResult, pszSeps, &(pch));
        if (pszToken && strcmp(pszToken, "OK") == 0) {
            pszToken = strtok_r(NULL, pszSeps, &(pch));
            if (pszToken)
                pClient->sessid = strdup(pszToken);
        }
    }
    
    return ret;
}


/**
 *  Deregister frontend for not receiving UDP event messages anymore:
 *  UNSUBSCRIBE NOTIFICATION <session-id>
 */
lscp_status_t lscp_client_unsubscribe ( lscp_client_t *pClient )
{
    lscp_status_t ret;
    char szQuery[LSCP_BUFSIZ];

    if (pClient == NULL)
        return LSCP_FAILED;
    if (pClient->sessid == NULL)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "UNSUBSCRIBE NOTIFICATION %s\n\n", pClient->sessid);
    ret = lscp_client_query(pClient, szQuery);
    if (ret == LSCP_OK) {
#ifdef DEBUG
        fprintf(stderr, "lscp_client_unsubscribe: %s\n", lscp_client_get_result(pClient));
#endif
        // Bail out session-id string.
        free(pClient->sessid);
        pClient->sessid = NULL;
    }
    
    return ret;
}


//-------------------------------------------------------------------------
// Client command protocol functions.

/**
 *  Loading an instrument:
 *  LOAD INSTRUMENT <filename> <instr-index> <sampler-channel>
 */
lscp_status_t lscp_load_instrument ( lscp_client_t *pClient, const char *pszFileName, int iInstrIndex, int iSamplerChannel )
{
    char szQuery[LSCP_BUFSIZ];
    
    if (pszFileName == NULL || iSamplerChannel < 0)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "LOAD INSTRUMENT %s %d %d\r\n", pszFileName, iInstrIndex, iSamplerChannel);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Loading a sampler engine:
 *  LOAD ENGINE <engine-name> <sampler-channel>
 */
lscp_status_t lscp_load_engine ( lscp_client_t *pClient, const char *pszEngineName, int iSamplerChannel )
{
    char szQuery[LSCP_BUFSIZ];

    if (pszEngineName == NULL || iSamplerChannel < 0)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "LOAD ENGINE %s %d\r\n", pszEngineName, iSamplerChannel);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Current number of sampler channels:
 *  GET CHANNELS
 */
int lscp_get_channels ( lscp_client_t *pClient )
{
    int iChannels = -1;
    if (lscp_client_query(pClient, "GET CHANNELS\r\n") == LSCP_OK)
        iChannels = atoi(lscp_client_get_result(pClient));
    return iChannels;
}


/**
 *  Adding a new sampler channel:
 *  ADD CHANNEL
 */
lscp_status_t lscp_add_channel ( lscp_client_t *pClient )
{
    return lscp_client_query(pClient, "ADD CHANNEL\r\n");
}


/**
 *  Removing a sampler channel:
 *  REMOVE CHANNEL <sampler-channel>
 */
lscp_status_t lscp_remove_channel ( lscp_client_t *pClient, int iSamplerChannel )
{
    char szQuery[LSCP_BUFSIZ];

    if (iSamplerChannel < 0)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "REMOVE CHANNEL %d\r\n", iSamplerChannel);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Getting all available engines:
 *  GET AVAILABLE_ENGINES
 */
const char **lscp_get_available_engines ( lscp_client_t *pClient )
{
    const char *pszResult;
    const char  *pszCrlf = "\r\n";
    char **ppEngines = NULL;
    int    iEngines = 0;
    int    iMaxEngines, i;
    char  *pszToken;
    char  *pch;

    if (lscp_client_query(pClient, "GET AVAILABLE_ENGINES\r\n") == LSCP_OK) {
        pszResult = lscp_client_get_result(pClient);
        ppEngines = pClient->engines;
        pszToken = strtok_r((char *) pszResult, pszCrlf, &(pch));
        while (pszToken) {
            if (*pszToken) {
                // Grow current engine table?
                if (iEngines >= pClient->iMaxEngines) {
                    iMaxEngines = pClient->iMaxEngines + LSCP_MAX_ENGINES;
                    ppEngines = (char **) malloc(iMaxEngines * sizeof(char *));
                    if (ppEngines == NULL)
                        break;
                    // Initialize table, and copy old contents into it if any.
                    for (i = 0; i < iMaxEngines; i++) {
                        if (pClient->engines && i < iEngines)
                            ppEngines[i] = pClient->engines[i];
                        else
                            ppEngines[i] = NULL;
                    }
                    // Free old table.
                    if (pClient->engines)
                        free(pClient->engines);
                    // Set new one.
                    pClient->iMaxEngines = iMaxEngines;
                    pClient->engines = ppEngines;

                }
                // Free old entry.
                if (ppEngines[iEngines])
                    free(ppEngines[iEngines]);
                // Set new one.
                ppEngines[iEngines] = strdup(pszToken);
                iEngines++;
            }
            pszToken = strtok_r(NULL, pszCrlf, &(pch));
        }
        // Don't forget to have a null termination.
        if (ppEngines)
            ppEngines[iEngines] = NULL;
    }

    return (const char **) ppEngines;
}


/**
 *  Getting information about an engine.
 *  GET ENGINE INFO <engine-name>
 */
lscp_engine_info_t *lscp_get_engine_info ( lscp_client_t *pClient, const char *pszEngineName )
{
    lscp_engine_info_t *pEngineInfo = NULL;
    char szQuery[LSCP_BUFSIZ];
    const char *pszResult;
    const char *pszSeps = ":";
    const char *pszCrlf = "\r\n";
    char *pszToken;
    char *pch;

    if (pszEngineName == NULL)
        return NULL;

    snprintf(szQuery, sizeof(szQuery), "GET ENGINE INFO %s\r\n", pszEngineName);
    if (lscp_client_query(pClient, szQuery) == LSCP_OK) {
        pszResult = lscp_client_get_result(pClient);
        pEngineInfo = &(pClient->engine_info);
        _lscp_engine_info_reset(pEngineInfo);
        pszToken = strtok_r((char *) pszResult, pszSeps, &(pch));
        while (pszToken) {
            if (strcmp(pszToken, "DESCRIPTION") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pEngineInfo->description = strdup(_lscp_ltrim(pszToken));
            }
            else if (strcmp(pszToken, "VERSION") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pEngineInfo->version = strdup(_lscp_ltrim(pszToken));
            }
            pszToken = strtok_r(NULL, pszSeps, &(pch));
        }
    }

    return pEngineInfo;
}


/**
 *  Getting sampler channel informations:
 *  GET CHANNEL INFO <sampler-channel>
 */
lscp_channel_info_t *lscp_get_channel_info ( lscp_client_t *pClient, int iSamplerChannel )
{
    lscp_channel_info_t *pChannelInfo = NULL;
    char szQuery[LSCP_BUFSIZ];
    const char *pszResult;
    const char *pszSeps = ":";
    const char *pszCrlf = "\r\n";
    char *pszToken;
    char *pch;

    if (iSamplerChannel < 0)
        return NULL;
        
    snprintf(szQuery, sizeof(szQuery), "GET CHANNEL INFO %d\r\n", iSamplerChannel);
    if (lscp_client_query(pClient, szQuery) == LSCP_OK) {
        pszResult = lscp_client_get_result(pClient);
        pChannelInfo = &(pClient->channel_info);
        _lscp_channel_info_reset(pChannelInfo);
        pszToken = strtok_r((char *) pszResult, pszSeps, &(pch));
        while (pszToken) {
            if (strcmp(pszToken, "ENGINE_NAME") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pChannelInfo->engine_name = strdup(_lscp_ltrim(pszToken));
            }
            else if (strcmp(pszToken, "AUDIO_OUTPUT_TYPE") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken) {
                    pszToken = (char *) _lscp_ltrim(pszToken);
                    if (strcmp(pszToken, "ALSA") == 0)
                        pChannelInfo->audio_type = LSCP_AUDIO_ALSA;
                    else if (strcmp(pszToken, "JACK") == 0)
                        pChannelInfo->audio_type = LSCP_AUDIO_JACK;
                }
            }
            else if (strcmp(pszToken, "AUDIO_OUTPUT_CHANNEL") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pChannelInfo->audio_channel = atoi(_lscp_ltrim(pszToken));
            }
            else if (strcmp(pszToken, "INSTRUMENT") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pChannelInfo->instrument = strdup(_lscp_ltrim(pszToken));
            }
            else if (strcmp(pszToken, "MIDI_INPUT_TYPE") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken) {
                    pszToken = (char *) _lscp_ltrim(pszToken);
                    if (strcmp(pszToken, "ALSA") == 0)
                        pChannelInfo->midi_type = LSCP_MIDI_ALSA;
                }
            }
            else if (strcmp(pszToken, "MIDI_INPUT_PORT") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pChannelInfo->midi_port = strdup(_lscp_ltrim(pszToken));
            }
            else if (strcmp(pszToken, "MIDI_INPUT_CHANNEL") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pChannelInfo->midi_channel = atoi(_lscp_ltrim(pszToken));
            }
            else if (strcmp(pszToken, "VOLUME") == 0) {
                pszToken = strtok_r(NULL, pszCrlf, &(pch));
                if (pszToken)
                    pChannelInfo->volume = atof(_lscp_ltrim(pszToken));
            }
            pszToken = strtok_r(NULL, pszSeps, &(pch));
        }
    }

    return pChannelInfo;
}


/**
 *  Current number of active voices:
 *  GET CHANNEL VOICE_COUNT <sampler-channel>
 */
int lscp_get_channel_voice_count ( lscp_client_t *pClient, int iSamplerChannel )
{
    char szQuery[LSCP_BUFSIZ];
    int iVoiceCount = -1;

    if (iSamplerChannel < 0)
        return iVoiceCount;

    snprintf(szQuery, sizeof(szQuery), "GET CHANNEL VOICE_COUNT %d\r\n", iSamplerChannel);
    if (lscp_client_query(pClient, szQuery) == LSCP_OK)
        iVoiceCount = atoi(lscp_client_get_result(pClient));

    return iVoiceCount;
}


/**
 *  Current number of active disk streams:
 *  GET CHANNEL STREAM_COUNT <sampler-channel>
 */
int lscp_get_channel_stream_count ( lscp_client_t *pClient, int iSamplerChannel )
{
    char szQuery[LSCP_BUFSIZ];
    int iStreamCount = -1;

    if (iSamplerChannel < 0)
        return iStreamCount;

    snprintf(szQuery, sizeof(szQuery), "GET CHANNEL STREAM_COUNT %d\r\n", iSamplerChannel);
    if (lscp_client_query(pClient, szQuery) == LSCP_OK)
        iStreamCount = atoi(lscp_client_get_result(pClient));

    return iStreamCount;
}


/**
 *  Current fill state of disk stream buffers:
 *  GET CHANNEL BUFFER_FILL {BYTES|PERCENTAGE} <sampler-channel>
 */
lscp_buffer_fill_t *lscp_get_channel_buffer_fill ( lscp_client_t *pClient, lscp_usage_t usage_type, int iSamplerChannel )
{
    lscp_buffer_fill_t *pBufferFill = NULL;
    char szQuery[LSCP_BUFSIZ];
    int iStreamCount = pClient->iStreamCount;
    const char *pszUsageType = (usage_type == LSCP_USAGE_BYTES ? "BYTES" : "PERCENTAGE");
    const char *pszResult;
    const char *pszSeps = "[]%,";
    char *pszToken;
    char *pch;
    int   iStream;

    if (iSamplerChannel < 0)
        return NULL;
    if (iStreamCount < 1)
        iStreamCount = lscp_get_channel_stream_count(pClient, iSamplerChannel);
    if (iStreamCount < 1)
        return NULL;

    if (pClient->iStreamCount != iStreamCount) {
        if (pClient->buffer_fill)
            free(pClient->buffer_fill);
        pClient->iStreamCount = 0;
        pClient->buffer_fill = (lscp_buffer_fill_t *) malloc(iStreamCount * sizeof(lscp_buffer_fill_t));
        pClient->iStreamCount = iStreamCount;
    }

    snprintf(szQuery, sizeof(szQuery), "GET CHANNEL BUFFER_FILL %s %d\r\n", pszUsageType, iSamplerChannel);
    if (lscp_client_query(pClient, szQuery) == LSCP_OK) {
        pszResult = lscp_client_get_result(pClient);
        pBufferFill = pClient->buffer_fill;
        pszToken = strtok_r((char *) pszResult, pszSeps, &(pch));
        iStream = 0;
        while (pszToken && iStream < pClient->iStreamCount) {
            if (*pszToken) {
                pBufferFill[iStream].stream_id = atol(pszToken);
                pszToken = strtok_r(NULL, pszSeps, &(pch));
                if (pszToken == NULL)
                    break;
                pBufferFill[iStream].stream_usage = atol(pszToken);
                iStream++;
            }
            pszToken = strtok_r(NULL, pszSeps, &(pch));
        }
    }

    return pBufferFill;
}


/**
 *  Setting audio output type:
 *  SET CHANNEL AUDIO_OUTPUT_TYPE <sampler-channel> <audio-output-type>
 */
lscp_status_t lscp_set_channel_audio_type ( lscp_client_t *pClient, int iSamplerChannel, lscp_audio_t iAudioType )
{
    char szQuery[LSCP_BUFSIZ];
    const char *pszAudioType;

    if (iSamplerChannel < 0)
        return LSCP_FAILED;

    switch (iAudioType) {
    case LSCP_AUDIO_ALSA:
        pszAudioType = "ALSA";
        break;
    case LSCP_AUDIO_JACK:
        pszAudioType = "JACK";
        break;
    default:
        return LSCP_FAILED;
    }

    snprintf(szQuery, sizeof(szQuery), "SET CHANNEL AUDIO_OUTPUT_TYPE %d %s\r\n", iSamplerChannel, pszAudioType);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Setting audio output channel:
 *  SET CHANNEL AUDIO_OUTPUT_CHANNEL <sampler-channel> <audio-channel>
 */
lscp_status_t lscp_set_channel_audio_channel ( lscp_client_t *pClient, int iSamplerChannel, int iAudioChannel )
{
    char szQuery[LSCP_BUFSIZ];

    if (iSamplerChannel < 0 || iAudioChannel < 0)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "SET CHANNEL AUDIO_OUTPUT_CHANNEL %d %d\r\n", iSamplerChannel, iAudioChannel);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Setting MIDI input type:
 *  SET CHANNEL MIDI_INPUT_TYPE <sampler-channel> <midi-input-type>
 */
lscp_status_t lscp_set_channel_midi_type ( lscp_client_t *pClient, int iSamplerChannel, lscp_midi_t iMidiType )
{
    char szQuery[LSCP_BUFSIZ];
    const char *pszMidiType;

    if (iSamplerChannel < 0)
        return LSCP_FAILED;

    switch (iMidiType) {
    case LSCP_MIDI_ALSA:
        pszMidiType = "ALSA";
        break;
    default:
        return LSCP_FAILED;
    }
    snprintf(szQuery, sizeof(szQuery), "SET CHANNEL MIDI_INPUT_TYPE %d %s\r\n", iSamplerChannel, pszMidiType);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Setting MIDI input port:
 *  SET CHANNEL MIDI_INPUT_PORT <sampler-channel> <midi-input-port>
 */
lscp_status_t lscp_set_channel_midi_port ( lscp_client_t *pClient, int iSamplerChannel, const char *pszMidiPort )
{
    char szQuery[LSCP_BUFSIZ];

    if (iSamplerChannel < 0 || pszMidiPort == NULL)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "SET CHANNEL MIDI_INPUT_PORT %d %s\r\n", iSamplerChannel, pszMidiPort);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Setting MIDI input channel:
 *  SET CHANNEL MIDI_INPUT_CHANNEL <sampler-channel> <midi-input-chan>
 */
lscp_status_t lscp_set_channel_midi_channel ( lscp_client_t *pClient, int iSamplerChannel, int iMidiChannel )
{
    char szQuery[LSCP_BUFSIZ];

    if (iSamplerChannel < 0 || iMidiChannel < 1 || iMidiChannel > 16)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "SET CHANNEL MIDI_INPUT_CHANNEL %d %d\r\n", iSamplerChannel, iMidiChannel);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Setting channel volume:
 *  SET CHANNEL VOLUME <sampler-channel> <volume>
 */
lscp_status_t lscp_set_channel_volume ( lscp_client_t *pClient, int iSamplerChannel, float fVolume )
{
    char szQuery[LSCP_BUFSIZ];

    if (iSamplerChannel < 0 || fVolume < 0.0)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "SET CHANNEL VOLUME %d %g\r\n", iSamplerChannel, fVolume);
    return lscp_client_query(pClient, szQuery);
}


/**
 *  Resetting a sampler channel:
 *  RESET CHANNEL <sampler-channel>
 */
lscp_status_t lscp_reset_channel ( lscp_client_t *pClient, int iSamplerChannel )
{
    char szQuery[LSCP_BUFSIZ];

    if (iSamplerChannel < 0)
        return LSCP_FAILED;

    snprintf(szQuery, sizeof(szQuery), "RESET CHANNEL %d\r\n", iSamplerChannel);
    return lscp_client_query(pClient, szQuery);
}


// end of socket.c