/* server-tls-callback.c
 *
 * Copyright (C) 2006-2018 wolfSSL Inc.
 *
 * This file is part of wolfSSL. (formerly known as CyaSSL)
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */
/* the usual suspects */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* socket includes */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

/* wolfSSL */
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/certs_test.h>

/* ESP specific */
#include "wifi_connect.h"

#ifdef WOLFSSL_TRACK_MEMORY
    #include <wolfssl/wolfcrypt/mem_track.h>
#endif

const char *TAG = "tls_server";

void tls_smp_server_task()
{
    int                sockfd;
    int                connd;
    struct sockaddr_in servAddr;
    struct sockaddr_in clientAddr;
    socklen_t          size = sizeof(clientAddr);
    char               buff[256];
    size_t             len;
    int                shutdown = 0;
    int                ret;

    /* declare wolfSSL objects */
    WOLFSSL_CTX* ctx;
    WOLFSSL*     ssl;

    WOLFSSL_ENTER("tls_smp_server_task");

#ifdef DEBUG_WOLFSSL
    WOLFSSL_MSG("Debug ON");
    wolfSSL_Debugging_ON();
#endif
    /* Initialize wolfSSL */
    WOLFSSL_MSG("Start wolfSSL_Init()");
    wolfSSL_Init();

    /* Create a socket that uses an internet IPv4 address,
     * Sets the socket to be stream based (TCP),
     * 0 means choose the default protocol. */
    WOLFSSL_MSG( "start socket())");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR: failed to create the socket");
    }

    /* Create and initialize WOLFSSL_CTX */
    WOLFSSL_MSG("Create and initialize WOLFSSL_CTX");
    if ((ctx = wolfSSL_CTX_new(wolfSSLv23_server_method())) == NULL) {
        printf("ERROR: failed to create WOLFSSL_CTX");
    }
    WOLFSSL_MSG("Loading certificate...");
    /* Load server certificates into WOLFSSL_CTX */
    if ((ret = wolfSSL_CTX_use_certificate_buffer(ctx, server_cert_der_2048,
                        sizeof_server_cert_der_2048,
                        WOLFSSL_FILETYPE_ASN1)) != SSL_SUCCESS) {
        printf("ERROR: failed to load cert");
    }
    WOLFSSL_MSG("Loading key info...");
    /* Load server key into WOLFSSL_CTX */
    if((ret=wolfSSL_CTX_use_PrivateKey_buffer(ctx,
                            server_key_der_2048, sizeof_server_key_der_2048,
                            WOLFSSL_FILETYPE_ASN1)) != SSL_SUCCESS) {
        printf("ERROR: failed to load privatekey");
    }

    /* Initialize the server address struct with zeros */
    memset(&servAddr, 0, sizeof(servAddr));
    /* Fill in the server address */
    servAddr.sin_family      = AF_INET;             /* using IPv4      */
    servAddr.sin_port        = htons(DEFAULT_PORT); /* on DEFAULT_PORT */
    servAddr.sin_addr.s_addr = INADDR_ANY;          /* from anywhere   */

    /* Bind the server socket to our port */
    if (bind(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr)) == -1) {
         printf("ERROR: failed to bind");
    }

    /* Listen for a new connection, allow 5 pending connections */
    if (listen(sockfd, 5) == -1) {
         printf("ERROR: failed to listen");
    }
    /* Continue to accept clients until shutdown is issued */
    while (!shutdown) {
         WOLFSSL_MSG("Waiting for a connection...");
        /* Accept client connections */
        if ((connd = accept(sockfd, (struct sockaddr*)&clientAddr, &size))
            == -1) {
             printf("ERROR: failed to accept the connection");
        }
        /* Create a WOLFSSL object */
        if ((ssl = wolfSSL_new(ctx)) == NULL) {
             printf("ERROR: failed to create WOLFSSL object");
        }
        /* Attach wolfSSL to the socket */
        wolfSSL_set_fd(ssl, connd);
        /* Establish TLS connection */
        ret = wolfSSL_accept(ssl);
        if (ret != SSL_SUCCESS) {
            printf("wolfSSL_accept error %d", wolfSSL_get_error(ssl, ret));
        }
        WOLFSSL_MSG("Client connected successfully");
        /* Read the client data into our buff array */
        memset(buff, 0, sizeof(buff));
        if (wolfSSL_read(ssl, buff, sizeof(buff)-1) == -1) {
            printf("ERROR: failed to read");
        }
        /* Print to stdout any data the client sends */
        WOLFSSL_MSG("Client sends:");
        WOLFSSL_MSG(buff);
        /* Check for server shutdown command */
        if (strncmp(buff, "shutdown", 8) == 0) {
            WOLFSSL_MSG("Shutdown command issued!");
            shutdown = 1;
        }
        /* Write our reply into buff */
        memset(buff, 0, sizeof(buff));
        memcpy(buff, "I hear ya fa shizzle!", sizeof(buff));
        len = strnlen(buff, sizeof(buff));
        /* Reply back to the client */
        if (wolfSSL_write(ssl, buff, len) != len) {
            printf("ERROR: failed to write");
        }
        /* Cleanup after this connection */
        wolfSSL_free(ssl);      /* Free the wolfSSL object              */
        close(connd);           /* Close the connection to the client   */
    }
    /* Cleanup and return */
    wolfSSL_CTX_free(ctx);  /* Free the wolfSSL context object          */
    wolfSSL_Cleanup();      /* Cleanup the wolfSSL environment          */
    close(sockfd);          /* Close the socket listening for clients   */

    vTaskDelete(NULL);

    return;                 /* Return reporting a success               */
}